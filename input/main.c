#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/calibration.h"
#include "devices/registry.h"
#include "common/uinput.h"
#include "runtime.h"
#include "common/tester.h"

#define RUMBLE_STRENGTH_PATH "/opt/muos/device/config/board/strength"

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_reload_config = 0;

struct service_options {
    const char *backend_id;
    unsigned int rumble_strength;
    int foreground;
    int verbose;
    int test_mode;
    int rumble_strength_locked;
};

static void print_usage(FILE *stream, const char *program) {
    fprintf(
        stream,
        "Usage: %s [OPTIONS]\n"
        "  -d, --device ID       override the configured board backend\n"
        "  -f, --foreground      do not daemonise\n"
        "  -v, --verbose         enable backend diagnostics\n"
        "  -t, --test            interactive buttons/axes/rumble test\n"
        "  -r, --rumble-strength PERCENT\n"
        "                        scale force feedback from 0 to 100\n"
        "  -l, --list-devices    list registered backend IDs\n"
        "  -h, --help            show this help\n",
        program
    );
}

static int parse_strength(const char *value, unsigned int *strength) {
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno || !value[0] || !end || *end || parsed > 100u) {
        fprintf(stderr, "Invalid rumble strength '%s' (expected 0-100)\n", value);
        return -1;
    }
    *strength = (unsigned int) parsed;
    return 0;
}

static int load_strength_file(unsigned int *strength) {
    FILE *file = fopen(RUMBLE_STRENGTH_PATH, "r");
    if (!file) {
        if (errno == ENOENT) return 0;
        perror(RUMBLE_STRENGTH_PATH);
        return -1;
    }

    char value[32];
    if (!fgets(value, sizeof(value), file)) {
        if (ferror(file))
            perror(RUMBLE_STRENGTH_PATH);
        else
            fprintf(stderr, "Empty rumble strength file: %s\n", RUMBLE_STRENGTH_PATH);
        fclose(file);
        return -1;
    }
    if (!strchr(value, '\n') && !feof(file)) {
        fprintf(stderr, "Invalid rumble strength in %s\n", RUMBLE_STRENGTH_PATH);
        fclose(file);
        return -1;
    }
    if (fclose(file) < 0) {
        perror(RUMBLE_STRENGTH_PATH);
        return -1;
    }

    char *start = value;
    while (isspace((unsigned char) *start))
        start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1]))
        end--;
    *end = '\0';
    return parse_strength(start, strength);
}

static int parse_options(int argc, char **argv, struct service_options *options) {
    static const struct option long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {"foreground", no_argument, NULL, 'f'},
        {"verbose", no_argument, NULL, 'v'},
        {"test", no_argument, NULL, 't'},
        {"rumble-strength", required_argument, NULL, 'r'},
        {"list-devices", no_argument, NULL, 'l'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "d:fvtr:lh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                options->backend_id = optarg;
                break;
            case 'f':
                options->foreground = 1;
                break;
            case 'v':
                options->verbose = 1;
                break;
            case 't':
                options->test_mode = 1;
                options->foreground = 1;
                break;
            case 'r':
                if (parse_strength(optarg, &options->rumble_strength) < 0) return -1;
                options->rumble_strength_locked = 1;
                break;
            case 'l':
                device_registry_print(stdout);
                return 1;
            case 'h':
                print_usage(stdout, argv[0]);
                return 1;
            default:
                print_usage(stderr, argv[0]);
                return -1;
        }
    }

    if (optind != argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        return -1;
    }
    return 0;
}

static int daemonise_process(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }
    signal(SIGHUP, SIG_IGN);
    return 0;
}

static void handle_signal(int signal_number) {
    (void) signal_number;
    g_running = 0;
}

static void handle_reload_signal(int signal_number) {
    (void) signal_number;
    g_reload_config = 1;
}

static void install_signal_handlers(void) {
    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    action.sa_handler = handle_reload_signal;
    sigaction(SIGHUP, &action, NULL);
}

static void
initialise_axes(struct axis_state *lx, struct axis_state *ly, struct axis_state *rx, struct axis_state *ry) {
    lx->debug_id = 0;
    ly->debug_id = 1;
    rx->debug_id = 3;
    ry->debug_id = 4;
    cal_initialise(lx);
    cal_initialise(ly);
    cal_initialise(rx);
    cal_initialise(ry);
}

static void add_microseconds(struct timespec *time, useconds_t interval_us) {
    time->tv_sec += (time_t) (interval_us / 1000000u);
    time->tv_nsec += (long) (interval_us % 1000000u) * 1000L;
    if (time->tv_nsec >= 1000000000L) {
        time->tv_sec++;
        time->tv_nsec -= 1000000000L;
    }
}

static int timespec_before(const struct timespec *left, const struct timespec *right) {
    return left->tv_sec < right->tv_sec || (left->tv_sec == right->tv_sec && left->tv_nsec < right->tv_nsec);
}

