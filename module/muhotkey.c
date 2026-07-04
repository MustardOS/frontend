#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "../common/init.h"
#include "../common/fileio.h"
#include "../common/log.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/board.h"
#include "../common/theme.h"
#include "../common/strutil.h"
#include "../common/exec.h"
#include "../common/util.h"
#include "../common/json/json.h"

#define SEQ_BUF_SIZE 32
#define MAX_SEQUENCE 16

#define SAFE_BIT(i)  ((uint64_t) 1 << ((i) & 63))
#define SEQUENCE_WIN (400 + 150 * seq_buf.count)

static struct pollfd vol_pfd = {.fd = -1, .events = POLLIN};

static struct pollfd pwr_pfd = {.fd = -1, .events = POLLIN};

static struct pollfd lid_pfd = {.fd = -1, .events = POLLIN};

static void handle_combo(int num, mux_input_action action);

static void handle_input(mux_input_type type, mux_input_action action);

static void handle_idle(void);

typedef enum {
    idle_inhibit_none = 0,  // No idle inhibit.
    idle_inhibit_both = 1,  // Inhibit idle sleep and display.
    idle_inhibit_sleep = 2, // Inhibit idle sleep only.
} idle_inhibit_state;

typedef struct {
    int idle;
    uint32_t tick;
    const char *idle_name;
    const char *active_name;
} idle_timer;

typedef struct {
    char *name;
    int handle_hold;
    int handle_double_press;
    int handle_double_hold;
    int is_sequence;
    int is_handheld_mode;
    int is_normal_mode;

    uint64_t type_mask;
    uint64_t negate_mask;

    int sequence_inputs[MAX_SEQUENCE];
    int sequence_length;
    uint32_t max_interval;

    char *exec_cmd;
    char **exec_argv;
    size_t exec_argc;
} combo_config;

static struct {
    int inputs[SEQ_BUF_SIZE];
    uint32_t time[SEQ_BUF_SIZE];
    int count;
} seq_buf = {0};

static const char *input_name[mux_input_count] = {
    // Gamepad buttons:
    [mux_input_a] = "A",
    [mux_input_b] = "B",
    [mux_input_c] = "C",
    [mux_input_x] = "X",
    [mux_input_y] = "Y",
    [mux_input_z] = "Z",
    [mux_input_l1] = "L1",
    [mux_input_l2] = "L2",
    [mux_input_l3] = "L3",
    [mux_input_r1] = "R1",
    [mux_input_r2] = "R2",
    [mux_input_r3] = "R3",
    [mux_input_select] = "SELECT",
    [mux_input_start] = "START",
    [mux_input_switch] = "SWITCH",

    // D-pad:
    [mux_input_dpad_up] = "DPAD_UP",
    [mux_input_dpad_down] = "DPAD_DOWN",
    [mux_input_dpad_left] = "DPAD_LEFT",
    [mux_input_dpad_right] = "DPAD_RIGHT",

    // Left stick:
    [mux_input_ls_up] = "LS_UP",
    [mux_input_ls_down] = "LS_DOWN",
    [mux_input_ls_left] = "LS_LEFT",
    [mux_input_ls_right] = "LS_RIGHT",

    // Right stick:
    [mux_input_rs_up] = "RS_UP",
    [mux_input_rs_down] = "RS_DOWN",
    [mux_input_rs_left] = "RS_LEFT",
    [mux_input_rs_right] = "RS_RIGHT",

    // Volume buttons:
    [mux_input_vol_up] = "VOL_UP",
    [mux_input_vol_down] = "VOL_DOWN",

    // Function buttons:
    [mux_input_menu] = "MENU",

    // System buttons:
    [mux_input_power_long] = "POWER_LONG",
    [mux_input_power_short] = "POWER_SHORT",

    // Lid (Hall Switch):
    [mux_input_lid_open] = "LID_OPEN",
    [mux_input_lid_close] = "LID_CLOSE",
};

static const char *action_name[] = {
    [mux_input_press] = "PRESS",
    [mux_input_hold] = "HOLD",
    [mux_input_release] = "RELEASE",
};

static mux_input_options input_opts = {
    .max_idle_ms = IDLE_MS,
    .input_handler = handle_input,
    .combo_handler = handle_combo,
    .idle_handler = handle_idle,
};

