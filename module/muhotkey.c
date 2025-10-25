#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/log.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/theme.h"
#include "../common/json/json.h"

#define MAX_SEQUENCE 12
#define SAFE_BIT(i) ((i) < 64 ? (1ULL << (i)) : 0ULL)
#define SEQUENCE_WIN (400 + 150 * seq_buf.count)

static void handle_combo(int num, mux_input_action action);

static void handle_input(mux_input_type type, mux_input_action action);

static void handle_idle(void);

typedef enum {
    IDLE_INHIBIT_NONE = 0, // No idle inhibit.
    IDLE_INHIBIT_BOTH = 1, // Inhibit idle sleep and display.
    IDLE_INHIBIT_SLEEP = 2, // Inhibit idle sleep only.
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
    int sequence_inputs[MAX_SEQUENCE];
    int sequence_length;
    uint32_t max_interval;
    char *exec_cmd;
} combo_config;

static struct {
    int inputs[16];
    uint32_t time[16];
    int count;
} seq_buf = {0};

static const char *input_name[MUX_INPUT_COUNT] = {
        // Gamepad buttons:
        [MUX_INPUT_A] = "A",
        [MUX_INPUT_B] = "B",
        [MUX_INPUT_C] = "C",
        [MUX_INPUT_X] = "X",
        [MUX_INPUT_Y] = "Y",
        [MUX_INPUT_Z] = "Z",
        [MUX_INPUT_L1] = "L1",
        [MUX_INPUT_L2] = "L2",
        [MUX_INPUT_L3] = "L3",
        [MUX_INPUT_R1] = "R1",
        [MUX_INPUT_R2] = "R2",
        [MUX_INPUT_R3] = "R3",
        [MUX_INPUT_SELECT] = "SELECT",
        [MUX_INPUT_START] = "START",
        [MUX_INPUT_SWITCH] = "SWITCH",

        // D-pad:
        [MUX_INPUT_DPAD_UP] = "DPAD_UP",
        [MUX_INPUT_DPAD_DOWN] = "DPAD_DOWN",
        [MUX_INPUT_DPAD_LEFT] = "DPAD_LEFT",
        [MUX_INPUT_DPAD_RIGHT] = "DPAD_RIGHT",

        // Left stick:
        [MUX_INPUT_LS_UP] = "LS_UP",
        [MUX_INPUT_LS_DOWN] = "LS_DOWN",
        [MUX_INPUT_LS_LEFT] = "LS_LEFT",
        [MUX_INPUT_LS_RIGHT] = "LS_RIGHT",

        // Right stick:
        [MUX_INPUT_RS_UP] = "RS_UP",
        [MUX_INPUT_RS_DOWN] = "RS_DOWN",
        [MUX_INPUT_RS_LEFT] = "RS_LEFT",
        [MUX_INPUT_RS_RIGHT] = "RS_RIGHT",

        // Volume buttons:
        [MUX_INPUT_VOL_UP] = "VOL_UP",
        [MUX_INPUT_VOL_DOWN] = "VOL_DOWN",

        // Function buttons:
        [MUX_INPUT_MENU_LONG] = "MENU_LONG",
        [MUX_INPUT_MENU_SHORT] = "MENU_SHORT",

        // System buttons:
        [MUX_INPUT_POWER_LONG] = "POWER_LONG",
        [MUX_INPUT_POWER_SHORT] = "POWER_SHORT",
};

static const char *action_name[] = {
        [MUX_INPUT_PRESS] = "PRESS",
        [MUX_INPUT_HOLD] = "HOLD",
        [MUX_INPUT_RELEASE] = "RELEASE",
};

static mux_input_options input_opts = {
        .max_idle_ms = 1000,
        .input_handler = handle_input,
        .combo_handler = handle_combo,
        .idle_handler = handle_idle,
};

static combo_config combo[MUX_INPUT_COMBO_COUNT] = {};
static int combo_count = 0;
static int last_combo_index = 0;
static int verbose = 0;
static uint32_t global_tick = 0;

