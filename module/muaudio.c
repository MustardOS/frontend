#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <common/base/options.h>

#define DEVICE_SCRIPTS   OPT_PATH "script/device/"
#define HEADPHONE_SCRIPT DEVICE_SCRIPTS "headphone.sh"
#define USBAUDIO_SCRIPT  DEVICE_SCRIPTS "usbaudio.sh"

#define RETRY_LATER 75
#define RETRY_WAIT  76
#define USB_RETRIES 10

#define JACK_POLL_MS  500
#define USB_SETTLE_MS 300
#define USB_RETRY_MS  2000

#define PIXEL_GPIO      "86"
#define PIXEL_GPIO_PATH "/sys/class/gpio/gpio" PIXEL_GPIO "/value"
#define DEBUG_GPIO      "/sys/kernel/debug/gpio"
#define INPUT_DEVICES   "/proc/bus/input/devices"
#define SOUND_DEVICES   "/dev/snd"
#define ASOUND_CARDS    "/proc/asound/cards"
#define USB_DEVICES     "/sys/bus/usb/devices"

enum jack_source { jack_none, jack_gpio, jack_debug, jack_input };

static volatile sig_atomic_t running = 1;

static const char *const debug_boards[] = {
    "rg28xx-h",   "rg34xx-h",  "rg34xx-sp", "rg35xx-2024", "rg35xx-h",   "rg35xx-plus",
    "rg35xx-pro", "rg35xx-sp", "rg40xx-h",  "rg40xx-v",    "rgcubexx-h", "rgsp",
};

static void stop(const int signal) {
    (void) signal;
    running = 0;
}

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int run_script(const char *script, const char *a, const char *b) {
    const pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execl(script, script, a, b, (char *) NULL);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static ssize_t read_text(const char *path, char *out, const size_t size) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    size_t used = 0;
    ssize_t got;
    while (used + 1 < size && (got = read(fd, out + used, size - used - 1)) > 0)
        used += (size_t) got;

    close(fd);
    out[used] = '\0';
    return (ssize_t) used;
}

static int gpio_jack(void) {
    char value[8];
    if (read_text(PIXEL_GPIO_PATH, value, sizeof(value)) <= 0) return -1;
    return value[0] == '1';
}

static int debug_jack(void) {
    static char text[32768];
    if (read_text(DEBUG_GPIO, text, sizeof(text)) <= 0) return -1;

    char *line = strstr(text, "Headphone detection");
    if (!line) return -1;

    char *end = strchr(line, '\n');
    if (end) *end = '\0';

    size_t length = strlen(line);
    while (length && line[length - 1] == ' ')
        line[--length] = '\0';

    const char *level = strrchr(line, ' ');
    if (!level) return -1;

    return strcmp(level + 1, "hi") == 0;
}

static int open_jack_input(void) {
    static char text[32768];
    if (read_text(INPUT_DEVICES, text, sizeof(text)) <= 0) return -1;

    int jack = 0;
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        if (strncmp(line, "N:", 2) == 0) jack = strstr(line, "Audio Jack") != NULL;
        if (!jack || strncmp(line, "H:", 2) != 0) continue;

        const char *event = strstr(line, "event");
        if (!event) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%.*s", (int) strspn(event, "event0123456789"), event);
        return open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }

    return -1;
}

static int input_jack(const int fd) {
    unsigned char switches[(SW_MAX + 8) / 8] = {0};
    if (ioctl(fd, EVIOCGSW(sizeof(switches)), switches) < 0) return -1;
    return (switches[SW_HEADPHONE_INSERT / 8] >> (SW_HEADPHONE_INSERT % 8)) & 1;
}