static combo_config combo[MUX_INPUT_COMBO_COUNT] = {};

static int combo_count = 0;
static int last_combo_index = 0;
static int verbose = 0;
static uint32_t global_tick = 0;

static int vol_fd = -1;
static int raw_vol_up_pressed = 0;
static int raw_vol_down_pressed = 0;

static uint32_t raw_vol_up_next_repeat = 0;
static uint32_t raw_vol_down_next_repeat = 0;

#define POWER_LONG_MS 400

static int pwr_fd = -1;
static int raw_power_pressed = 0;
static int raw_power_long_active = 0;
static uint32_t raw_power_press_tick = 0;

#define RAW_REPEAT_INITIAL_MS  180
#define RAW_REPEAT_INTERVAL_MS 70

static int lid_fd = -1;

static idle_timer idle_display = {.idle_name = "IDLE_DISPLAY", .active_name = "IDLE_ACTIVE"};
static idle_timer idle_sleep = {.idle_name = "IDLE_SLEEP"};

static char *boot_governor = NULL;
static char *running_governor = NULL;
static char *previous_governor = NULL;

static volatile sig_atomic_t pending_signal = 0;

static int in_idle_state(void) {
    return idle_state_exists;
}

static void del_old_proc(const int signo) {
    (void) signo;

    for (;;) {
        int status;
        const pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) break;
    }
}

static void do_cleanup(const int signo) {
    (void) signo;

    static int cleanup_done = 0;
    if (cleanup_done) return;
    cleanup_done = 1;

    const char *restore_governor = running_governor ? running_governor : boot_governor;
    if (restore_governor) set_scaling_governor(restore_governor, 0);

    for (int i = 0; i < combo_count; ++i) {
        free(combo[i].name);
        free(combo[i].exec_cmd);
        free_array(combo[i].exec_argv, combo[i].exec_argc);
    }

    if (vol_fd >= 0) {
        close(vol_fd);
        vol_fd = -1;
        vol_pfd.fd = -1;
    }

    if (pwr_fd >= 0) {
        close(pwr_fd);
        pwr_fd = -1;
        pwr_pfd.fd = -1;
    }

    if (lid_fd >= 0) {
        close(lid_fd);
        lid_fd = -1;
        lid_pfd.fd = -1;
    }

    raw_vol_up_pressed = 0;
    raw_vol_down_pressed = 0;
    raw_vol_up_next_repeat = 0;
    raw_vol_down_next_repeat = 0;

    raw_power_pressed = 0;
    raw_power_long_active = 0;

    free(boot_governor);
    free(running_governor);
    free(previous_governor);
}

static void cleanup(const int signo) {
    pending_signal = signo;
}

static void record_sequence(const mux_input_type type) {
    if (seq_buf.count >= SEQ_BUF_SIZE) {
        memmove(seq_buf.inputs, seq_buf.inputs + 1, (SEQ_BUF_SIZE - 1) * sizeof(int));
        memmove(seq_buf.time, seq_buf.time + 1, (SEQ_BUF_SIZE - 1) * sizeof(uint32_t));
        seq_buf.count = SEQ_BUF_SIZE - 1;
    }

    seq_buf.inputs[seq_buf.count] = type;
    seq_buf.time[seq_buf.count] = global_tick;
    seq_buf.count++;

    // Trim old entries with new sequence input
    int new_count = 0;
    for (int i = 0; i < seq_buf.count; ++i) {
        if (global_tick - seq_buf.time[i] <= SEQUENCE_WIN) {
            seq_buf.inputs[new_count] = seq_buf.inputs[i];
            seq_buf.time[new_count++] = seq_buf.time[i];
        }
    }
    seq_buf.count = new_count;

    // Decay if idle too long since last input
    if (seq_buf.count > 0 && global_tick - seq_buf.time[seq_buf.count - 1] > SEQUENCE_WIN) {
        seq_buf.count = 0;
    }

    if (verbose && seq_buf.count > 0) {
        printf("SEQ: ");
        for (int i = 0; i < seq_buf.count; ++i) {
            printf("%s%s", input_name[seq_buf.inputs[i]], i < seq_buf.count - 1 ? "," : "");
        }
        printf("\n");
    }
}

