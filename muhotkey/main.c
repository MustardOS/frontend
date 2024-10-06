#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"

static void handle_combo(int num, mux_input_action action);
static void handle_input(mux_input_type type, mux_input_action action);
static void handle_idle(void);

struct mux_device device;
struct mux_config config;

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

static bool verbose = false;

static char *combo_name[MUX_INPUT_COMBO_COUNT] = {};
static bool combo_holdable[MUX_INPUT_COMBO_COUNT] = {};

static bool handled_input = false;
static uint32_t last_input_tick;

static bool idle_display = false;
static bool idle_sleep = false;

static void idle_active(void) {
    // Reset idle timer in response to activity.
    if (idle_display || idle_sleep) {
        printf("IDLE_ACTIVE\n");
    }

    last_input_tick = mux_tick();
    idle_display = false;
    idle_sleep = false;
}

static void handle_input(mux_input_type type, mux_input_action action) {
    if (verbose) {
        printf("[%s %s]\n", input_name[type], action_name[action]);
    }
    handled_input = true;
    idle_active();
}

static void handle_idle(void) {
    // If we handled input on this iteration of the event loop, we're already in the active state
    // and don't need to read the idle_inhibit file. That helps performance since the idle handler
    // is effectively called in a tight loop for continuous input (e.g., spinning a control stick).
    if (handled_input) {
        handled_input = false;
        return;
    }

    // Allow the shell scripts to temporarily inhibit idle detection. (We could check those
    // conditions here, but it's more flexible to leave that externally controllable.)
    if (read_int_from_file("/run/muos/system/idle_inhibit")) {
        idle_active();
        return;
    }

    // Detect idle display and/or sleep timeout.
    uint32_t idle_ms = mux_tick() - last_input_tick;

    uint32_t timeout_display_ms = config.SETTINGS.POWER.IDLE_DISPLAY * 1000;
    if (timeout_display_ms && idle_ms >= timeout_display_ms && !idle_display) {
        printf("IDLE_DISPLAY\n");
        idle_display = true;
    }

    uint32_t timeout_sleep_ms = config.SETTINGS.POWER.IDLE_SLEEP * 1000;
    if (timeout_sleep_ms && idle_ms >= timeout_sleep_ms && !idle_sleep) {
        printf("IDLE_SLEEP\n");
        idle_sleep = true;
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
        if (action == MUX_INPUT_RELEASE && !mux_input_pressed(MUX_INPUT_POWER_LONG)) {
            printf("%s\n", combo_name[num]);
        }
        return;
    }

    if (action == MUX_INPUT_PRESS || (action == MUX_INPUT_HOLD && combo_holdable[num])) {
        printf("%s\n", combo_name[num]);
    }
}

// Parse an input type specified by name on the command line.
//
// Returns MUX_INPUT_COUNT if the name is not valid.
int parse_type(const char *name) {
    int i;
    for (i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (input_name[i] && strcmp(input_name[i], name) == 0) {
            break;
        }
    }
    return i;
}

// Parse command-line argument specifying a combo to listen for.
//
// Combo specs have the format `NAME=INPUT1+INPUT2+...`. For example, `OSF=POWER_SHORT+L1+L2+R1+R2`
// is a valid combo that triggers when all five of those inputs are active.
//
// Combos are allowed to overlap, in which case the first combo that matches will trigger. For
// example, consider combos `BRIGHT_UP=MENU_LONG+VOL_UP` and `VOL_UP=`VOL_UP`:
//
// * When Volume Up is pressed on its own, the VOL_UP combo will trigger.
// * If Menu is held before Volume Up is pressed, then the BRIGHT_UP combo will trigger instead.
void parse_combo(int combo_num, const char *combo_spec) {
    // Duplicate the argument so we can modify it without affecting the process command line.
    char *s = strdup(combo_spec);
    if (!s) {
        fprintf(stderr, "muhotkey: couldn't allocate memory for combo\n");
        exit(1);
    }

    // Parse NAME.
    combo_name[combo_num] = strsep(&s, "=");
    if (!s) {
        fprintf(stderr, "muhotkey: combo must specify at least one input: %s\n", combo_spec);
        exit(1);
    }

    do {
        // Parse INPUT.
        const char *input_name = strsep(&s, "+");

        // Look up input type by name.
        int type = parse_type(input_name);
        if (type == MUX_INPUT_COUNT) {
            fprintf(stderr, "muhotkey: combo specified invalid input name %s: %s\n", input_name,
                    combo_spec);
            exit(1);
        }

        // Update combo config accordingly.
        input_opts.combo[combo_num].type_mask |= BIT(type);
    } while (s);
}

void usage(FILE *file) {
    fprintf(file,
            "Usage: muhotkey [-v] [-C|-H COMBO_SPEC]...\n"
            "Usage: muhotkey -l\n"
            "\n"
            "Monitor input for a list of hotkey combos.\n"
            "\n"
            "  -C COMBO_SPEC combo that will receive events on press\n"
            "  -H COMBO_SPEC combo that will receive events on press and hold\n"
            "  -v            prints every input received (verbose mode)\n"
            "  -l            lists valid input names for COMBO_SPEC\n"
            "  -h            displays this usage message\n"
            "\n"
            "COMBO_SPEC has format `NAME=INPUT1+INPUT2+...`. Use -l to see valid INPUT values.\n"
    );
}

int main(int argc, char *argv[]) {
    load_device(&device);
    load_config(&config);

    input_opts.gamepad_fd = open(device.INPUT.EV1, O_RDONLY);
    if (input_opts.gamepad_fd < 0) {
        perror("muhotkey: couldn't open joystick input device\n");
        return 1;
    }

    input_opts.system_fd = open(device.INPUT.EV0, O_RDONLY);
    if (input_opts.system_fd < 0) {
        perror("muhotkey: couldn't open system input device");
        return 1;
    }

    int opt, next_combo = 0;
    while ((opt = getopt(argc, argv, "C:H:vlh")) != -1) {
        switch (opt) {
            case 'C':
            case 'H':
                if (next_combo >= MUX_INPUT_COMBO_COUNT) {
                    fprintf(stderr, "muhotkey: at most %d combos supported\n",
                            MUX_INPUT_COMBO_COUNT);
                    return 1;
                }

                parse_combo(next_combo, optarg);
                combo_holdable[next_combo] = (opt == 'H');
                ++next_combo;
                break;
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
    if (optind < argc) {
        usage(stderr);
        return 1;
    }

    setlinebuf(stdout);

    last_input_tick = mux_tick();
    mux_input_task(&input_opts);

    for (int i = 0; i < MUX_INPUT_COMBO_COUNT; ++i) {
        free(combo_name[i]);
    }

    close(input_opts.gamepad_fd);
    close(input_opts.system_fd);

    return 0;
}
