#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <SDL2/SDL.h>

#define GCB_PATH_MAIN "/usr/lib/gamecontrollerdb.txt"
#define GCB_PATH_USER "/opt/muos/share/info/gamecontrollerdb/user.txt"

#define SLOT_COUNT 21
#define AXIS_THRESHOLD 16384

typedef struct {
    const char *gc_key;
    const char *label;

    int is_axis;
    int is_half_axis;
} slot_def;

static const slot_def slots[SLOT_COUNT] = {
        {"a",             "A",          0, 0},
        {"b",             "B",          0, 0},
        {"x",             "X",          0, 0},
        {"y",             "Y",          0, 0},
        {"leftshoulder",  "L1",         0, 0},
        {"rightshoulder", "R1",         0, 0},
        {"lefttrigger",   "L2",         0, 1},
        {"righttrigger",  "R2",         0, 1},
        {"leftstick",     "L3",         0, 0},
        {"rightstick",    "R3",         0, 0},
        {"start",         "Start",      0, 0},
        {"back",          "Select",     0, 0},
        {"guide",         "Menu",       0, 0},
        {"dpup",          "DPAD Up",    0, 0},
        {"dpdown",        "DPAD Down",  0, 0},
        {"dpleft",        "DPAD Left",  0, 0},
        {"dpright",       "DPAD Right", 0, 0},
        {"leftx",         "Left X",     1, 0},
        {"lefty",         "Left Y",     1, 0},
        {"rightx",        "Right X",    1, 0},
        {"righty",        "Right Y",    1, 0},
};

static char phys[SLOT_COUNT][32];
static volatile int quit = 0;

static void handle_signal(int sig) {
    (void) sig;
    quit = 1;
}

static int capture_event(int slot_idx, char *out) {
    printf("  >> Press the physical input for [%s]  (or Ctrl-C to skip)... ", slots[slot_idx].label);
    fflush(stdout);

    SDL_Event ev;
    while (!quit) {
        if (!SDL_WaitEventTimeout(&ev, 250)) continue;

        switch (ev.type) {
            case SDL_JOYBUTTONDOWN:
                snprintf(out, 32, "b%d", ev.jbutton.button);
                printf("b%d\n", ev.jbutton.button);
                return 1;
            case SDL_JOYHATMOTION:
                if (ev.jhat.value == SDL_HAT_CENTERED) continue;
                snprintf(out, 32, "h%d.%d", ev.jhat.hat, ev.jhat.value);
                printf("h%d.%d\n", ev.jhat.hat, ev.jhat.value);
                return 1;
            case SDL_JOYAXISMOTION: {
                int16_t val = ev.jaxis.value;
                if (val < -AXIS_THRESHOLD || val > AXIS_THRESHOLD) {
                    int axis = ev.jaxis.axis;
                    if (slots[slot_idx].is_axis) {
                        snprintf(out, 32, "a%d", axis);
                        printf("a%d\n", axis);
                    } else {
                        snprintf(out, 32, "%sa%d", val > 0 ? "+" : "-", axis);
                        printf("%sa%d\n", val > 0 ? "+" : "-", axis);
                    }
                    return 1;
                }
                break;
            }
            case SDL_QUIT:
                quit = 1;
                break;
            default:
                break;
        }
    }

    printf("(skipped)\n");
    return 0;
}

static void build_mapping_line(const char *guid, const char *name, char *buf) {
    int n = snprintf(buf, 4096, "%s,%s", guid, name);

    for (int i = 0; i < SLOT_COUNT; i++) {
        if (phys[i][0]) n += snprintf(buf + n, 4096 - (size_t) n, ",%s:%s", slots[i].gc_key, phys[i]);
    }

    snprintf(buf + n, 4096 - (size_t) n, ",platform:Linux,");
}

int main(int argc, char **argv) {
    const char *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else {
            fprintf(stderr, "Usage: muremap [--output <file>]\n");
            return 1;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GameControllerAddMappingsFromFile(GCB_PATH_MAIN);
    SDL_GameControllerAddMappingsFromFile(GCB_PATH_USER);

    SDL_JoystickEventState(SDL_ENABLE);
    SDL_PumpEvents();
    SDL_JoystickUpdate();

    int num = SDL_NumJoysticks();
    if (num <= 0) {
        fprintf(stderr, "No joystick/controller detected.\n");
        SDL_Quit();
        return 1;
    }

    SDL_Joystick *joy = SDL_JoystickOpen(0);
    if (!joy) {
        fprintf(stderr, "Failed to open joystick: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    char guid[64];
    SDL_JoystickGUID joy_guid = SDL_JoystickGetGUID(joy);
    SDL_JoystickGetGUIDString(joy_guid, guid, sizeof(guid));

    const char *name = SDL_JoystickName(joy);
    if (!name) name = "Unknown";

    printf("muremap - MustardOS Input Remap Tool\n");
    printf("================================\n");
    printf("Device : %s\n", name);
    printf("GUID   : %s\n", guid);
    printf("Axes   : %d   Buttons: %d   Hats: %d\n\n",
           SDL_JoystickNumAxes(joy),
           SDL_JoystickNumButtons(joy),
           SDL_JoystickNumHats(joy));
    printf("Press each physical input when prompted.\n");
    printf("Press Ctrl-C during a prompt to skip that slot.\n\n");

    memset(phys, 0, sizeof(phys));

    for (int i = 0; i < SLOT_COUNT && !quit; i++) {
        int was_quit = quit;

        quit = 0;
        capture_event(i, phys[i]);

        if (was_quit) {
            quit = 1;
            break;
        }
    }

    char mapping[4096];
    build_mapping_line(guid, name, mapping);

    printf("\n── Result ──────────────────────────────────────────────────────\n");
    printf("%s\n", mapping);
    printf("────────────────────────────────────────────────────────────────\n\n");

    if (output_file) {
        FILE *f = fopen(output_file, "w");
        if (!f) {
            perror(output_file);
        } else {
            fprintf(f, "# muremap output\n%s\n", mapping);
            fclose(f);
            printf("Written to: %s\n", output_file);
        }
    }

    SDL_JoystickClose(joy);
    SDL_Quit();

    return 0;
}