static void run_command(combo_config *c) {
    if (!c || !c->exec_argv || c->exec_argc == 0) return;

    if (c->is_handheld_mode && config.boot.device_mode != 0) {
        if (verbose) LOG_INFO("input", "Skipped %s (restricted by mode)", c->name);
        return;
    }

    if (c->is_normal_mode && config.boot.factory_reset != 0) {
        if (verbose) LOG_INFO("input", "Skipped %s (restricted by mode)", c->name);
        return;
    }

    const char *argv[c->exec_argc + 1];
    for (size_t i = 0; i <= c->exec_argc; i++)
        argv[i] = c->exec_argv[i];
    run_exec(argv, c->exec_argc + 1, 1, 0, NULL, NULL);
}

// Finds the first non-sequence combo matching `type_bit` whose negate_mask isn't currently held.
// Shared by the simplified 'one raw event w/ one combo' dispatchers below.
static combo_config *find_matching_combo(const uint64_t type_bit, const int exact_match) {
    for (int i = 0; i < combo_count; ++i) {
        combo_config *c = &combo[i];

        if (c->is_sequence) continue;
        if (exact_match ? c->type_mask != type_bit : !(c->type_mask & type_bit)) continue;
        if (c->negate_mask && mux_input_pressed_any(c->negate_mask)) continue;

        return c;
    }

    return NULL;
}

static void run_raw_power_short_release(void) {
    combo_config *c = find_matching_combo(SAFE_BIT(mux_input_power_short), 1);
    if (!c) return;

    printf("%s\n", c->name);
    run_command(c);
}

static void run_raw_lid_action(const mux_input_type type) {
    combo_config *c = find_matching_combo(SAFE_BIT(type), 1);
    if (!c) return;

    printf("%s\n", c->name);
    run_command(c);
}

static void handle_raw_lid(void) {
    if (lid_pfd.fd < 0 || poll(&lid_pfd, 1, 0) <= 0) return;

    struct input_event ev;

    for (;;) {
        const ssize_t r = read(lid_pfd.fd, &ev, sizeof(ev));

        if (r != sizeof(ev)) break;
        if (ev.type != EV_KEY) continue;

        global_tick = mux_input_tick();

        if (ev.code == KEY_INSERT && ev.value == 0) {
            handle_input(mux_input_lid_close, mux_input_press);
            handle_input(mux_input_lid_close, mux_input_release);
            run_raw_lid_action(mux_input_lid_close);

        } else if (ev.code == KEY_DELETE && ev.value == 1) {
            handle_input(mux_input_lid_open, mux_input_press);
            handle_input(mux_input_lid_open, mux_input_release);
            run_raw_lid_action(mux_input_lid_open);
        }
    }
}

static void run_raw_power_long_action(void) {
    combo_config *c = find_matching_combo(SAFE_BIT(mux_input_power_long), 0);
    if (!c) return;

    printf("%s\n", c->name);
    run_command(c);
    raw_power_long_active = 1;
}

static void run_raw_volume_action(const mux_input_type type, const mux_input_action action) {
    handle_input(type, action);

    const uint64_t vol_bit = SAFE_BIT(type);
    int brightness_triggered = 0;

    if (mux_input_pressed(mux_input_menu)) {
        for (int i = 0; i < combo_count; ++i) {
            combo_config *c = &combo[i];

            if (c->is_sequence) continue;
            if (!(c->type_mask & vol_bit)) continue;
            if (!(c->type_mask & SAFE_BIT(mux_input_menu))) continue;
            if (c->negate_mask && mux_input_pressed_any(c->negate_mask)) continue;

            if (action == mux_input_press || (action == mux_input_hold && c->handle_hold)) {
                printf("%s\n", c->name);
                run_command(c);
                brightness_triggered = 1;
                break;
            }
        }
    }

    if (brightness_triggered) return;

    for (int i = 0; i < combo_count; ++i) {
        combo_config *c = &combo[i];

        if (c->is_sequence) continue;
        if (c->type_mask != vol_bit) continue;
        if (c->negate_mask && mux_input_pressed_any(c->negate_mask)) continue;

        if (action == mux_input_press || (action == mux_input_hold && c->handle_hold)) {
            printf("%s\n", c->name);
            run_command(c);
            break;
        }
    }
}

