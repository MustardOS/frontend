#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "tester.h"
#include "calibration.h"
#include "../devices/device.h"
#include "uinput.h"

#define TESTER_MAX_BUTTONS  64
#define TESTER_MAX_AXES     16
#define TESTER_MAX_SWITCHES 16

struct tester_button {
    unsigned short code;
    int value;
    int seen;
};

struct tester_axis {
    unsigned short code;
    int value;
    int minimum;
    int maximum;
    int seen;
};

struct tester_switch {
    unsigned short code;
    int value;
    int seen;
};

struct input_tester {
    const struct device_backend *backend;
    struct gamepad *gamepad;
    struct axis_state *calibration[4];
    struct tester_button buttons[TESTER_MAX_BUTTONS];
    struct tester_axis axes[TESTER_MAX_AXES];
    struct tester_switch switches[TESTER_MAX_SWITCHES];
    size_t button_count;
    size_t axis_count;
    size_t switch_count;
    unsigned int rumble_strength;
    struct termios original_terminal;
    int original_stdin_flags;
    int terminal_configured;
    int quit;
    int dirty;
    uint64_t last_render_ms;
    pid_t rumble_pid;
    char notice[96];
    char rumble_status[96];
};

static uint64_t monotonic_milliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return 0;
    }
    return (uint64_t) now.tv_sec * 1000u + (uint64_t) now.tv_nsec / 1000000u;
}

static const char *key_name(const unsigned short code) {
    switch (code) {
        case BTN_SOUTH:
            return "SOUTH";
        case BTN_EAST:
            return "EAST";
        case BTN_NORTH:
            return "NORTH";
        case BTN_WEST:
            return "WEST";
        case BTN_TL:
            return "L1";
        case BTN_TR:
            return "R1";
        case BTN_TL2:
            return "L2";
        case BTN_TR2:
            return "R2";
        case BTN_SELECT:
            return "SELECT";
        case BTN_START:
            return "START";
        case BTN_MODE:
            return "MENU";
        case BTN_THUMBL:
            return "L3";
        case BTN_THUMBR:
            return "R3";
        case BTN_0:
            return "BTN_0";
        case BTN_1:
            return "BTN_1";
        case KEY_VOLUMEDOWN:
            return "VOLUME-";
        case KEY_VOLUMEUP:
            return "VOLUME+";
        case KEY_F1:
            return "F1";
        case KEY_F2:
            return "F2";
        case KEY_HOMEPAGE:
            return "HOME";
        default:
            return "KEY";
    }
}

static const char *axis_name(const unsigned short code) {
    switch (code) {
        case ABS_X:
            return "LX";
        case ABS_Y:
            return "LY";
        case ABS_Z:
            return "RX";
        case ABS_RX:
            return "RX";
        case ABS_RY:
            return "RY";
        case ABS_RZ:
            return "RY";
        case ABS_HAT0X:
            return "HAT-X";
        case ABS_HAT0Y:
            return "HAT-Y";
        default:
            return "ABS";
    }
}

static const char *switch_name(const unsigned short code) {
    return code == SW_TABLET_MODE ? "TABLET" : "SWITCH";
}

static void observe_event(const struct input_event *event, void *context) {
    struct input_tester *tester = context;
    if (!tester || !event) {
        return;
    }

    if (event->type == EV_KEY) {
        for (size_t i = 0; i < tester->button_count; ++i) {
            if (tester->buttons[i].code == event->code) {
                tester->buttons[i].value = event->value;
                tester->buttons[i].seen |= event->value != 0;
                tester->dirty = 1;
                return;
            }
        }
    } else if (event->type == EV_ABS) {
        for (size_t i = 0; i < tester->axis_count; ++i) {
            if (tester->axes[i].code == event->code) {
                struct tester_axis *axis = &tester->axes[i];
                axis->value = event->value;
                if (!axis->seen) {
                    axis->minimum = axis->maximum = event->value;
                } else {
                    if (event->value < axis->minimum) axis->minimum = event->value;
                    if (event->value > axis->maximum) axis->maximum = event->value;
                }
                axis->seen = 1;
                tester->dirty = 1;
                return;
            }
        }
    } else if (event->type == EV_SW) {
        for (size_t i = 0; i < tester->switch_count; ++i) {
            if (tester->switches[i].code == event->code) {
                tester->switches[i].value = event->value;
                tester->switches[i].seen = 1;
                tester->dirty = 1;
                return;
            }
        }
    }
}

