#include "input.h"

#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/param.h>
#include <unistd.h>

#include "config.h"
#include "device.h"

extern uint32_t mux_tick();

// Whether to exit the input task before the next iteration of the event loop.
static bool stop = false;

// Whether or not each input is currently active.
static bool pressed[MUX_INPUT_COUNT] = {};

// Processes gamepad buttons.
static void process_key(const mux_input_options *opts, const struct input_event *event) {
    mux_input_type type;
    if (event->code == device.RAW_INPUT.BUTTON.A) {
        type = !opts->swap_btn ? MUX_INPUT_A : MUX_INPUT_B;
    } else if (event->code == device.RAW_INPUT.BUTTON.B) {
        type = !opts->swap_btn ? MUX_INPUT_B : MUX_INPUT_A;
    } else if (event->code == device.RAW_INPUT.BUTTON.C) {
        type = MUX_INPUT_C;
    } else if (event->code == device.RAW_INPUT.BUTTON.X) {
        type = MUX_INPUT_X;
    } else if (event->code == device.RAW_INPUT.BUTTON.Y) {
        type = MUX_INPUT_Y;
    } else if (event->code == device.RAW_INPUT.BUTTON.Z) {
        type = MUX_INPUT_Z;
    } else if (event->code == device.RAW_INPUT.BUTTON.L1) {
        type = MUX_INPUT_L1;
    } else if (event->code == device.RAW_INPUT.BUTTON.L2) {
        type = MUX_INPUT_L2;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.CLICK) {
        type = MUX_INPUT_L3;
    } else if (event->code == device.RAW_INPUT.BUTTON.R1) {
        type = MUX_INPUT_R1;
    } else if (event->code == device.RAW_INPUT.BUTTON.R2) {
        type = MUX_INPUT_R2;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.CLICK) {
        type = MUX_INPUT_R3;
    } else if (event->code == device.RAW_INPUT.BUTTON.SELECT) {
        type = MUX_INPUT_SELECT;
    } else if (event->code == device.RAW_INPUT.BUTTON.START) {
        type = MUX_INPUT_START;
    } else if (event->code == device.RAW_INPUT.BUTTON.VOLUME_UP) {
        type = MUX_INPUT_VOL_UP;
    } else if (event->code == device.RAW_INPUT.BUTTON.VOLUME_DOWN) {
        type = MUX_INPUT_VOL_DOWN;
    } else if (event->code == device.RAW_INPUT.BUTTON.MENU_SHORT) {
        type = MUX_INPUT_MENU_SHORT;
    } else if (event->code == device.RAW_INPUT.BUTTON.MENU_LONG) {
        type = MUX_INPUT_MENU_LONG;
    } else {
        return;
    }
    pressed[type] = (event->value == 1);
}

// Processes gamepad axes (D-pad and the sticks).
static void process_abs(const mux_input_options *opts, const struct input_event *event) {
    int axis;
    bool analog;
    if (event->code == device.RAW_INPUT.DPAD.UP) {
        // Axis: D-pad vertical
        axis = !opts->swap_axis ? MUX_INPUT_DPAD_UP : MUX_INPUT_DPAD_LEFT;
        analog = false;
    } else if (event->code == device.RAW_INPUT.DPAD.LEFT) {
        // Axis: D-pad horizontal
        axis = !opts->swap_axis ? MUX_INPUT_DPAD_LEFT : MUX_INPUT_DPAD_UP;
        analog = false;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.UP) {
        // Axis: left stick vertical
        axis = !opts->swap_axis ? MUX_INPUT_LS_UP : MUX_INPUT_LS_LEFT;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.LEFT) {
        // Axis: left stick horizontal
        axis = !opts->swap_axis ? MUX_INPUT_LS_LEFT : MUX_INPUT_LS_UP;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.UP) {
        // Axis: right stick vertical
        axis = !opts->swap_axis ? MUX_INPUT_RS_UP : MUX_INPUT_RS_LEFT;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.LEFT) {
        // Axis: right stick horizontal
        axis = !opts->swap_axis ? MUX_INPUT_RS_LEFT : MUX_INPUT_RS_UP;
        analog = true;
    } else {
        return;
    }

    // Sometimes, hardware issues prevent sticks from reaching the full range of the analog axis.
    // (This is especially common with cheap Hall-effect sticks.)
    //
    // We use threshold of 80% of the nominal axis maximum to detect analog directional presses,
    // which seems to accommodate most variation without being too sensitive for "in-spec" sticks.
    if ((analog && event->value <= -device.INPUT.AXIS + device.INPUT.AXIS / 5) ||
            (!analog && event->value == -1)) {
        // Direction: up/left
        pressed[axis] = true;
        pressed[axis + 1] = false;
    } else if ((analog && event->value >= device.INPUT.AXIS - device.INPUT.AXIS / 5) ||
            (!analog && event->value == 1)) {
        // Direction: down/right
        pressed[axis] = false;
        pressed[axis + 1] = true;
    } else {
        // Direction: center
        pressed[axis] = false;
        pressed[axis + 1] = false;
    }
}

// Process system buttons.
static void process_sys(const mux_input_options *opts, const struct input_event *event) {
    if (event->code == device.RAW_INPUT.BUTTON.POWER_SHORT) {
        if (event->value == 1) {
            // Power button: short press
            pressed[MUX_INPUT_POWER_SHORT] = 1;
            pressed[MUX_INPUT_POWER_LONG] = 0;
        } else if (event->value == 2) {
            // Power button: long press
            pressed[MUX_INPUT_POWER_SHORT] = 0;
            pressed[MUX_INPUT_POWER_LONG] = 1;
        } else {
            // Power button: release
            pressed[MUX_INPUT_POWER_SHORT] = 0;
            pressed[MUX_INPUT_POWER_LONG] = 0;
        }
    }
}