static void handle_raw_power(void) {
    if (pwr_pfd.fd < 0 || poll(&pwr_pfd, 1, 0) <= 0) return;

    struct input_event ev;

    for (;;) {
        const ssize_t r = read(pwr_pfd.fd, &ev, sizeof(ev));

        if (r != sizeof(ev)) break;
        if (ev.type != EV_KEY) continue;
        if (ev.code != KEY_POWER) continue;

        global_tick = mux_input_tick();

        if (ev.value == 1) {
            raw_power_pressed = 1;
            raw_power_long_active = 0;
            raw_power_press_tick = global_tick;

            handle_input(mux_input_power_short, mux_input_press);
        } else if (ev.value == 0) {
            if (raw_power_long_active) handle_input(mux_input_power_long, mux_input_release);

            handle_input(mux_input_power_short, mux_input_release);
            if (!raw_power_long_active) run_raw_power_short_release();

            raw_power_pressed = 0;
            raw_power_long_active = 0;
            raw_power_press_tick = 0;
        }
    }
}

static void handle_raw_volume(void) {
    if (vol_pfd.fd < 0 || poll(&vol_pfd, 1, 0) <= 0) return;
    struct input_event ev;

    for (;;) {
        const ssize_t r = read(vol_pfd.fd, &ev, sizeof(ev));

        if (r != sizeof(ev)) break;
        if (ev.type != EV_KEY) continue;

        mux_input_type type;
        int *pressed;
        uint32_t *next_repeat;

        if (ev.code == KEY_VOLUMEUP) {
            type = mux_input_vol_up;
            pressed = &raw_vol_up_pressed;
            next_repeat = &raw_vol_up_next_repeat;
        } else if (ev.code == KEY_VOLUMEDOWN) {
            type = mux_input_vol_down;
            pressed = &raw_vol_down_pressed;
            next_repeat = &raw_vol_down_next_repeat;
        } else {
            continue;
        }

        global_tick = mux_input_tick();

        if (ev.value == 1) {
            *pressed = 1;
            *next_repeat = global_tick + RAW_REPEAT_INITIAL_MS;
            run_raw_volume_action(type, mux_input_press);
        } else if (ev.value == 2) {
            *pressed = 1;
            *next_repeat = global_tick + RAW_REPEAT_INTERVAL_MS;
            run_raw_volume_action(type, mux_input_hold);
        } else {
            *pressed = 0;
            *next_repeat = 0;
            run_raw_volume_action(type, mux_input_release);
        }
    }
}

static int open_raw_event_index(const int idx, struct pollfd *pfd, const char *label) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/dev/input/event%d", idx);

    const int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        LOG_WARN("input", "Cannot open %s", path);
        return -1;
    }

    char name[128] = {0};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) snprintf(name, sizeof(name), "%s", "unknown");

    LOG_DEBUG("input", "Raw %s device: %s (%s)", label, path, name);

    pfd->fd = fd;
    pfd->events = POLLIN;

    return fd;
}

static void check_idle(idle_timer *timer, const uint32_t timeout_ms) {
    const uint32_t idle_ms = global_tick - timer->tick;

    if (idle_ms >= timeout_ms && !timer->idle) {
        if (verbose) LOG_INFO("input", "Device is now IDLE");
        if (timer->idle_name) printf("%s\n", timer->idle_name);

        if (!running_governor) {
            running_governor = read_all_char_from(device.cpu.governor);
            if (running_governor) write_text_to_file(WAKE_CPU_GOV, "w", CHAR, running_governor);
        }

        if (!previous_governor) previous_governor = read_all_char_from(device.cpu.governor);
        set_scaling_governor(config.settings.power.gov.idle, 0);

        write_text_to_file(IDLE_STATE, "w", INT, 1);
        timer->idle = 1;
    } else if (idle_ms < timeout_ms && timer->idle) {
        if (verbose) LOG_INFO("input", "Device is now ACTIVE");
        if (timer->active_name) printf("%s\n", timer->active_name);

        if (running_governor && previous_governor) {
            set_scaling_governor(previous_governor, 0);
            free(running_governor);
            running_governor = NULL;
            free(previous_governor);
            previous_governor = NULL;
        }

        if (file_exist(IDLE_STATE)) remove(IDLE_STATE);

        timer->idle = 0;
        timer->tick = global_tick;
    }
}