static int configure_terminal(struct input_tester *tester) {
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &tester->original_terminal) < 0) {
        fprintf(stderr, "--test requires an interactive terminal\n");
        return -1;
    }

    struct termios terminal = tester->original_terminal;
    terminal.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    terminal.c_cc[VMIN] = 0;
    terminal.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal) < 0) {
        perror("tcsetattr");
        return -1;
    }

    tester->original_stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (tester->original_stdin_flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, tester->original_stdin_flags | O_NONBLOCK);
    }
    tester->terminal_configured = 1;
    return 0;
}

static void reset_coverage(struct input_tester *tester) {
    for (size_t i = 0; i < tester->button_count; ++i) {
        tester->buttons[i].seen = 0;
    }
    for (size_t i = 0; i < tester->axis_count; ++i) {
        tester->axes[i].seen = 0;
        tester->axes[i].minimum = tester->axes[i].maximum = tester->axes[i].value;
    }
    for (size_t i = 0; i < tester->switch_count; ++i) {
        tester->switches[i].seen = 0;
    }
    snprintf(tester->notice, sizeof(tester->notice), "coverage reset");
    tester->dirty = 1;
}

static int run_rumble_child(struct gamepad *gamepad, const char *event_path) {
    close(gamepad_get_fd(gamepad));
    const int event_fd = open(event_path, O_RDWR);
    if (event_fd < 0) {
        return 2;
    }

    struct ff_effect effect = {0};
    effect.type = FF_RUMBLE;
    effect.id = -1;
    effect.u.rumble.strong_magnitude = 0xffff;
    effect.u.rumble.weak_magnitude = 0x8000;
    effect.replay.length = 750;
    if (ioctl(event_fd, EVIOCSFF, &effect) < 0) {
        close(event_fd);
        return 3;
    }

    struct input_event play = {.type = EV_FF, .code = (unsigned short) effect.id, .value = 1};
    if (write(event_fd, &play, sizeof(play)) != (ssize_t) sizeof(play)) {
        ioctl(event_fd, EVIOCRMFF, effect.id);
        close(event_fd);
        return 4;
    }

    struct timespec duration = {.tv_sec = 0, .tv_nsec = 750000000L};
    while (nanosleep(&duration, &duration) < 0 && errno == EINTR) {
    }
    play.value = 0;
    if (write(event_fd, &play, sizeof(play)) != (ssize_t) sizeof(play)) {
        ioctl(event_fd, EVIOCRMFF, effect.id);
        close(event_fd);
        return 5;
    }
    ioctl(event_fd, EVIOCRMFF, effect.id);
    close(event_fd);
    return 0;
}

static void start_rumble_test(struct input_tester *tester) {
    const struct gamepad_desc *desc = tester->backend->gamepad;
    if (!desc->enable_ff_rumble || desc->ff_effects_max == 0) {
        snprintf(tester->rumble_status, sizeof(tester->rumble_status), "unsupported by this backend");
        tester->dirty = 1;
        return;
    }
    if (tester->rumble_pid > 0) {
        snprintf(tester->rumble_status, sizeof(tester->rumble_status), "already running");
        tester->dirty = 1;
        return;
    }

    char event_path[128] = {0};
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (gamepad_get_event_path(tester->gamepad, event_path, sizeof(event_path)) == 0) {
            break;
        }
        usleep(25000);
    }
    if (!event_path[0]) {
        snprintf(tester->rumble_status, sizeof(tester->rumble_status), "failed: evdev node not found");
        tester->dirty = 1;
        return;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        snprintf(tester->rumble_status, sizeof(tester->rumble_status), "failed: fork: %s", strerror(errno));
    } else if (pid == 0) {
        _exit(run_rumble_child(tester->gamepad, event_path));
    } else {
        tester->rumble_pid = pid;
        snprintf(
            tester->rumble_status, sizeof(tester->rumble_status), "running for 750 ms at %u%%", tester->rumble_strength
        );
    }
    tester->dirty = 1;
}

