#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"

struct mux_device device;
struct mux_config config;

static char *combo_name[MUX_INPUT_COMBO_COUNT] = {};

static uint32_t last_input_tick;

static bool idle_display = false;
static bool idle_sleep = false;

void idle_active() {
    // Reset idle timer in response to activity.
    if (idle_display || idle_sleep) {
        printf("IDLE ACTIVE\n");
    }

    last_input_tick = mux_tick();
    idle_display = false;
    idle_sleep = false;
}

void handle_input(mux_input_type type, mux_input_action action) {
    idle_active();
}

void handle_idle() {
    // Allow the shell scripts to temporarily inhibit idle detection. (We could check those
    // conditions here, but it's more flexible to leave that externally controllable.)
    if (file_exist("/run/muos/system/idle_inhibit")) {
        idle_active();
        return;
    }

    // Detect idle display and/or sleep timeout.
    uint32_t idle_ms = mux_tick() - last_input_tick;

    uint32_t timeout_display_ms = config.SETTINGS.GENERAL.IDLE_DISPLAY * 1000;
    if (timeout_display_ms && idle_ms >= timeout_display_ms && !idle_display) {
        printf("IDLE DISPLAY\n");
        idle_display = true;
    }

    uint32_t timeout_sleep_ms = config.SETTINGS.GENERAL.IDLE_SLEEP * 1000;
    if (timeout_sleep_ms && idle_ms >= timeout_sleep_ms && !idle_sleep) {
        printf("IDLE SLEEP\n");
        idle_sleep = true;
    }
}

void handle_combo(int num, mux_input_action action) {
    const char *action_name;
    switch (action) {
        case MUX_INPUT_PRESS:
            action_name = "PRESS";
            break;
        case MUX_INPUT_HOLD:
            action_name = "HOLD";
            break;
        case MUX_INPUT_RELEASE:
            action_name = "RELEASE";
            break;
    }
    printf("%s %s\n", combo_name[num], action_name);
}

// Parse an input type specified by name on the command line.
//
// Returns MUX_INPUT_COUNT if the name is not valid.
int parse_type(const char *type_name) {
    const char *name[MUX_INPUT_COUNT] = {
        // Gamepad buttons:
        [MUX_INPUT_A] = "A",
        [MUX_INPUT_B] = "B",
        [MUX_INPUT_B] = "C",
        [MUX_INPUT_X] = "X",
        [MUX_INPUT_Y] = "Y",
        [MUX_INPUT_Y] = "Z",
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

    int i;
    for (i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (name[i] && strcmp(name[i], type_name) == 0) {
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
void parse_combo(int combo_num, const char *combo_spec, mux_input_combo *combo) {
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
        const char *type_name = strsep(&s, "+");

        // Look up input type by name.
        int type = parse_type(type_name);
        if (type == MUX_INPUT_COUNT) {
            fprintf(stderr, "muhotkey: combo specified invalid input name %s: %s\n", type_name,
                    combo_spec);
            exit(1);
        }

        // Update combo config accordingly.
        combo->type_mask |= BIT(type);
    } while (s);
}

int main(int argc, char *argv[]) {
    load_device(&device);
    load_config(&config);

    int js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("muhotkey: couldn't open joystick input device\n");
        return 1;
    }

    int js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("muhotkey: couldn't open system input device");
        return 1;
    }

    mux_input_options input_opts = {
        .gamepad_fd = js_fd,
        .system_fd = js_fd_sys,
        .max_idle_ms = 1000,
        .input_handler = handle_input,
        .combo_handler = handle_combo,
        .idle_handler = handle_idle,
    };

    for (int i = 0; i < argc - 1; ++i) {
        if (i >= MUX_INPUT_COMBO_COUNT) {
            fprintf(stderr, "muhotkey: at most %d combos supported\n", MUX_INPUT_COMBO_COUNT);
            return 1;
        }

        parse_combo(i, argv[i + 1], &input_opts.combo[i]);
    }

    setlinebuf(stdout);

    last_input_tick = mux_tick();
    mux_input_task(&input_opts);

    for (int i = 0; i < MUX_INPUT_COMBO_COUNT; ++i) {
        free(combo_name[i]);
    }

    close(js_fd);
    close(js_fd_sys);

    return 0;
}

uint32_t mux_tick(void) {
    static uint64_t start_ms = 0;

    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t) (now_ms - start_ms);
}