static void handle_input(const mux_input_type type, const mux_input_action action) {
    global_tick = mux_input_tick();

    // Ignore the stupid TrimUI switch while device is in idle state.
    // Spent the last fucking hour trying to debug and it was this
    // stupid little cunt interrupting the idle timer...
    if (type == mux_input_switch && in_idle_state()) return;

    if (verbose) printf("[%s %s]\n", input_name[type], action_name[action]);

    if (action == mux_input_press) {
        record_sequence(type);

        for (int i = 0; i < combo_count; ++i) {
            combo_config *c = &combo[i];
            if (!c->is_sequence) continue;
            if (c->sequence_length > seq_buf.count) continue;

            int match = 1;
            uint32_t prev_time = 0;

            for (int j = 0; j < c->sequence_length; ++j) {
                const int idx = seq_buf.count - c->sequence_length + j;

                if (seq_buf.inputs[idx] != c->sequence_inputs[j]) {
                    match = 0;
                    break;
                }

                if (c->max_interval && j > 0) {
                    const uint32_t delta = seq_buf.time[idx] - prev_time;
                    if (delta > c->max_interval) {
                        match = 0;
                        break;
                    }
                }
                prev_time = seq_buf.time[idx];
            }

            if (match) {
                printf("%s\n", c->name);
                seq_buf.count = 0; // reset after successful sequence match
                run_command(c);
                break;
            }
        }
    }

    idle_display.tick = global_tick;
    idle_sleep.tick = global_tick;
}

static void handle_idle(void) {
    if (pending_signal) {
        do_cleanup(pending_signal);
        mux_input_stop();
        return;
    }

    global_tick = mux_input_tick();

    if (vol_fd >= 0) handle_raw_volume();
    if (pwr_fd >= 0) handle_raw_power();
    if (lid_fd >= 0) handle_raw_lid();

    if (raw_vol_up_pressed && raw_vol_up_next_repeat && global_tick >= raw_vol_up_next_repeat) {
        raw_vol_up_next_repeat = global_tick + RAW_REPEAT_INTERVAL_MS;
        run_raw_volume_action(mux_input_vol_up, mux_input_hold);
    }

    if (raw_vol_down_pressed && raw_vol_down_next_repeat && global_tick >= raw_vol_down_next_repeat) {
        raw_vol_down_next_repeat = global_tick + RAW_REPEAT_INTERVAL_MS;
        run_raw_volume_action(mux_input_vol_down, mux_input_hold);
    }

    if (raw_power_pressed && !raw_power_long_active) {
        if (global_tick - raw_power_press_tick >= POWER_LONG_MS) {
            raw_power_long_active = 1;
            handle_input(mux_input_power_long, mux_input_press);
            run_raw_power_long_action();
        }
    }

    if (idle_display.tick != global_tick) {
        // Allow the shell scripts to temporarily inhibit idle detection. (We could check those
        // conditions here, but it's more flexible to leave that externally controllable.)
        switch (read_line_int_from(CONF_CONFIG_PATH "system/idle_inhibit", 1)) {
            case idle_inhibit_both:
                idle_display.tick = global_tick;
                // fallthrough
            case idle_inhibit_sleep:
                idle_sleep.tick = global_tick;
                break;
            default:
                break;
        }
    }

    const uint32_t disp_timeout = config.settings.power.idle.display * 1000;
    const uint32_t sleep_timeout = config.settings.power.idle.sleep * 1000;

    if (disp_timeout) check_idle(&idle_display, disp_timeout);
    if (sleep_timeout) check_idle(&idle_sleep, sleep_timeout);
}

static void handle_combo(const int num, const mux_input_action action) {
    combo_config *c = &combo[num];
    const uint64_t mask = input_opts.combo[num].type_mask;

    if (!mask || (c->negate_mask && mux_input_pressed_any(c->negate_mask))) return;

    if (mask == SAFE_BIT(mux_input_power_short)) {
        if (action == mux_input_release && !mux_input_pressed(mux_input_power_long)) {
            printf("%s\n", c->name);
            run_command(c);
        }
        return;
    }

    if (mask == SAFE_BIT(mux_input_menu)) {
        if (action == mux_input_release && !mux_input_pressed(mux_input_menu)) {
            printf("%s\n", c->name);
            run_command(c);
        }
        return;
    }

    if (!mux_input_pressed_any(mask)) return;

    if (action == mux_input_press || (action == mux_input_hold && c->handle_hold)) {
        printf("%s\n", c->name);
        run_command(c);
    }
}