// Invokes the relevant handler(s) for a particular input type and action.
static void handle_input(const mux_input_options *opts,
                         mux_input_type type,
                         mux_input_action action) {
    // Remap input types when using left stick as D-pad. (We still track pressed and held status for
    // the stick and D-pad inputs separately to avoid unintuitive hold behavior.)
    if (opts->stick_nav) {
        switch (type) {
            case MUX_INPUT_L3:
                type = MUX_INPUT_A;
                break;
            case MUX_INPUT_LS_UP:
                type = MUX_INPUT_DPAD_UP;
                break;
            case MUX_INPUT_LS_DOWN:
                type = MUX_INPUT_DPAD_DOWN;
                break;
            case MUX_INPUT_LS_LEFT:
                type = MUX_INPUT_DPAD_LEFT;
                break;
            case MUX_INPUT_LS_RIGHT:
                type = MUX_INPUT_DPAD_RIGHT;
                break;
            default:
                break;
        }
    }

    mux_input_handler handler = NULL;
    switch (action) {
        case MUX_INPUT_PRESS:
            handler = opts->press_handler[type];
            break;
        case MUX_INPUT_HOLD:
            handler = opts->hold_handler[type];
            break;
        case MUX_INPUT_RELEASE:
            handler = opts->release_handler[type];
            break;
    }

    // First invoke specific handler (if one was registered for this input type and action).
    if (handler) {
        handler();
    }

    // Then invoke generic handler (if a catchall handler was registered).
    if (opts->input_handler) {
        opts->input_handler(type, action);
    }
}

void mux_input_task(const mux_input_options *opts) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("mux_input_task: epoll_create1");
        return;
    }

    struct epoll_event epoll_event[device.DEVICE.EVENT];

    epoll_event[0].events = EPOLLIN;
    epoll_event[0].data.fd = opts->gamepad_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, opts->gamepad_fd, &epoll_event[0]) == -1) {
        perror("mux_input_task: epoll_ctl(gamepad_fd)");
        return;
    }

    epoll_event[0].events = EPOLLIN;
    epoll_event[0].data.fd = opts->system_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, opts->system_fd, &epoll_event[0]) == -1) {
        perror("mux_input_task: epoll_ctl(system_fd)");
        return;
    }

    // Inputs active during the previous iteration of the event loop.
    bool held[MUX_INPUT_COUNT] = {};
    // Delay (millis) before invoking hold handler again.
    uint32_t hold_delay[MUX_INPUT_COUNT] = {};
    // Tick (millis) each input was last pressed or held.
    uint32_t hold_tick[MUX_INPUT_COUNT] = {};

    // Delay (millis) to wait for an input event before timing out. This determines two things:
    //
    // 1. The min rate at which hold_handlers are called. (This delay should be no longer than the
    //    shortly expected "menu acceleration" setting.)
    // 2. The min rate at which idle_handler is called. (This controls the screen refresh interval.)
    //
    // When there's an idle handler registered, force a delay of no more than 16ms, roughly 60 FPS.
    // Otherwise, wait up to the hold delay to reduce CPU usage.
    int idle_delay = config.SETTINGS.ADVANCED.ACCELERATE;
    if (opts->idle_handler) {
        idle_delay = MIN(16, idle_delay);
    }

    // Input event loop:
    while (!stop) {
        int num_events = epoll_wait(epoll_fd, epoll_event, device.DEVICE.EVENT, idle_delay);
        if (num_events == -1) {
            perror("mux_input_task: epoll_wait");
            continue;
        }

        // Read evdev input events and update pressed/released keys.
        for (int i = 0; i < num_events; ++i) {
            struct input_event event;
            if (read(epoll_event[i].data.fd, &event, sizeof(event)) == -1) {
                perror("mux_input_task: read");
                continue;
            }

            if (epoll_event[i].data.fd == opts->gamepad_fd) {
                if (event.type == EV_KEY) {
                    process_key(opts, &event);
                } else if (event.type == EV_ABS) {
                    process_abs(opts, &event);
                }
            } else if (epoll_event[i].data.fd == opts->system_fd) {
                process_sys(opts, &event);
            }
        }

        // Invoke registered input handlers.
        uint32_t tick = mux_tick();

        for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
            if (pressed[i]) {
                if (!held[i]) {
                    // Pressed & not held: Invoke "press" handler.
                    handle_input(opts, i, MUX_INPUT_PRESS);

                    // Double delay before initial repeat.
                    hold_delay[i] = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                    hold_tick[i] = tick;
                } else if (tick - hold_tick[i] >= hold_delay[i]) {
                    // Pressed & held: Invoke "hold" handler (every ACCELERATE interval).
                    handle_input(opts, i, MUX_INPUT_HOLD);

                    // Single delay for each subsequent repeat.
                    hold_delay[i] = config.SETTINGS.ADVANCED.ACCELERATE;
                    hold_tick[i] = tick;
                }
            } else if (held[i]) {
                // Held & not pressed: Invoke "release" handler.
                handle_input(opts, i, MUX_INPUT_RELEASE);
            }
        }

        // Keys pressed at the end of iteration N are held at the start of iteration N+1.
        memcpy(held, pressed, sizeof(pressed));

        // Invoke "idle" handler to run extra logic every iteration of the event loop.
        if (opts->idle_handler) {
            opts->idle_handler();
        }
    }
}

bool mux_input_pressed(mux_input_type type) {
    return pressed[type];
}

void mux_input_stop() {
    stop = true;
}
