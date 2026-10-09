#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stb/stb_image_write.h>
#include <common/display/screenshot.h>

#define BOUNDARY         "muframe"
#define RESEND_MS        2000
#define PRIVATE_CHECK_MS 1000
#define DEFAULT_FPS      15
#define DEFAULT_QUALITY  70
#define DEFAULT_SECONDS  1800
#define SEND_TIMEOUT_S   10

static const char *const private_modules[] = {"muxwebcode", "muxpass", "muxnetprofile", "muxwebserv"};

typedef void *(*tj_init_fn)(void);
typedef int (*tj_compress_fn)(
    void *, const unsigned char *, int, int, int, int, unsigned char **, unsigned long *, int, int, int
);
typedef void (*tj_free_fn)(unsigned char *);

static tj_compress_fn tj_compress;
static tj_free_fn tj_free;
static void *tj_handle;

static struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} encoded;

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double) t.tv_sec * 1000.0 + (double) t.tv_nsec / 1e6;
}

static void sleep_ms(const double ms) {
    if (ms <= 0) return;
    struct timespec t = {(time_t) (ms / 1000.0), (long) ((ms - (double) (time_t) (ms / 1000.0) * 1000.0) * 1e6)};
    while (nanosleep(&t, &t) < 0 && errno == EINTR) {
    }
}

static int write_all(const void *data, size_t length) {
    const unsigned char *p = data;
    while (length) {
        const ssize_t sent = write(STDOUT_FILENO, p, length);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        p += sent;
        length -= (size_t) sent;
    }
    return 1;
}

static int send_part(const char *type, const void *data, const size_t length) {
    char header[160];
    const int written = snprintf(
        header, sizeof(header), "--" BOUNDARY "\r\nContent-Type: %s\r\nContent-Length: %zu\r\n\r\n", type, length
    );
    return written > 0 && write_all(header, (size_t) written) && write_all(data, length) && write_all("\r\n", 2);
}

static void load_turbojpeg(void) {
    void *library = dlopen("libturbojpeg.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!library) return;

    tj_init_fn init = NULL;
    *(void **) &init = dlsym(library, "tjInitCompress");
    *(void **) &tj_compress = dlsym(library, "tjCompress2");
    *(void **) &tj_free = dlsym(library, "tjFree");
    if (init && tj_compress && tj_free) tj_handle = init();
    if (!tj_handle) tj_compress = NULL;
}

static void stb_sink(void *context, void *data, const int size) {
    (void) context;
    if (size <= 0) return;
    if (encoded.length + (size_t) size > encoded.capacity) {
        const size_t capacity = (encoded.length + (size_t) size) * 2;
        unsigned char *grown = realloc(encoded.data, capacity);
        if (!grown) return;
        encoded.data = grown;
        encoded.capacity = capacity;
    }
    memcpy(encoded.data + encoded.length, data, (size_t) size);
    encoded.length += (size_t) size;
}

static int encode(const uint8_t *rgb, const uint32_t width, const uint32_t height, const int quality) {
    if (tj_compress) {
        unsigned char *jpeg = NULL;
        unsigned long size = 0;
        if (tj_compress(tj_handle, rgb, (int) width, (int) width * 3, (int) height, 0, &jpeg, &size, 2, quality, 2048)
            != 0) {
            if (jpeg) tj_free(jpeg);
            return 0;
        }
        encoded.length = 0;
        stb_sink(NULL, jpeg, (int) size);
        tj_free(jpeg);
        return encoded.length == size;
    }

    encoded.length = 0;
    return stbi_write_jpg_to_func(stb_sink, NULL, (int) width, (int) height, 3, rgb, quality) && encoded.length;
}

static void halve(uint8_t *rgb, uint32_t *width, uint32_t *height) {
    const uint32_t w = *width / 2;
    const uint32_t h = *height / 2;
    const size_t stride = (size_t) *width * 3U;

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *top = rgb + (size_t) y * 2U * stride;
        const uint8_t *bottom = top + stride;
        uint8_t *out = rgb + (size_t) y * w * 3U;
        for (uint32_t x = 0; x < w; x++) {
            for (int c = 0; c < 3; c++) {
                const size_t i = (size_t) x * 6U + (size_t) c;
                out[x * 3U + (uint32_t) c] = (uint8_t) ((top[i] + top[i + 3] + bottom[i] + bottom[i + 3] + 2) / 4);
            }
        }
    }

    *width = w;
    *height = h;
}