// Parses input type specified by name. Returns mux_input_cOUNT if name is not valid.
static int parse_type(const struct json input) {
    for (int i = 0; i < mux_input_count; ++i) {
        if (input_name[i] && !json_string_compare(input, input_name[i])) return i;
    }

    return mux_input_count;
}

static void parse_inputs_array(const struct json array, combo_config *c) {
    for (struct json input = json_first(array); json_exists(input); input = json_next(input)) {
        const int type = parse_type(input);
        if (type == mux_input_count) {
            LOG_ERROR("input", "JSON Error: Invalid input name");
            exit(1);
        }
        c->type_mask |= SAFE_BIT(type);
    }
}

static void parse_negate_array(const struct json array, combo_config *c) {
    for (struct json input = json_first(array); json_exists(input); input = json_next(input)) {
        const int type = parse_type(input);
        if (type == mux_input_count) {
            LOG_ERROR("input", "JSON Error: Invalid negate input name");
            exit(1);
        }
        c->negate_mask |= SAFE_BIT(type);
    }
}

static void parse_sequence_array(const struct json array, combo_config *c) {
    int count = 0;

    for (struct json input = json_first(array); json_exists(input); input = json_next(input)) {
        if (count >= MAX_SEQUENCE) {
            LOG_ERROR("input", "Input sequence '%s' too long (max %d", c->name, MAX_SEQUENCE);
            exit(1);
        }

        const int type = parse_type(input);
        if (type == mux_input_count) {
            LOG_ERROR("input", "JSON Error: Invalid input name in sequence");
            exit(1);
        }

        c->sequence_inputs[count++] = type;
    }

    c->sequence_length = count;
}

static int combo_name_exists(const char *name) {
    for (int i = 0; i < combo_count; ++i) {
        if (strcasecmp(combo[i].name, name) == 0) return 1;
    }

    return 0;
}

// Orders combos from longest to shortest (by number of keys), then alphabetically.
//
// This ensures that when overlapping combos like VOL_UP and VOL_UP+MENU are specified, the
// longer combo (e.g., VOL_UP+MENU) is checked first. (If the shorter combo were checked first,
// it would always match when VOL_UP was pressed, and the longer combo would never trigger.)
static int cmp_combo(const void *p1, const void *p2) {
    const combo_config *c1 = p1, *c2 = p2;

    const int o1 = __builtin_popcountll(c1->type_mask);
    const int o2 = __builtin_popcountll(c2->type_mask);

    return o1 != o2 ? o2 - o1 : strcmp(c1->name, c2->name);
}

static void print_combo_config(const combo_config *c) {
    char inputs[MAX_BUFFER_SIZE];
    inputs[0] = '\0';

    if (c->is_sequence) {
        for (int i = 0; i < c->sequence_length; ++i) {
            strlcat(inputs, input_name[c->sequence_inputs[i]], sizeof(inputs));
            if (i < c->sequence_length - 1) strlcat(inputs, ",", sizeof(inputs));
        }
    } else {
        int first = 1;
        for (int i = 0; i < mux_input_count; ++i) {
            if (c->type_mask & SAFE_BIT(i)) {
                if (!first) strlcat(inputs, ",", sizeof(inputs));
                strlcat(inputs, input_name[i], sizeof(inputs));
                first = 0;
            }
        }
    }

    LOG_INFO("input", "\t%s = %s", c->name, inputs);
    if (c->is_sequence)
        LOG_INFO("input", "\thandle = sequence (%d)%s", c->sequence_length, c->max_interval ? " (timed)" : "");

    if (c->handle_hold) {
        LOG_INFO("input", "\thandle = hold");
    } else if (c->handle_double_press) {
        LOG_INFO("input", "\thandle = double press");
    } else if (c->handle_double_hold) {
        LOG_INFO("input", "\thandle = double hold");
    }

    if (c->exec_cmd) LOG_INFO("input", "\texec = '%s'", c->exec_cmd);
}

