#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/log.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/theme.h"
#include "../common/json/json.h"

static void handle_combo(int num, mux_input_action action);

static void handle_input(mux_input_type type, mux_input_action action);

static void handle_idle(void);

typedef enum {
    IDLE_INHIBIT_NONE = 0, // No idle inhibit.
    IDLE_INHIBIT_BOTH = 1, // Inhibit idle sleep and display.
    IDLE_INHIBIT_SLEEP = 2, // Inhibit idle sleep only.
} idle_inhibit_state;

typedef struct {
    bool idle;
    uint32_t tick;
    const char *idle_name;
    const char *active_name;
} idle_timer;

typedef struct {
    char *name;
    bool handle_hold;
    uint64_t type_mask;
} combo_config;

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
static bool verbose = false;

static idle_timer idle_display = {.idle_name = "IDLE_DISPLAY", .active_name = "IDLE_ACTIVE"};
static idle_timer idle_sleep = {.idle_name = "IDLE_SLEEP"};

static char *running_governor;

static void check_idle(idle_timer *timer, uint32_t timeout_ms) {
    uint32_t idle_ms = mux_input_tick() - timer->tick;
    if (idle_ms >= timeout_ms && !timer->idle) {
        running_governor = read_all_char_from(device.CPU.GOVERNOR);
        set_scaling_governor(config.SETTINGS.POWER.GOV.IDLE);
        if (timer->idle_name) printf("%s\n", timer->idle_name);
        timer->idle = true;
    } else if (idle_ms < timeout_ms && timer->idle) {
        set_scaling_governor(running_governor);
        if (timer->active_name) printf("%s\n", timer->active_name);
        timer->idle = false;
    }
}

static void handle_input(mux_input_type type, mux_input_action action) {
    if (verbose) {
        printf("[%s %s]\n", input_name[type], action_name[action]);
    }

    idle_display.tick = mux_input_tick();
    idle_sleep.tick = mux_input_tick();
}

static void handle_idle(void) {
    // If we handled input on this iteration of the event loop, we're already in the active state
    // and don't need to read the idle_inhibit file. That helps performance since the idle handler
    // is effectively called in a tight loop for continuous input (e.g., spinning a control stick).
    if (idle_display.tick != mux_input_tick()) {
        // Allow the shell scripts to temporarily inhibit idle detection. (We could check those
        // conditions here, but it's more flexible to leave that externally controllable.)
        switch (read_line_int_from((CONF_CONFIG_PATH "system/idle_inhibit"), 1)) {
            case IDLE_INHIBIT_BOTH:
                idle_display.tick = mux_input_tick();
                // fallthrough
            case IDLE_INHIBIT_SLEEP:
                idle_sleep.tick = mux_input_tick();
                break;
        }
    }

    // Handle idle timeout/activity.
    uint32_t display_timeout_ms = config.SETTINGS.POWER.IDLE.DISPLAY * 1000;
    if (display_timeout_ms) {
        check_idle(&idle_display, display_timeout_ms);
    }

    uint32_t sleep_timeout_ms = config.SETTINGS.POWER.IDLE.SLEEP * 1000;
    if (sleep_timeout_ms) {
        check_idle(&idle_sleep, sleep_timeout_ms);
    }
}

static void handle_combo(int num, mux_input_action action) {
    if (input_opts.combo[num].type_mask == BIT(MUX_INPUT_POWER_SHORT)) {
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
        if (action == MUX_INPUT_RELEASE && !mux_input_pressed(MUX_INPUT_POWER_LONG)) printf("%s\n", combo[num].name);
        return;
    }

    if (input_opts.combo[num].type_mask == BIT(MUX_INPUT_MENU_SHORT)) {
        if (action == MUX_INPUT_RELEASE && !mux_input_pressed(MUX_INPUT_MENU_SHORT)) printf("%s\n", combo[num].name);
        return;
    }

    if (action == MUX_INPUT_PRESS || (action == MUX_INPUT_HOLD && combo[num].handle_hold)) {
        printf("%s\n", combo[num].name);
    }
}

// Parses input type specified by name. Returns MUX_INPUT_COUNT if name is not valid.
static int parse_type(struct json input) {
    int i;
    for (i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (input_name[i] && !json_string_compare(input, input_name[i])) {
            break;
        }
    }
    return i;
}

