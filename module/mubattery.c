#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include <common/base/options.h>
#include <common/platform/battery.h>
#include <common/storage/fileio.h>

#define CHECKPOINT_TICKS  60
#define LOW_BATTERY_TICKS 2
#define BATTERY_OVERLAY   RUN_PATH "overlay.battery"
#define IDLE_FLAG         RUN_PATH "is_idle"

typedef struct {
    const char *board;
    int prefix;
    const char *args;
} low_battery_led;

static const low_battery_led low_battery_leds[] = {
    {"rg", 1, "-b SERIAL 1 255 255 0 0 255 0 0"},
    {"tui-brick", 0, "-b SYSFS 1 10 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0"},
    {"tui-brick-pro", 0,
     "-b SYSFS 1 10 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0 255 0 0"},
    {"tui-spoon", 0, "-b SYSFS 1 10 255 0 0 255 0 0 255 0 0"},
};

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t resumed = 0;

static pid_t checkpoint_pid = 0;
static int low_battery_active = 0;

static void handle_signal(const int sig) {
    (void) sig;
    running = 0;
}

static void handle_resume(const int sig) {
    (void) sig;
    resumed = 1;
}

static void reap_checkpoint(void) {
    if (checkpoint_pid > 0 && waitpid(checkpoint_pid, NULL, WNOHANG) != 0) checkpoint_pid = 0;
}

static void checkpoint_activity(void) {
    if (checkpoint_pid > 0 || access(INFO_ACT_PATH "/.current_session", F_OK) != 0) return;

    const pid_t pid = fork();
    if (pid == 0) {
        execl(TRACK_BIN, TRACK_BIN, "checkpoint", (char *) NULL);
        _exit(127);
    }

    if (pid > 0) checkpoint_pid = pid;
}

static void read_text(const char *path, char *out, const size_t size) {
    out[0] = '\0';

    FILE *file = fopen(path, "r");
    if (!file) return;

    if (fgets(out, (int) size, file)) out[strcspn(out, "\r\n")] = '\0';
    fclose(file);
}

static void write_text(const char *path, const char *text) {
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return;

    const ssize_t written = write(fd, text, strlen(text));
    (void) written;
    close(fd);
}

static void run_rgb(const char *args) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", args);

    char *argv[48];
    int argc = 0;
    argv[argc++] = (char *) RGBLED_BIN;

    char *save = NULL;
    for (char *token = strtok_r(buffer, " ", &save); token && argc < 47; token = strtok_r(NULL, " ", &save)) {
        argv[argc++] = token;
    }
    argv[argc] = NULL;

    const pid_t pid = fork();
    if (pid == 0) {
        execv(RGBLED_BIN, argv);
        _exit(127);
    }

    if (pid > 0) waitpid(pid, NULL, 0);
}

static const char *low_battery_led_args(void) {
    char board[64];
    read_text(CONF_DEVICE_PATH "board/name", board, sizeof(board));

    for (size_t i = 0; i < A_SIZE(low_battery_leds); i++) {
        const low_battery_led *led = &low_battery_leds[i];
        const int match =
            led->prefix ? strncmp(board, led->board, strlen(led->board)) == 0 : strcmp(board, led->board) == 0;
        if (match) return led->args;
    }

    return NULL;
}

static void low_battery_rgb(void) {
    if (read_line_int_from(CONF_DEVICE_PATH "led/rgb", 1) != 1 || access(RGBLED_BIN, X_OK) != 0) return;
    if (read_line_int_from(CONF_CONFIG_PATH "settings/general/rgb", 1) != 1) return;
    if (read_line_int_from(CONF_CONFIG_PATH "settings/rgb/mode", 1) == 10) return;

    const char *args = low_battery_led_args();
    if (!args) return;

    run_rgb(args);
    if (access(IDLE_FLAG, F_OK) != 0) run_rgb("restore");
}

static void set_low_led(const char *value) {
    char path[256];
    read_text(CONF_DEVICE_PATH "led/low", path, sizeof(path));
    if (path[0]) write_text(path, value);
}

static void check_low_battery(void) {
    const int low = !battery_is_charging()
                    && battery_get_capacity() <= read_line_int_from(CONF_CONFIG_PATH "settings/power/low_battery", 1);

    if (!low) {
        if (low_battery_active) {
            remove(BATTERY_OVERLAY);
            set_low_led("0\n");
            low_battery_active = 0;
        }
        return;
    }

    if (low_battery_active) return;

    write_text(BATTERY_OVERLAY, "");
    set_low_led("1\n");
    low_battery_active = 1;
    low_battery_rgb();
}

int main(void) {
    const struct timespec ts = {5, 0};
    int ticks = 0;
    int low_ticks = LOW_BATTERY_TICKS;

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGCONT, handle_resume);

    battery_set_daemon_mode(1);
    battery_init();

    while (running) {
        if (resumed) {
            resumed = 0;
            ticks = 0;
            battery_reset();
        }

        battery_update();
        reap_checkpoint();

        if (++low_ticks >= LOW_BATTERY_TICKS) {
            low_ticks = 0;
            check_low_battery();
        }

        if (++ticks >= CHECKPOINT_TICKS) {
            ticks = 0;
            checkpoint_activity();
        }

        nanosleep(&ts, NULL);
    }

    return 0;
}