static void parse_combos_file(const char *filename) {
    char *json_str = read_all_char_from(filename);
    if (!json_str) {
        LOG_ERROR("input", "Cannot read %s", filename);
        return;
    }

    struct json root = json_parse(json_str);
    root = json_ensure(root);

    if (!json_exists(root) || json_type(root) != JSON_OBJECT) {
        LOG_ERROR("input", "JSON Error: %s is not a valid JSON file", filename);
        free(json_str);
        return;
    }

    struct json devices = json_object_get(root, "devices");
    if (json_exists(devices) && json_type(devices) == JSON_ARRAY) {
        int match = 0;

        for (struct json j = json_first(devices); json_exists(j); j = json_next(j)) {
            if (!json_string_compare(j, device.board.name)) {
                match = 1;
                break;
            }
        }

        if (!match) {
            if (verbose) LOG_INFO("input", "Skipping File: '%s' not listed as compatible board", device.board.name);
            free(json_str);
            return;
        }
    }

    for (struct json key = json_first(root); json_exists(key);) {
        struct json value = json_next(key);

        // Check compatible devices in the hotkey files first and skip if they
        // aren't in the list, omitting this key means that it will work for
        // all devices.  Especially useful for SWITCH on the TrimUI devices.
        if (json_string_compare(key, "devices") == 0) {
            key = json_next(value);
            continue;
        }

        if (combo_count == MUX_INPUT_COMBO_COUNT) {
            LOG_ERROR("input", "Maximum combo count of %d exceeded", MUX_INPUT_COMBO_COUNT);
            break;
        }

        size_t len = json_string_length(key) + 1;
        char *name = mux_malloc(len);
        json_string_copy(key, name, len);

        if (combo_name_exists(name)) {
            if (verbose) LOG_WARN("input", "Duplicate combo name '%s'", name);
            free(name);
            key = json_next(value);
            continue;
        }

        if (json_type(value) != JSON_OBJECT) {
            if (verbose) LOG_WARN("input", "%s: '%s' value must be object", filename, name);
            free(name);
            key = json_next(value);
            continue;
        }

        combo_config *c = &combo[combo_count++];
        c->name = name;

        struct json hold_json = json_object_get(value, "handle_hold");
        struct json dp_json = json_object_get(value, "handle_double_press");
        struct json dh_json = json_object_get(value, "handle_double_hold");
        struct json seq_json = json_object_get(value, "is_sequence");
        struct json interval = json_object_get(value, "max_interval");
        struct json handheld = json_object_get(value, "is_handheld_mode");
        struct json normal = json_object_get(value, "is_normal_mode");
        struct json exec_json = json_object_get(value, "run_command");
        struct json inputs = json_object_get(value, "inputs");
        struct json negate = json_object_get(value, "negate");

        c->handle_hold = json_exists(hold_json) && json_bool(hold_json);
        c->handle_double_press = json_exists(dp_json) && json_bool(dp_json);
        c->handle_double_hold = json_exists(dh_json) && json_bool(dh_json);
        c->is_sequence = json_exists(seq_json) && json_bool(seq_json);
        c->is_handheld_mode = json_exists(handheld) && json_bool(handheld);
        c->is_normal_mode = json_exists(normal) && json_bool(normal);
        c->max_interval = json_exists(interval) && json_int(interval) > 0 ? (uint32_t) json_int(interval) : 0;

        if (json_exists(exec_json)) {
            size_t l = json_string_length(exec_json) + 1;
            c->exec_cmd = mux_malloc(l);
            json_string_copy(exec_json, c->exec_cmd, l);
            c->exec_argv = split_command(c->exec_cmd, &c->exec_argc);
        }

        if (!json_exists(inputs)) {
            if (verbose) LOG_WARN("input", "'%s' missing inputs array", c->name);
            key = json_next(value);
            continue;
        }

        if (c->is_sequence) {
            parse_sequence_array(inputs, c);
        } else {
            parse_inputs_array(inputs, c);
        }

        if (json_exists(negate)) {
            parse_negate_array(negate, c);
        }

        key = json_next(value);
    }

    free(json_str);
}

static void load_hotkeys(void) {
    DIR *dir = opendir(STORAGE_HOTKEY);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (ent->d_name[0] == '.') continue;

        char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".json") != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", STORAGE_HOTKEY, ent->d_name);

        if (verbose) LOG_INFO("input", "Parsing Hotkey File: %s", path);
        parse_combos_file(path);

        if (verbose) {
            for (int i = last_combo_index; i < combo_count; ++i) {
                print_combo_config(&combo[i]);
            }
        }

        last_combo_index = combo_count;
    }

    closedir(dir);
}