static int usb_audio_present(void) {
    char cards[4096];
    if (read_text(ASOUND_CARDS, cards, sizeof(cards)) > 0 && strcasestr(cards, "usb")) return 1;

    DIR *dir = opendir(USB_DEVICES);
    if (!dir) return 0;

    int found = 0;
    const struct dirent *entry;
    while (!found && (entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        char value[8];
        snprintf(path, sizeof(path), USB_DEVICES "/%s/bInterfaceClass", entry->d_name);
        found = read_text(path, value, sizeof(value)) > 0 && strncmp(value, "01", 2) == 0;
    }

    closedir(dir);
    return found;
}

static enum jack_source jack_source_for(const char *board, int *input_fd) {
    if (strcmp(board, "rk-pixel-2") == 0) {
        const int fd = open("/sys/class/gpio/export", O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            if (write(fd, PIXEL_GPIO, strlen(PIXEL_GPIO)) < 0) errno = 0;
            close(fd);
        }
        return jack_gpio;
    }

    for (size_t i = 0; i < A_SIZE(debug_boards); i++) {
        if (strcmp(board, debug_boards[i]) == 0) return jack_debug;
    }

    if (strncmp(board, "tui", 3) == 0) {
        *input_fd = open_jack_input();
        return *input_fd >= 0 ? jack_input : jack_none;
    }

    return jack_none;
}

static int read_jack(const enum jack_source source, const int input_fd) {
    switch (source) {
        case jack_gpio:
            return gpio_jack();
        case jack_debug:
            return debug_jack();
        case jack_input:
            return input_jack(input_fd);
        default:
            return -1;
    }
}

static void drain(const int fd) {
    char buffer[4096];
    while (read(fd, buffer, sizeof(buffer)) > 0) {
    }
}

int main(const int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <board>\n", argv[0]);
        return 2;
    }

    signal(SIGTERM, stop);
    signal(SIGINT, stop);

    int input_fd = -1;
    const enum jack_source source = jack_source_for(argv[1], &input_fd);

    const int sound_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (sound_fd >= 0) inotify_add_watch(sound_fd, SOUND_DEVICES, IN_CREATE | IN_DELETE);

    int jack_last = -1;
    int usb_last = 0;
    int usb_checked = 0;
    int usb_switched = 0;
    int usb_retries = 0;
    int usb_pending = 0;
    long usb_due = now_ms();
    long jack_due = now_ms();

    while (running) {
        const long now = now_ms();

        if (source != jack_none && now >= jack_due) {
            const int jack = read_jack(source, input_fd);
            if (jack >= 0 && jack != jack_last) {
                run_script(HEADPHONE_SCRIPT, jack ? "1" : "0", jack_last < 0 ? "1" : "0");
                jack_last = jack;
            }
            jack_due = source == jack_input ? LONG_MAX : now + JACK_POLL_MS;
        }

        if (usb_due && now >= usb_due) {
            usb_due = 0;
            const int usb = usb_audio_present();

            if (usb && (!usb_last || usb_pending)) {
                const int result = run_script(USBAUDIO_SCRIPT, "1", usb_checked ? "0" : "1");
                usb_switched = result == 0;
                usb_pending = result == RETRY_WAIT || (result == RETRY_LATER && usb_retries < USB_RETRIES);
                usb_retries = usb_pending ? usb_retries + (result == RETRY_LATER) : 0;
                if (usb_pending) usb_due = now_ms() + USB_RETRY_MS;
            } else if (!usb && usb_last && usb_switched) {
                run_script(USBAUDIO_SCRIPT, "0", "0");
                usb_switched = 0;
            }

            if (!usb) usb_pending = usb_retries = 0;

            usb_last = usb;
            usb_checked = 1;
        }

        struct pollfd fds[2];
        nfds_t count = 0;
        if (sound_fd >= 0) fds[count++] = (struct pollfd) {.fd = sound_fd, .events = POLLIN};
        if (input_fd >= 0) fds[count++] = (struct pollfd) {.fd = input_fd, .events = POLLIN};

        long wait = LONG_MAX;
        if (source != jack_none && jack_due != LONG_MAX) wait = jack_due - now_ms();
        if (usb_due) wait = wait < usb_due - now_ms() ? wait : usb_due - now_ms();

        const int timeout = wait == LONG_MAX ? -1 : wait < 0 ? 0 : (int) wait;
        if (poll(fds, count, timeout) <= 0) continue;

        for (nfds_t i = 0; i < count; i++) {
            if (!(fds[i].revents & POLLIN)) continue;
            drain(fds[i].fd);

            if (fds[i].fd == sound_fd) {
                usb_due = now_ms() + USB_SETTLE_MS;
            } else {
                jack_due = now_ms();
            }
        }
    }

    if (input_fd >= 0) close(input_fd);
    if (sound_fd >= 0) close(sound_fd);
    return 0;
}