static uint64_t fingerprint(const uint8_t *rgb, const size_t length) {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < length; i += 61) {
        hash ^= rgb[i];
        hash *= 1099511628211ULL;
    }
    return hash ^ length;
}

static int private_screen_open(void) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    int found = 0;
    struct dirent *entry;
    while (!found && (entry = readdir(proc))) {
        if (entry->d_name[0] < '1' || entry->d_name[0] > '9') continue;

        char path[300];
        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
        FILE *file = fopen(path, "r");
        if (!file) continue;

        char name[32] = {0};
        if (fgets(name, sizeof(name), file)) {
            name[strcspn(name, "\n")] = '\0';
            for (size_t i = 0; i < sizeof(private_modules) / sizeof(private_modules[0]); i++) {
                if (strcmp(name, private_modules[i]) == 0) found = 1;
            }
        }
        fclose(file);
    }

    closedir(proc);
    return found;
}

static void print_usage(FILE *stream) {
    fprintf(
        stream, "Usage: muscreen [options]\n"
                "Writes the screen to standard output as an MJPEG multipart stream for Remote View.\n"
                "  -f, --fps N        frames per second (default 15, 1 to 30)\n"
                "  -q, --quality N    JPEG quality (default 70, 20 to 95)\n"
                "  -s, --seconds N    stop after N seconds (default 1800)\n"
                "  -a, --all          also stream private screens such as Web Dashboard Code\n"
                "  -h, --help         show this help\n"
    );
}

int main(const int argc, char **argv) {
    int fps = DEFAULT_FPS;
    int quality = DEFAULT_QUALITY;
    long seconds = DEFAULT_SECONDS;
    int show_private = 0;

    const struct option options[] = {
        {"fps", required_argument, NULL, 'f'},     {"quality", required_argument, NULL, 'q'},
        {"seconds", required_argument, NULL, 's'}, {"all", no_argument, NULL, 'a'},
        {"help", no_argument, NULL, 'h'},          {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "f:q:s:ah", options, NULL)) != -1) {
        switch (option) {
            case 'f':
                fps = atoi(optarg);
                break;
            case 'q':
                quality = atoi(optarg);
                break;
            case 's':
                seconds = atol(optarg);
                break;
            case 'a':
                show_private = 1;
                break;
            case 'h':
                print_usage(stdout);
                return 0;
            default:
                print_usage(stderr);
                return 2;
        }
    }

    if (fps < 1 || fps > 30 || quality < 20 || quality > 95 || seconds < 1) {
        print_usage(stderr);
        return 2;
    }

    signal(SIGPIPE, SIG_IGN);
    setpriority(PRIO_PROCESS, 0, 10);

    const struct timeval timeout = {SEND_TIMEOUT_S, 0};
    setsockopt(STDOUT_FILENO, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    load_turbojpeg();

    const double interval = 1000.0 / fps;
    const double stop_at = now_ms() + (double) seconds * 1000.0;
    double last_sent = 0;
    double last_private_check = -PRIVATE_CHECK_MS;
    int hidden = 0;
    uint64_t last_print = 0;

    while (now_ms() < stop_at) {
        const double started = now_ms();

        if (!show_private && started - last_private_check >= PRIVATE_CHECK_MS) {
            last_private_check = started;
            hidden = private_screen_open();
        }

        if (hidden) {
            if (started - last_sent >= RESEND_MS) {
                if (!send_part("text/plain", "hidden", 6)) return 0;
                last_sent = started;
                last_print = 0;
            }
            sleep_ms(PRIVATE_CHECK_MS / 4.0);
            continue;
        }

        uint8_t *rgb = NULL;
        uint32_t width = 0;
        uint32_t height = 0;
        if (screenshot_grab(screenshot_auto, &rgb, &width, &height) == 0) {
            if (width >= 1000 || height >= 1000) halve(rgb, &width, &height);

            const uint64_t print = fingerprint(rgb, (size_t) width * height * 3U);
            const int changed = print != last_print;
            if ((changed || started - last_sent >= RESEND_MS) && (!changed || encode(rgb, width, height, quality))) {
                if (!send_part("image/jpeg", encoded.data, encoded.length)) {
                    free(rgb);
                    return 0;
                }
                last_sent = started;
                last_print = print;
            }
            free(rgb);
        }

        sleep_ms(interval - (now_ms() - started));
    }

    return 0;
}