static idle_timer idle_display = {.idle_name = "IDLE_DISPLAY", .active_name = "IDLE_ACTIVE"};
static idle_timer idle_sleep = {.idle_name = "IDLE_SLEEP"};

static char *running_governor;

static void cleanup(int signo) {
    (void) signo;
    set_scaling_governor(running_governor, 0);
    close(input_opts.general_fd);
    close(input_opts.power_fd);
    close(input_opts.volume_fd);
    close(input_opts.extra_fd);
    exit(0);
}

static void record_sequence(mux_input_type type) {
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

static void run_command(const combo_config *c) {
    if (!c || !c->exec_cmd || !*c->exec_cmd) return;

    int handheld_ok = 1;
    if (c->is_handheld_mode) handheld_ok = (config.BOOT.DEVICE_MODE == 0);

    int normal_ok = 1;
    if (c->is_normal_mode) normal_ok = (config.BOOT.FACTORY_RESET == 0);

    if (!handheld_ok || !normal_ok) {
        if (verbose) LOG_INFO("input", "Skipped %s (restricted by mode)", c->name)
        return;
    }

    size_t argc = 0;
    char **args = split_command(c->exec_cmd, &argc);
    if (!args || argc == 0) return;

    run_exec((const char **) args, argc + 1, 1, 0, NULL);

    for (size_t i = 0; i < argc; i++) free(args[i]);
    free(args);
}

static void check_idle(idle_timer *timer, uint32_t timeout_ms) {
    uint32_t idle_ms = global_tick - timer->tick;

    if (idle_ms >= timeout_ms && !timer->idle) {
        running_governor = read_all_char_from(device.CPU.GOVERNOR);
        set_scaling_governor(config.SETTINGS.POWER.GOV.IDLE, 0);
        if (verbose) LOG_INFO("input", "Device is now IDLE")
        timer->idle = true;
    } else if (idle_ms < timeout_ms && timer->idle) {
        set_scaling_governor(running_governor, 0);
        if (verbose) LOG_INFO("input", "Device is now ACTIVE")
        timer->idle = false;
    }
}

static void handle_input(mux_input_type type, mux_input_action action) {
    global_tick = mux_input_tick();
    if (verbose) printf("[%s %s]\n", input_name[type], action_name[action]);

    if (action == MUX_INPUT_PRESS) {
        record_sequence(type);

        for (int i = 0; i < combo_count; ++i) {
            combo_config *c = &combo[i];
            if (!c->is_sequence) continue;
            if (c->sequence_length > seq_buf.count) continue;

            int match = 1;
            uint32_t prev_time = 0;

            for (int j = 0; j < c->sequence_length; ++j) {
                int idx = seq_buf.count - c->sequence_length + j;

                if (seq_buf.inputs[idx] != c->sequence_inputs[j]) {
                    match = 0;
                    break;
                }

                if (c->max_interval && j > 0) {
                    uint32_t delta = seq_buf.time[idx] - prev_time;
                    if (delta > c->max_interval) {
                        match = 0;
                        break;
                    }
                }
                prev_time = seq_buf.time[idx];
            }

            if (match) {
                printf("%s\n", c->name);
                seq_buf.count = 0; // reset after successful sequence math
                run_command(c);
                break;
            }
        }
    }

    idle_display.tick = global_tick;
    idle_sleep.tick = global_tick;
}

static void handle_idle(void) {
    // If we handled input on this iteration of the event loop, we're already in the active state
    // and don't need to read the idle_inhibit file. That helps performance since the idle handler
    // is effectively called in a tight loop for continuous input (e.g., spinning a control stick).
    global_tick = mux_input_tick();

    if (idle_display.tick != global_tick) {
        // Allow the shell scripts to temporarily inhibit idle detection. (We could check those
        // conditions here, but it's more flexible to leave that externally controllable.)
        switch (read_line_int_from((CONF_CONFIG_PATH "system/idle_inhibit"), 1)) {
            case IDLE_INHIBIT_BOTH:
                idle_display.tick = global_tick;
                // fallthrough
            case IDLE_INHIBIT_SLEEP:
                idle_sleep.tick = global_tick;
                break;
        }
    }

    uint32_t disp_timeout = config.SETTINGS.POWER.IDLE.DISPLAY * 1000;
    uint32_t sleep_timeout = config.SETTINGS.POWER.IDLE.SLEEP * 1000;

    if (disp_timeout) check_idle(&idle_display, disp_timeout);
    if (sleep_timeout) check_idle(&idle_sleep, sleep_timeout);
}

static void handle_combo(int num, mux_input_action action) {
    uint64_t mask = input_opts.combo[num].type_mask;

    if (mask == SAFE_BIT(MUX_INPUT_POWER_SHORT)) {
        // The power button behaves differently from every other button (including the menu button).
        // We receive these events on a short press:
        //
        // 1. POWER_SHORT PRESS
        // 2. POWER_SHORT RELEASE
        //
        // And these on a long press:
        //
        // 1. POWER_SHORT PRESS
        // 2. POWER_SHORT RELEASE + POWER_LONG PRESS (simultaneous)
        // 3. POWER_LONG RELEASE
        //
        // To support separate hotkeys for these two cases, we delay triggering of a POWER_SHORT
        // combo till release (2) so we can tell if the short press turned into a long press or not.
        if (action == MUX_INPUT_RELEASE && !mux_input_pressed(MUX_INPUT_POWER_LONG)) {
            printf("%s\n", combo[num].name);
            run_command(&combo[num]);
        }
        return;
    }

    if (mask == SAFE_BIT(MUX_INPUT_MENU_SHORT)) {
        if (action == MUX_INPUT_RELEASE && !mux_input_pressed(MUX_INPUT_MENU_SHORT)) {
            printf("%s\n", combo[num].name);
            run_command(&combo[num]);
        }
        return;
    }

    if (action == MUX_INPUT_PRESS || (action == MUX_INPUT_HOLD && combo[num].handle_hold)) {
        printf("%s\n", combo[num].name);
        run_command(&combo[num]);
    }
}

// Parses input type specified by name. Returns MUX_INPUT_COUNT if name is not valid.
static int parse_type(struct json input) {
    for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (input_name[i] && !json_string_compare(input, input_name[i])) return i;
    }

    return MUX_INPUT_COUNT;
}