static void usage(FILE *file) {
    fprintf(
        file, "Usage: muhotkey [-v]\n\n"
              "Monitor input for activity and hotkey combos.\n\n"
              "\t-v prints a lot of information input messages (verbose mode)\n"
              "\t-l lists valid input names to be used for combos\n"
              "\t-h displays this usage message\n\n"
              "Hotkey names are arbitrary strings. Use -l to see valid values for inputs.\n\n"
              "See 'hotkey' in share directory for example JSON hotkey files.\n"
    );
}

static void cleanup_atexit(void) {
    do_cleanup(0);
}

int main(int argc, char *argv[]) {
    atexit(cleanup_atexit);

    struct sigaction sa = {.sa_handler = cleanup};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction old_proc = {0};
    old_proc.sa_handler = del_old_proc;
    sigemptyset(&old_proc.sa_mask);
    old_proc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &old_proc, NULL);

    load_device(&device);
    load_config(&config);

    board_init(device.board.name);

    int volume_idx = board_volume_event_index();
    if (volume_idx >= 0) {
        vol_fd = open_raw_event_index(volume_idx, &vol_pfd, "volume");
        if (vol_fd < 0) LOG_WARN("input", "Volume input event%d could not be opened", volume_idx);
    }

    int power_idx = board_power_event_index();
    if (power_idx >= 0) {
        pwr_fd = open_raw_event_index(power_idx, &pwr_pfd, "power");
        if (pwr_fd < 0) LOG_WARN("input", "Power input event%d could not be opened", power_idx);
    }

    int lid_idx = board_lid_event_index();
    if (lid_idx >= 0) {
        lid_fd = open_raw_event_index(lid_idx, &lid_pfd, "lid");
        if (lid_fd < 0) LOG_WARN("input", "Lid input event%d could not be opened", lid_idx);
    }

    boot_governor = read_all_char_from(device.cpu.governor);
    if (!boot_governor) {
        LOG_WARN("input", "Could not read initial CPU governor");
        boot_governor = strdup("ondemand");
    } else {
        LOG_INFO("input", "Initial CPU governor: %s", boot_governor);
    }

    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_ERROR("input", "SDL init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);

    for (int opt; (opt = getopt(argc, argv, "vlh")) != -1;) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;

            case 'l':
                for (int i = 0; i < mux_input_count; ++i) {
                    if (input_name[i]) printf("%s\n", input_name[i]);
                }
                return 0;

            case 'h':
                usage(stdout);
                return 0;

            default:
                usage(stderr);
                return 1;
        }
    }

    load_hotkeys();
    last_combo_index = combo_count;

    qsort(combo, combo_count, sizeof(*combo), cmp_combo);

    for (int i = 0; i < combo_count; ++i)
        input_opts.combo[i].type_mask = combo[i].type_mask;

    for (int i = combo_count; i < MUX_INPUT_COMBO_COUNT; ++i)
        input_opts.combo[i].type_mask = 0;

    input_opts.combo_count = combo_count;

    if (verbose) {
        LOG_INFO("input", "====================================");
        LOG_INFO("input", "Final Sorted Combo Order");

        for (int i = 0; i < combo_count; ++i) {
            char buf[MAX_BUFFER_SIZE] = {0};
            int first = 1;

            for (int b = 0; b < mux_input_count; ++b) {
                if (input_opts.combo[i].type_mask & SAFE_BIT(b)) {
                    if (!first) strlcat(buf, ",", sizeof(buf));
                    strlcat(buf, input_name[b], sizeof(buf));
                    first = 0;
                }
            }

            LOG_INFO(
                "input", "\t%2d: %-14s\tmask=%016llx [%s]", i, combo[i].name,
                (unsigned long long) input_opts.combo[i].type_mask, buf
            );
        }

        LOG_INFO("input", "====================================");
    }

    setlinebuf(stdout);

    global_tick = mux_tick();
    idle_display.tick = idle_sleep.tick = global_tick;

    LOG_INFO("input", "Hotkey daemon ready! Monitoring input events...");

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    mux_input_task(&input_opts);

    SDL_Quit();
    do_cleanup(0);

    return 0;
}