// Parses JSON file mapping hotkey names to configuration. File has the following format:
//
// {
//   "NAME1": {"handle_hold": false, "inputs": ["INPUT1", "INPUT2"]},
//   "NAME2": {"handle_hold": true, "inputs": ["INPUT3"]}
// }
static void parse_combos(const char *filename) {
    char *json_str = read_all_char_from(filename);
    if (!json_valid(json_str)) {
        fprintf(stderr, "muhotkey: JSON error: file not valid\n");
        exit(1);
    }

    struct json json = json_parse(json_str);
    if (json_type(json) != JSON_OBJECT) {
        fprintf(stderr, "muhotkey: JSON error: root must be an object\n");
        exit(1);
    }

    // Iterate over root object (NAME1, CONFIG1, NAME2, CONFIG2, ...).
    for (json = json_first(json); json_exists(json); json = json_next(json)) {
        if (combo_count == MUX_INPUT_COMBO_COUNT) {
            fprintf(stderr, "muhotkey: at most %d hotkeys supported\n", MUX_INPUT_COMBO_COUNT);
            exit(1);
        }

        combo_config *c = &combo[combo_count++];

        // Parse hotkey name (e.g., "NAME1").
        size_t name_length = json_string_length(json) + 1;
        c->name = malloc(name_length);
        json_string_copy(json, c->name, name_length);

        if (c->name[0] == '\0') {
            fprintf(stderr, "muhotkey: JSON error: hotkey name must not be empty\n");
            exit(1);
        }

        // Parse hotkey config (e.g., {"handle_hold": false, "inputs": ["INPUT1", "INPUT2"]}).
        json = json_next(json);

        if (json_type(json) != JSON_OBJECT) {
            fprintf(stderr, "muhotkey: JSON error: hotkey config must be an object\n");
            exit(1);
        }

        struct json hold_json = json_object_get(json, "handle_hold");
        c->handle_hold = json_exists(hold_json) && json_bool(hold_json);

        // Parse hotkey inputs (e.g., ["INPUT1", "INPUT2"]).
        for (struct json input = json_first(json_object_get(json, "inputs"));
             json_exists(input);
             input = json_next(input)) {
            int type = parse_type(input);
            if (type == MUX_INPUT_COUNT) {
                fprintf(stderr, "muhotkey: JSON error: invalid input name\n");
                exit(1);
            }
            c->type_mask |= BIT(type);
        }
    }

    free(json_str);
}

// Orders combos from longest to shortest (by number of keys), then alphabetically.
//
// This ensures that when overlapping combos like VOL_UP and VOL_UP+MENU_LONG are specified, the
// longer combo (e.g., VOL_UP+MENU_LONG) is checked first. (If the shorter combo were checked first,
// it would always match when VOL_UP was pressed, and the longer combo would never trigger.)
static int cmp_combo(const void *p1, const void *p2) {
    const combo_config *combo1 = (const combo_config *) p1;
    const combo_config *combo2 = (const combo_config *) p2;

    int length1 = __builtin_popcountll(combo1->type_mask);
    int length2 = __builtin_popcountll(combo2->type_mask);

    return (length1 != length2) ? length2 - length1 : strcmp(combo1->name, combo2->name);
}

// Prints a combo config for debugging. Format is similar to "[NAME=INPUT1+INPUT2]".
static void print_combo_config(const combo_config *c) {
    printf("[%s=", c->name);
    for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (c->type_mask & BIT(i)) {
            printf("%s", input_name[i]);
            if (c->type_mask >> (i + 1)) {
                printf(",");
            }
        }
    }
    printf("]\n");
}

static void usage(FILE *file) {
    fprintf(file,
            "Usage: muhotkey [-v] FILE\n"
            "Usage: muhotkey -l\n"
            "\n"
            "Monitor input for activity and hotkey combos.\n"
            "\n"
            "  -v prints every input received (verbose mode)\n"
            "  -l lists valid input names\n"
            "  -h displays this usage message\n"
            "\n"
            "FILE should specify a JSON file having the following format:\n"
            "\n"
            "{\n"
            "  \"NAME1\": {\"handle_hold\": false, \"inputs\": [\"INPUT1\", \"INPUT2\"]},\n"
            "  \"NAME2\": {\"handle_hold\": true, \"inputs\": [\"INPUT3\"]}\n"
            "}\n"
            "\n"
            "Hotkey names are arbitrary strings. Use -l to see valid values for inputs.\n"
    );
}

int main(int argc, char *argv[]) {
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
                    if (input_name[i]) {
                        printf("%s\n", input_name[i]);
                    }
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
    if (argc - optind != 1) {
        usage(stderr);
        return 1;
    }

    // Parse combos, and then sort them from longest to shortest, ensuring longer combos (e.g.,
    // VOL_UP+MENU_LONG) can still trigger when they overlap with shorter ones (e.g, VOL_UP).
    parse_combos(argv[optind]);
    qsort(combo, combo_count, sizeof(*combo), cmp_combo);
    for (int i = 0; i < combo_count; ++i) {
        input_opts.combo[i].type_mask = combo[i].type_mask;
        if (verbose) {
            print_combo_config(&combo[i]);
        }
    }

    // Flush triggered combo names to stdout immediately.
    setlinebuf(stdout);

    // Start idle timers.
    uint32_t tick = mux_tick();
    idle_display.tick = tick;
    idle_sleep.tick = tick;

    // Process input and respond to combos indefinitely.
    mux_input_task(&input_opts);

    // Clean up.
    for (int i = 0; i < combo_count; ++i) {
        free(combo[i].name);
    }

    close(input_opts.general_fd);
    close(input_opts.power_fd);
    close(input_opts.volume_fd);
    close(input_opts.extra_fd);

    return 0;
}