static void poll_rumble_test(struct input_tester *tester) {
    if (tester->rumble_pid <= 0) {
        return;
    }
    int status = 0;
    const pid_t result = waitpid(tester->rumble_pid, &status, WNOHANG);
    if (result == 0) {
        return;
    }
    tester->rumble_pid = 0;
    if (result > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        snprintf(
            tester->rumble_status, sizeof(tester->rumble_status), "completed at %u%% (confirm motor response)",
            tester->rumble_strength
        );
    } else {
        const int code = result > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        snprintf(tester->rumble_status, sizeof(tester->rumble_status), "failed (code %d)", code);
    }
    tester->dirty = 1;
}

static void reset_calibration(struct input_tester *tester) {
    if (!tester->backend->has_analogue_calibration) {
        snprintf(
            tester->notice, sizeof(tester->notice), "backend uses kernel-calibrated axes; no userspace reset needed"
        );
        tester->dirty = 1;
        return;
    }
    for (size_t i = 0; i < 4; ++i) {
        cal_initialise(tester->calibration[i]);
    }
    snprintf(tester->notice, sizeof(tester->notice), "calibration restarted; release all sticks");
    tester->dirty = 1;
}

struct input_tester *input_tester_create(
    const struct device_backend *backend, struct gamepad *gamepad, const unsigned int rumble_strength,
    struct axis_state *lx, struct axis_state *ly, struct axis_state *rx, struct axis_state *ry
) {
    if (!backend || !backend->gamepad || !gamepad) {
        return NULL;
    }

    struct input_tester *tester = calloc(1, sizeof(*tester));
    if (!tester) {
        return NULL;
    }
    tester->backend = backend;
    tester->gamepad = gamepad;
    tester->rumble_strength = rumble_strength;
    tester->calibration[0] = lx;
    tester->calibration[1] = ly;
    tester->calibration[2] = rx;
    tester->calibration[3] = ry;
    tester->button_count =
        backend->gamepad->key_count < TESTER_MAX_BUTTONS ? backend->gamepad->key_count : TESTER_MAX_BUTTONS;
    tester->axis_count =
        backend->gamepad->axis_count < TESTER_MAX_AXES ? backend->gamepad->axis_count : TESTER_MAX_AXES;
    tester->switch_count =
        backend->gamepad->switch_count < TESTER_MAX_SWITCHES ? backend->gamepad->switch_count : TESTER_MAX_SWITCHES;

    for (size_t i = 0; i < tester->button_count; ++i)
        tester->buttons[i].code = backend->gamepad->keys[i];
    for (size_t i = 0; i < tester->axis_count; ++i)
        tester->axes[i].code = backend->gamepad->axes[i].code;
    for (size_t i = 0; i < tester->switch_count; ++i)
        tester->switches[i].code = backend->gamepad->switches[i];

    if (configure_terminal(tester) < 0) {
        free(tester);
        return NULL;
    }
    reset_coverage(tester);
    snprintf(
        tester->notice, sizeof(tester->notice), "%s",
        backend->has_analogue_calibration ? "ready; release sticks during startup calibration"
                                          : "ready; kernel input source is grabbed"
    );
    snprintf(tester->rumble_status, sizeof(tester->rumble_status), "not run");
    gamepad_set_event_observer(gamepad, observe_event, tester);
    return tester;
}

int input_tester_poll(struct input_tester *tester) {
    if (!tester) {
        return 0;
    }
    poll_rumble_test(tester);

    char input[32];
    ssize_t count;
    while ((count = read(STDIN_FILENO, input, sizeof(input))) > 0) {
        for (ssize_t i = 0; i < count; ++i) {
            switch (input[i]) {
                case 'q':
                case 'Q':
                    tester->quit = 1;
                    break;
                case 'r':
                case 'R':
                    start_rumble_test(tester);
                    break;
                case 'c':
                case 'C':
                    reset_calibration(tester);
                    break;
                case 'x':
                case 'X':
                    reset_coverage(tester);
                    break;
                default:
                    break;
            }
        }
    }
    return !tester->quit;
}