static void pace_poll(struct timespec *deadline, useconds_t interval_us) {
    add_microseconds(deadline, interval_us);
    int result;
    do {
        result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL);
    } while (g_running && result == EINTR);

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
        struct timespec missed = *deadline;
        add_microseconds(&missed, interval_us);
        if (timespec_before(&missed, &now)) {
            *deadline = now;
        }
    }
}

static uint64_t monotonic_milliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return 0;
    }
    return (uint64_t) now.tv_sec * 1000u + (uint64_t) now.tv_nsec / 1000000u;
}

static const struct device_backend *select_backend(const char *forced_id) {
    if (forced_id && *forced_id) {
        const struct device_backend *backend = device_registry_find(forced_id);
        if (!backend) {
            fprintf(stderr, "Unknown input backend '%s'. Available backends:\n", forced_id);
            device_registry_print(stderr);
        }
        return backend;
    }
    return device_registry_detect();
}

int main(int argc, char **argv) {
    struct service_options options = {
        .rumble_strength = 100u,
    };
    if (load_strength_file(&options.rumble_strength) < 0) {
        fprintf(stderr, "Ignoring the stored rumble strength, using %u%%\n", options.rumble_strength);
    }
    const char *environment_strength = getenv("MUOS_RUMBLE_STRENGTH");
    if (environment_strength && parse_strength(environment_strength, &options.rumble_strength) < 0) {
        return EXIT_FAILURE;
    }
    if (environment_strength) options.rumble_strength_locked = 1;
    int parse_result = parse_options(argc, argv, &options);
    if (parse_result != 0) {
        return parse_result > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const struct device_backend *backend = select_backend(options.backend_id);
    if (!backend) {
        fprintf(stderr, "No supported input backend detected\n");
        return EXIT_FAILURE;
    }
    if (options.verbose) {
        fprintf(stderr, "Using input backend: %s (%s)\n", backend->id, backend->name);
    }
    if (runtime_prepare_state_dir() < 0) {
        return EXIT_FAILURE;
    }
    if (runtime_acquire_instance_lock() < 0) {
        return EXIT_FAILURE;
    }
    if (!options.foreground && daemonise_process() < 0) {
        return EXIT_FAILURE;
    }
    install_signal_handlers();

    struct gamepad *gamepad = gamepad_initialise(backend->gamepad);
    if (!gamepad) {
        fprintf(stderr, "Failed to create uinput gamepad for %s\n", backend->name);
        return EXIT_FAILURE;
    }

    struct axis_state lx, ly, rx, ry;
    initialise_axes(&lx, &ly, &rx, &ry);
    struct input_tester *tester = NULL;
    const struct device_options device_options = {
        .backend_id = backend->id,
        .verbose = options.verbose,
        .rumble_strength = options.rumble_strength,
    };
    void *device_ctx = NULL;
    if (backend->ops.initialise(&device_ctx, gamepad, &lx, &ly, &rx, &ry, &device_options) < 0) {
        fprintf(stderr, "Failed to initialise input backend %s\n", backend->id);
        gamepad_destroy(gamepad);
        return EXIT_FAILURE;
    }
    if (options.test_mode) {
        tester = input_tester_create(backend, gamepad, options.rumble_strength, &lx, &ly, &rx, &ry);
        if (!tester) {
            backend->ops.close(device_ctx);
            gamepad_destroy(gamepad);
            return EXIT_FAILURE;
        }
    }

    uint64_t next_refresh_ms = 0;
    if (backend->ops.refresh) {
        backend->ops.refresh(device_ctx);
        next_refresh_ms = monotonic_milliseconds() + 1000u;
    }

    const useconds_t poll_interval = backend->poll_interval_us ? backend->poll_interval_us : 16000;
    struct timespec next_poll;
    if (clock_gettime(CLOCK_MONOTONIC, &next_poll) < 0) {
        next_poll = (struct timespec) {0};
    }
    input_tester_render(tester, 1);
    while (g_running && backend->ops.poll(device_ctx)) {
        if (g_reload_config) {
            g_reload_config = 0;
            if (!options.rumble_strength_locked) {
                unsigned int strength = options.rumble_strength;
                if (load_strength_file(&strength) == 0 && strength != options.rumble_strength) {
                    options.rumble_strength = strength;
                    if (backend->ops.set_rumble_strength) {
                        backend->ops.set_rumble_strength(device_ctx, strength);
                    }
                    if (options.verbose) fprintf(stderr, "Rumble strength updated to %u%%\n", strength);
                }
            }
        }
        if (tester && !input_tester_poll(tester)) {
            break;
        }
        input_tester_render(tester, 0);
        uint64_t now_ms = monotonic_milliseconds();
        if (backend->ops.refresh && now_ms >= next_refresh_ms) {
            backend->ops.refresh(device_ctx);
            next_refresh_ms = now_ms + 1000u;
        }
        pace_poll(&next_poll, poll_interval);
    }
    g_running = 0;

    input_tester_destroy(tester);
    backend->ops.close(device_ctx);
    gamepad_destroy(gamepad);
    return EXIT_SUCCESS;
}