static void parse_inputs_array(struct json array, combo_config *c) {
    for (struct json input = json_first(array); json_exists(input); input = json_next(input)) {
        int type = parse_type(input);
        if (type == MUX_INPUT_COUNT) {
            LOG_ERROR("input", "JSON Error: Invalid input name")
            exit(1);
        }
        c->type_mask |= SAFE_BIT(type);
    }
}

static void parse_sequence_array(struct json array, combo_config *c) {
    int count = 0;

    for (struct json input = json_first(array); json_exists(input); input = json_next(input)) {
        if (count >= MAX_SEQUENCE) {
            LOG_ERROR("input", "Input sequence '%s' too long (max %d", c->name, MAX_SEQUENCE)
            exit(1);
        }

        int type = parse_type(input);
        if (type == MUX_INPUT_COUNT) {
            LOG_ERROR("input", "JSON Error: Invalid input name in sequence")
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
// This ensures that when overlapping combos like VOL_UP and VOL_UP+MENU_LONG are specified, the
// longer combo (e.g., VOL_UP+MENU_LONG) is checked first. (If the shorter combo were checked first,
// it would always match when VOL_UP was pressed, and the longer combo would never trigger.)
static int cmp_combo(const void *p1, const void *p2) {
    const combo_config *c1 = p1, *c2 = p2;

    int o1 = bc64(c1->type_mask);
    int o2 = bc64(c2->type_mask);

    return (o1 != o2) ? o2 - o1 : strcmp(c1->name, c2->name);
}

static void print_combo_config(const combo_config *c) {
    char inputs[MAX_BUFFER_SIZE];
    inputs[0] = '\0';

    if (c->is_sequence) {
        for (int i = 0; i < c->sequence_length; ++i) {
            strlcat(inputs, input_name[c->sequence_inputs[i]], sizeof(inputs));
            if (i < c->sequence_length - 1)
                strlcat(inputs, ",", sizeof(inputs));
        }
    } else {
        int first = 1;
        for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
            if (c->type_mask & SAFE_BIT(i)) {
                if (!first) strlcat(inputs, ",", sizeof(inputs));
                strlcat(inputs, input_name[i], sizeof(inputs));
                first = 0;
            }
        }
    }

    LOG_INFO("input", "\t%s = %s", c->name, inputs)
    if (c->is_sequence) LOG_INFO("input", "\thandle = sequence (%d)%s",
                                 c->sequence_length, c->max_interval ? " (timed)" : "")

    if (c->handle_hold) {
        LOG_INFO("input", "\thandle = hold")
    } else if (c->handle_double_press) {
        LOG_INFO("input", "\thandle = double press")
    } else if (c->handle_double_hold) {
        LOG_INFO("input", "\thandle = double hold")
    }

    if (c->exec_cmd) LOG_INFO("input", "\texec = '%s'", c->exec_cmd)
}

static void parse_combos_file(const char *filename) {
    char *json_str = read_all_char_from(filename);
    if (!json_str) {
        LOG_ERROR("input", "Cannot read %s", filename)
        return;
    }

    struct json root = json_parse(json_str);
    root = json_ensure(root);

    if (!json_exists(root) || json_type(root) != JSON_OBJECT) {
        LOG_ERROR("input", "JSON Error: %s is not a valid JSON file", filename)
        free(json_str);
        return;
    }

    struct json devices = json_object_get(root, "devices");
    if (json_exists(devices) && json_type(devices) == JSON_ARRAY) {
        int match = 0;

        for (struct json j = json_first(devices); json_exists(j); j = json_next(j)) {
            if (!json_string_compare(j, device.BOARD.NAME)) {
                match = 1;
                break;
            }
        }

        if (!match) {
            if (verbose) LOG_INFO("input", "Skipping File: '%s' not listed as compatible board", device.BOARD.NAME)
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
            LOG_ERROR("input", "Maximum combo count of %d exceeded", MUX_INPUT_COMBO_COUNT)
            break;
        }

        size_t len = json_string_length(key) + 1;
        char *name = malloc(len);
        json_string_copy(key, name, len);

        if (combo_name_exists(name)) {
            if (verbose) LOG_WARN("input", "Duplicate combo name '%s'", name)
            free(name);
            key = json_next(value);
            continue;
        }

        if (json_type(value) != JSON_OBJECT) {
            if (verbose) LOG_WARN("input", "%s: '%s' value must be object", filename, name)
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

        c->handle_hold = json_exists(hold_json) && json_bool(hold_json);
        c->handle_double_press = json_exists(dp_json) && json_bool(dp_json);
        c->handle_double_hold = json_exists(dh_json) && json_bool(dh_json);
        c->is_sequence = json_exists(seq_json) && json_bool(seq_json);
        c->is_handheld_mode = json_exists(handheld) && json_bool(handheld);
        c->is_normal_mode = json_exists(normal) && json_bool(normal);
        c->max_interval = (json_exists(interval) && json_int(interval) > 0) ? (uint32_t) json_int(interval) : 0;

        if (json_exists(exec_json)) {
            size_t l = json_string_length(exec_json) + 1;
            c->exec_cmd = malloc(l);
            json_string_copy(exec_json, c->exec_cmd, l);
        }

        if (!json_exists(inputs)) {
            if (verbose) LOG_WARN("input", "'%s' missing inputs array", c->name)
            key = json_next(value);
            continue;
        }

        if (c->is_sequence) {
            parse_sequence_array(inputs, c);
        } else {
            parse_inputs_array(inputs, c);
        }

        key = json_next(value);
    }

    free(json_str);
}

static void load_hotkeys() {
    DIR *dir = opendir(STORAGE_HOTKEY);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (ent->d_name[0] == '.') continue;
        const char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".json") != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", STORAGE_HOTKEY, ent->d_name);

        if (verbose) LOG_INFO("input", "Parsing Hotkey File: %s", path)
        parse_combos_file(path);

        for (int i = last_combo_index; i < combo_count; ++i) {
            input_opts.combo[i].type_mask = combo[i].type_mask;
            if (verbose) print_combo_config(&combo[i]);
        }

        last_combo_index = combo_count;
    }

    closedir(dir);
}

static void usage(FILE *file) {
    fprintf(file,
            "Usage: muhotkey [-v]\n\n"
            "Monitor input for activity and hotkey combos.\n\n"
            "\t-v prints a lot of information input messages (verbose mode)\n"
            "\t-l lists valid input names to be used for combos\n"
            "\t-h displays this usage message\n\n"
            "Hotkey names are arbitrary strings. Use -l to see valid values for inputs.\n\n"
            "See 'hotkey' in share directory for example JSON hotkey files.\n"
    );
}

int main(int argc, char *argv[]) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Read config and open input devices.
    load_device(&device);
    load_config(&config);

    running_governor = read_all_char_from(device.CPU.GOVERNOR);

    input_opts.general_fd = open(device.INPUT_EVENT.JOY_GENERAL, O_RDONLY);
    if (input_opts.general_fd < 0) {
        LOG_ERROR("input", "%s", lang.SYSTEM.NO_JOY_GENERAL)
        return 1;
    }

    input_opts.power_fd = open(device.INPUT_EVENT.JOY_POWER, O_RDONLY);
    if (input_opts.power_fd < 0) {
        LOG_ERROR("input", "%s", lang.SYSTEM.NO_JOY_POWER)
        return 1;
    }

    input_opts.volume_fd = open(device.INPUT_EVENT.JOY_VOLUME, O_RDONLY);
    if (input_opts.volume_fd < 0) {
        LOG_ERROR("input", "%s", lang.SYSTEM.NO_JOY_VOLUME)
        return 1;
    }

    input_opts.extra_fd = open(device.INPUT_EVENT.JOY_EXTRA, O_RDONLY);
    if (input_opts.extra_fd < 0) {
        LOG_ERROR("input", "%s", lang.SYSTEM.NO_JOY_EXTRA)
        return 1;
    }

    // Parse command line arguments.
    for (int opt; (opt = getopt(argc, argv, "vlh")) != -1;) {
        switch (opt) {
            case 'v':
                verbose = true;
                break;
            case 'l':
                for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
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

    // Parse combo files, and then sort them from longest to shortest, ensuring longer combos
    // (e.g. VOL_UP+MENU_LONG) can still trigger when they overlap with shorter ones (e.g, VOL_UP).
    load_hotkeys();

    // Sort longest to shortest so super combos get first dibs
    qsort(combo, combo_count, sizeof(*combo), cmp_combo);

    // Copy masks AGAIN to the runtime table in the **sorted** order
    for (int i = 0; i < combo_count; ++i) {
        input_opts.combo[i].type_mask = combo[i].type_mask;
    }

    // Zero out any remaining combo entries to be safe
    for (int i = combo_count; i < MUX_INPUT_COMBO_COUNT; ++i) {
        input_opts.combo[i].type_mask = 0;
    }

    if (verbose) {
        LOG_INFO("input", "====================================")
        LOG_INFO("input", "Final Sorted Combo Order")
        for (int i = 0; i < combo_count; ++i) {
            char buf[MAX_BUFFER_SIZE] = {0};
            int first = 1;
            for (int b = 0; b < MUX_INPUT_COUNT; ++b) {
                if (input_opts.combo[i].type_mask & SAFE_BIT(b)) {
                    if (!first) strlcat(buf, ",", sizeof(buf));
                    strlcat(buf, input_name[b], sizeof(buf));
                    first = 0;
                }
            }

            LOG_INFO("input", "\t%2d: %-14s\tmask=%016llx [%s]",
                     i, combo[i].name,
                     (unsigned long long) input_opts.combo[i].type_mask, buf)
        }
        LOG_INFO("input", "====================================")
    }

    // Flush triggered combo names to stdout immediately.
    setlinebuf(stdout);
    global_tick = mux_tick();
    idle_display.tick = idle_sleep.tick = global_tick;

    // Process input and respond to combos indefinitely.
    mux_input_task(&input_opts);

    cleanup(0);
    return 0;
}