void input_tester_render(struct input_tester *tester, const int force) {
    if (!tester) return;
    const uint64_t now = monotonic_milliseconds();
    if (!force && !tester->dirty && now - tester->last_render_ms < 200u) return;
    if (!force && now - tester->last_render_ms < 200u) return;
    tester->last_render_ms = now;
    tester->dirty = 0;

    const struct input_id *id = &tester->backend->gamepad->id;
    printf("\033[H\033[2Jmuinput hardware test — %s (%s)\n", tester->backend->name, tester->backend->id);
    printf(
        "Name: %s   ID: %04x:%04x:%04x:%04x\n", tester->backend->gamepad->name, id->bustype, id->vendor, id->product,
        id->version
    );
    printf("Commands: [R] rumble  [C] recalibrate  [X] reset coverage  [Q] quit\n");
    printf("Configured rumble strength: %u%%\n\n", tester->rumble_strength);

    size_t seen_buttons = 0;
    printf("Buttons (%zu):\n", tester->button_count);
    for (size_t i = 0; i < tester->button_count; ++i) {
        const struct tester_button *button = &tester->buttons[i];
        seen_buttons += button->seen ? 1u : 0u;
        printf(
            " %c %-8s %3u  %s%s", button->value ? '>' : ' ', key_name(button->code), button->code,
            button->seen ? "seen" : "----", (i + 1) % 4 == 0 ? "\n" : "     "
        );
    }
    if (tester->button_count % 4) printf("\n");
    printf("Coverage: %zu/%zu buttons pressed\n\n", seen_buttons, tester->button_count);

    printf("Axes:\n");
    for (size_t i = 0; i < tester->axis_count; ++i) {
        const struct tester_axis *axis = &tester->axes[i];
        printf(
            " %-5s code=%-3u now=%7d  observed=[%7d, %7d] %s\n", axis_name(axis->code), axis->code, axis->value,
            axis->minimum, axis->maximum, axis->seen ? "seen" : "----"
        );
    }

    if (tester->backend->has_analogue_calibration) {
        printf("\nCalibration (raw ADC):\n");
        static const char *cal_names[] = {"LX", "LY", "RX", "RY"};
        for (size_t i = 0; i < 4; ++i) {
            const struct axis_state *axis = tester->calibration[i];
            printf(
                " %-2s %-6s raw=%7.1f centre=%7.1f span=-%6.1f/+%6.1f dz=%5.1f boot=%d/31 reject=%d\n", cal_names[i],
                cal_ready(axis) ? "READY" : "WARMUP", axis->filt, axis->centre, axis->neg_span, axis->pos_span,
                axis->deadzone, axis->boot_count, axis->boot_rejections
            );
        }
    }

    if (tester->switch_count) {
        printf("\nSwitches:\n");
        for (size_t i = 0; i < tester->switch_count; ++i) {
            const struct tester_switch *item = &tester->switches[i];
            printf(
                " %-8s code=%u value=%d %s\n", switch_name(item->code), item->code, item->value,
                item->seen ? "seen" : "----"
            );
        }
    }
    printf("\nStatus: %s\n", tester->notice);
    printf("Rumble: %s\n", tester->rumble_status);
    fflush(stdout);
}

void input_tester_destroy(struct input_tester *tester) {
    if (!tester) return;
    gamepad_set_event_observer(tester->gamepad, NULL, NULL);
    if (tester->rumble_pid > 0) {
        kill(tester->rumble_pid, SIGTERM);
        waitpid(tester->rumble_pid, NULL, 0);
    }
    if (tester->terminal_configured) {
        tcsetattr(STDIN_FILENO, TCSANOW, &tester->original_terminal);
        if (tester->original_stdin_flags >= 0) {
            fcntl(STDIN_FILENO, F_SETFL, tester->original_stdin_flags);
        }
    }

    size_t seen_buttons = 0;
    for (size_t i = 0; i < tester->button_count; ++i)
        seen_buttons += tester->buttons[i].seen ? 1u : 0u;
    printf("\033[0m\033[2J\033[Hmuinput test summary: %zu/%zu buttons pressed\n", seen_buttons, tester->button_count);
    for (size_t i = 0; i < tester->axis_count; ++i) {
        printf(
            "  %-5s observed [%d, %d]\n", axis_name(tester->axes[i].code), tester->axes[i].minimum,
            tester->axes[i].maximum
        );
    }
    printf("  rumble: %s\n", tester->rumble_status);
    free(tester);
}
