#include "input.h"
#include <pthread.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/param.h>
#include <unistd.h>
#include <ctype.h>
#include "common.h"
#include "config.h"
#include "controller_profile.h"
#include "device.h"
#include "log.h"

struct controller_profile controller;

extern uint32_t mux_tick(void);

// Whether to exit the input task before the next iteration of the event loop.
static bool stop = false;

// System clock tick at the start of this iteration of the event loop.
static uint32_t tick = 0;

// Bitmask of input types that are currently active.
static uint64_t pressed = 0;

// Bitmask of input types that were active during the previous iteration of the event loop.
static uint64_t held = 0;

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
        type = !opts->swap_btn ? MUX_INPUT_X : MUX_INPUT_Y;
    } else if (event->code == device.RAW_INPUT.BUTTON.Y) {
        type = !opts->swap_btn ? MUX_INPUT_Y : MUX_INPUT_X;
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
    pressed = (event->value == 1) ? (pressed | BIT(type)) : (pressed & ~BIT(type));
}

// Processes gamepad axes (D-pad and the sticks).
static void process_abs(const mux_input_options *opts, const struct input_event *event) {
    int axis;
    bool analog;
    if (event->code == device.RAW_INPUT.DPAD.UP) {
        // Axis: D-pad vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_UP : MUX_INPUT_DPAD_LEFT;
        analog = false;
    } else if (event->code == device.RAW_INPUT.DPAD.LEFT) {
        // Axis: D-pad horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_LEFT : MUX_INPUT_DPAD_UP;
        analog = false;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.UP) {
        // Axis: left stick vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_LS_UP : MUX_INPUT_LS_LEFT;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.LEFT) {
        // Axis: left stick horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_LS_LEFT : MUX_INPUT_LS_UP;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.UP) {
        // Axis: right stick vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_RS_UP : MUX_INPUT_RS_LEFT;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.LEFT) {
        // Axis: right stick horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_RS_LEFT : MUX_INPUT_RS_UP;
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
        pressed = ((pressed | BIT(axis)) & ~BIT(axis + 1));
    } else if ((analog && event->value >= device.INPUT.AXIS - device.INPUT.AXIS / 5) ||
               (!analog && event->value == 1)) {
        // Direction: down/right
        pressed = ((pressed | BIT(axis + 1)) & ~BIT(axis));
    } else {
        // Direction: center
        pressed &= ~(BIT(axis) | BIT(axis + 1));
    }
}

// Process system buttons.
static void process_sys(const mux_input_options *opts, const struct input_event *event) {
    if (event->code == device.RAW_INPUT.BUTTON.POWER_SHORT) {
        switch (event->value) {
            case 1:
                // Power button: short press
                pressed = ((pressed | BIT(MUX_INPUT_POWER_SHORT)) & ~BIT(MUX_INPUT_POWER_LONG));
                break;
            case 2:
                // Power button: long press
                pressed = ((pressed | BIT(MUX_INPUT_POWER_LONG)) & ~BIT(MUX_INPUT_POWER_SHORT));
                break;
            default:
                // Power button: release
                pressed &= ~(BIT(MUX_INPUT_POWER_SHORT) | BIT(MUX_INPUT_POWER_LONG));
                break;
        }
    }
}

// Processes 8bitdo USB Pro 2 in D-Input mode gamepad buttons.
static void process_usb_key(const mux_input_options *opts, struct js_event js) {
    mux_input_type type;
    if (js.number == controller.BUTTON.A) {
        type = !opts->swap_btn ? MUX_INPUT_A : MUX_INPUT_B;
    } else if (js.number == controller.BUTTON.B) {
        type = !opts->swap_btn ? MUX_INPUT_B : MUX_INPUT_A;
    } else if (js.number == controller.BUTTON.X) {
        type = !opts->swap_btn ? MUX_INPUT_X : MUX_INPUT_Y;
    } else if (js.number == controller.BUTTON.Y) {
        type = !opts->swap_btn ? MUX_INPUT_Y : MUX_INPUT_X;
    } else if (js.number == controller.BUTTON.L1) {
        type = MUX_INPUT_L1;
    } else if (js.number == controller.BUTTON.L2) {
        type = MUX_INPUT_L2;
    } else if (js.number == controller.BUTTON.L3) {
        type = MUX_INPUT_L3;
    } else if (js.number == controller.BUTTON.R1) {
        type = MUX_INPUT_R1;
    } else if (js.number == controller.BUTTON.R2) {
        type = MUX_INPUT_R2;
    } else if (js.number == controller.BUTTON.R3) {
        type = MUX_INPUT_R3;
    } else if (js.number == controller.BUTTON.SELECT) {
        type = MUX_INPUT_SELECT;
    } else if (js.number == controller.BUTTON.START) {
        type = MUX_INPUT_START;
    } else if (js.number == controller.BUTTON.MENU) {
        type = MUX_INPUT_MENU_SHORT;
    } else {
        return;
    }
    pressed = (js.value == 1) ? (pressed | BIT(type)) : (pressed & ~BIT(type));
}

// Processes 8bitdo USB Pro 2 in D-Input mode gamepad axes (D-pad and the sticks).
static void process_usb_abs(const mux_input_options *opts, struct js_event js) {
    int axis;
    int axis_max = 32767;
    if (js.number == controller.DPAD.UP) {
        // Axis: D-pad vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_UP : MUX_INPUT_DPAD_LEFT;
        axis_max = controller.DPAD.AXIS;
    } else if (js.number == controller.DPAD.LEFT) {
        // Axis: D-pad horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_LEFT : MUX_INPUT_DPAD_UP;
        axis_max = controller.DPAD.AXIS;
    } else if (js.number == controller.ANALOG.LEFT.UP) {
        // Axis: left stick vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_LS_UP : MUX_INPUT_LS_LEFT;
        axis_max = controller.ANALOG.LEFT.AXIS;
    } else if (js.number == controller.ANALOG.LEFT.LEFT) {
        // Axis: left stick horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_LS_LEFT : MUX_INPUT_LS_UP;
        axis_max = controller.ANALOG.LEFT.AXIS;
    } else if (js.number == controller.ANALOG.RIGHT.UP) {
        // Axis: right stick vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_RS_UP : MUX_INPUT_RS_LEFT;
        axis_max = controller.ANALOG.RIGHT.AXIS;
    } else if (js.number == controller.ANALOG.RIGHT.LEFT) {
        // Axis: right stick horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_RS_LEFT : MUX_INPUT_RS_UP;
        axis_max = controller.ANALOG.RIGHT.AXIS;
    } else if (js.number == controller.TRIGGER.L2) {
        int threshold = (controller.TRIGGER.AXIS * 80) / 100;
        if (threshold > 0) {
            pressed = (js.value >= threshold) ? (pressed | BIT(MUX_INPUT_L2)) : (pressed & ~BIT(MUX_INPUT_L2));
        } else {
            pressed = (js.value <= threshold) ? (pressed | BIT(MUX_INPUT_L2)) : (pressed & ~BIT(MUX_INPUT_L2));
        }
        return;
    } else if (js.number == controller.TRIGGER.R2) {
        int threshold = (controller.TRIGGER.AXIS * 80) / 100;
        if (threshold > 0) {
            pressed = (js.value >= threshold) ? (pressed | BIT(MUX_INPUT_R2)) : (pressed & ~BIT(MUX_INPUT_R2));
        } else {
            pressed = (js.value <= threshold) ? (pressed | BIT(MUX_INPUT_R2)) : (pressed & ~BIT(MUX_INPUT_R2));
        }
        return;
    } else {
        return;
    }

    // Sometimes, hardware issues prevent sticks from reaching the full range of the analog axis.
    // (This is especially common with cheap Hall-effect sticks.)
    //
    // We use threshold of 80% of the nominal axis maximum to detect analog directional presses,
    // which seems to accommodate most variation without being too sensitive for "in-spec" sticks.
    if (js.value <= -axis_max + axis_max / 5) {
        // Direction: up/left
        pressed = ((pressed | BIT(axis)) & ~BIT(axis + 1));
    } else if (js.value >= axis_max - axis_max / 5) {
        // Direction: down/right
        pressed = ((pressed | BIT(axis + 1)) & ~BIT(axis));
    } else {
        // Direction: center
        pressed &= ~(BIT(axis) | BIT(axis + 1));
    }
}

// Processes gamepad button D-pad.  // Some controllers like PS3 the DPAD triggers button press events
static void process_usb_dpad_as_buttons(const mux_input_options *opts, struct js_event js) {
    int axis;
    int direction;
    if (js.number == controller.BUTTON.UP) {
        // Axis: D-pad vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_UP : MUX_INPUT_DPAD_LEFT;
        direction = -js.value;
    } else if (js.number == controller.BUTTON.LEFT) {
        // Axis: D-pad horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_LEFT : MUX_INPUT_DPAD_UP;
        direction = -js.value;
    } else if (js.number == controller.BUTTON.DOWN) {
        // Axis: D-pad vertical
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_UP : MUX_INPUT_DPAD_LEFT;
        direction = js.value;
    } else if (js.number == controller.BUTTON.RIGHT) {
        // Axis: D-pad horizontal
        axis = !opts->swap_axis || key_show ? MUX_INPUT_DPAD_LEFT : MUX_INPUT_DPAD_UP;
        direction = js.value;
    } else {
        return;
    }

    if (direction == -1) {
        // Direction: up/left
        pressed = ((pressed | BIT(axis)) & ~BIT(axis + 1));
    } else if (direction == 1) {
        // Direction: down/right
        pressed = ((pressed | BIT(axis + 1)) & ~BIT(axis));
    } else {
        // Direction: center
        pressed &= ~(BIT(axis) | BIT(axis + 1));
    }
}

// Invokes the relevant handler(s) for a particular input type and action.
static void dispatch_input(const mux_input_options *opts,
                           mux_input_type type,
                           mux_input_action action) {
    // Remap input types when using left stick as D-pad. (We still track pressed and held status for
    // the stick and D-pad inputs separately to avoid unintuitive hold behavior.)
    if (opts->stick_nav) {
        switch (type) {
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

// Invokes the relevant handler(s) for a particular input combo number and action.
static void dispatch_combo(const mux_input_options *opts, int num, mux_input_action action) {
    mux_input_handler handler = NULL;
    switch (action) {
        case MUX_INPUT_PRESS:
            handler = opts->combo[num].press_handler;
            break;
        case MUX_INPUT_HOLD:
            handler = opts->combo[num].hold_handler;
            break;
        case MUX_INPUT_RELEASE:
            handler = opts->combo[num].release_handler;
            break;
    }

    // First invoke specific handler (if one was registered for this combo number and action).
    if (handler) {
        handler();
    }

    // Then invoke generic handler (if a catchall handler was registered).
    if (opts->combo_handler) {
        opts->combo_handler(num, action);
    }
}

static void handle_inputs(const mux_input_options *opts) {
    // Delay (millis) before invoking hold handler again.
    static uint32_t hold_delay[MUX_INPUT_COUNT] = {};
    // Tick (millis) of last press or hold.
    static uint32_t hold_tick[MUX_INPUT_COUNT] = {};

    for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (pressed & BIT(i)) {
            if (!(held & BIT(i))) {
                // Pressed & not held: Invoke "press" handler.
                dispatch_input(opts, i, MUX_INPUT_PRESS);

                // Double delay before initial repeat.
                hold_delay[i] = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick[i] = tick;
            } else if (tick - hold_tick[i] >= hold_delay[i]) {
                // Pressed & held: Invoke "hold" handler.
                dispatch_input(opts, i, MUX_INPUT_HOLD);

                // Single delay for each subsequent repeat.
                hold_delay[i] = config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick[i] = tick;
            }
        } else if (held & BIT(i)) {
            // Held & not pressed: Invoke "release" handler.
            dispatch_input(opts, i, MUX_INPUT_RELEASE);
        }
    }
}

static void handle_combos(const mux_input_options *opts) {
    // Delay (millis) before invoking hold handler again.
    static uint32_t hold_delay = 0;
    // Tick (millis) of last press or hold.
    static uint32_t hold_tick = 0;

    // Combo number that was active during the previous iteration of the event loop, or
    // MUX_INPUT_COMBO_COUNT when no combo is held.
    static int active_combo = MUX_INPUT_COMBO_COUNT;

    if (active_combo != MUX_INPUT_COMBO_COUNT) {
        // Active combo; check if it's still held or was released.
        uint64_t mask = opts->combo[active_combo].type_mask;

        if ((pressed & mask) == mask) {
            if (tick - hold_tick >= hold_delay) {
                // Pressed & held: Invoke "hold" handler.
                dispatch_combo(opts, active_combo, MUX_INPUT_HOLD);

                // Single delay for each subsequent repeat.
                hold_delay = config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick = tick;
            }
        } else {
            // Held & not pressed: Invoke "release" handler.
            dispatch_combo(opts, active_combo, MUX_INPUT_RELEASE);
            active_combo = MUX_INPUT_COMBO_COUNT;
        }
    }

    if (active_combo == MUX_INPUT_COMBO_COUNT && (pressed & ~held)) {
        // No active combo, but a new input was pressed. Check if a combo should activate.
        //
        // Sometimes, a single evdev event can result in us registering both a release and a press
        // (e.g., when transitioning from POWER_SHORT to POWER_LONG), so we have to check this even
        // if a combo was previously active at the start of the function.
        for (int i = 0; i < MUX_INPUT_COMBO_COUNT; ++i) {
            uint64_t mask = opts->combo[i].type_mask;

            if (mask && (pressed & mask) == mask) {
                // Pressed & not held: Invoke "press" handler.
                dispatch_combo(opts, i, MUX_INPUT_PRESS);
                active_combo = i;

                // Double delay before initial repeat.
                hold_delay = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick = tick;

                // Only one combo can be active at a time.
                break;
            }
        }
    }
}

char *get_unique_controller_id(int usb_fd) {
    char name[128] = "Unknown";
    char vendor[16] = "0000";
    char product[16] = "0000";
    char controller_id[256];
    static char result[256];

    // Get joystick name
    if (ioctl(usb_fd, JSIOCGNAME(sizeof(name)), name) < 0) {
        LOG_ERROR("input", "Error reading joystick name")
        strncpy(name, "Unknown", sizeof(name));
    }

    // Normalize name to remove multiple spaces
    int j = 0;
    for (int i = 0; name[i] != '\0'; i++) {
        if (!(isspace(name[i]) && (j > 0 && isspace(result[j - 1])))) {
            result[j++] = name[i];
        }
    }
    result[j] = '\0';
    strncpy(name, result, sizeof(name));

    // Get Vendor ID
    FILE *vendor_file = fopen("/sys/class/input/js1/device/id/vendor", "r");
    if (vendor_file) {
        if (fgets(vendor, sizeof(vendor), vendor_file)) {
            vendor[strcspn(vendor, "\n")] = '\0'; // Remove trailing newline
        }
        fclose(vendor_file);
    } else {
        LOG_ERROR("input", "Failed to read vendor ID")
    }

    // Get Product ID
    FILE *product_file = fopen("/sys/class/input/js1/device/id/product", "r");
    if (product_file) {
        if (fgets(product, sizeof(product), product_file)) {
            product[strcspn(product, "\n")] = '\0'; // Remove trailing newline
        }
        fclose(product_file);
    } else {
        LOG_ERROR("input", "Failed to read product ID")
    }

    // Build the unique ID
    snprintf(controller_id, sizeof(controller_id), "%s_V%s_P%s", name, vendor, product);
    strncpy(result, controller_id, sizeof(result));

    return result;
}

void *joystick_handler(void *arg) {
    // Cast the argument back to the correct type
    const mux_input_options *opts = (const mux_input_options *) arg;
    // Open USB controller
    int usb_fd = open("/dev/input/js1", O_RDONLY);
    if (usb_fd == -1) {
        LOG_WARN("input", "Failed to open USB controller")
        return NULL;
    }

    load_controller_profile(&controller, get_unique_controller_id(usb_fd));

    struct js_event js;
    while (!stop) {
        // Read joystick event
        ssize_t bytes = read(usb_fd, &js, sizeof(struct js_event));
        if (bytes != sizeof(struct js_event)) {
            LOG_ERROR("input", "Error reading joystick event")
            break;
        }

        if (js.type & JS_EVENT_INIT) continue;
        LOG_INFO("input", "Joystick Event: type=%u number=%u value=%d", js.type, js.number, js.value)

        if (js.type & JS_EVENT_BUTTON) {
            if (js.number == controller.BUTTON.UP || js.number == controller.BUTTON.DOWN ||
                js.number == controller.BUTTON.LEFT || js.number == controller.BUTTON.RIGHT) {
                process_usb_dpad_as_buttons(opts, js);
            } else {
                process_usb_key(opts, js);
            }
        } else if (js.type & JS_EVENT_AXIS) {
            process_usb_abs(opts, js);
        }
    }
    close(usb_fd);
    return NULL;
}

void mux_input_task(const mux_input_options *opts) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        LOG_ERROR("input", "epoll create error")
        return;
    }

    struct epoll_event epoll_event[device.DEVICE.EVENT];

    epoll_event[0].events = EPOLLIN;
    epoll_event[0].data.fd = opts->gamepad_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, opts->gamepad_fd, &epoll_event[0]) == -1) {
        LOG_ERROR("input", "epoll control error - gamepad_fd")
        return;
    }

    epoll_event[0].events = EPOLLIN;
    epoll_event[0].data.fd = opts->system_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, opts->system_fd, &epoll_event[0]) == -1) {
        LOG_ERROR("input", "epoll control error - system_fd")
        return;
    }

    pthread_t joystick_thread;
    mux_input_options joystick_opts = *opts;
    pthread_create(&joystick_thread, NULL, joystick_handler, &joystick_opts);

    // Delay (millis) to wait for input before timing out. This determines the rate at which the
    // hold_handlers and idle_handler are called. To save CPU, we want to wait as long as possible.
    //
    // If no inputs are held, we only have the idle_handler to worry about, so we wait max_idle_ms
    // (or forever if unspecified).
    //
    // If at least one input is held, we instead wait either max_idle_ms or the "menu acceleration"
    // delay (whichever is shorter).
    int timeout;
    int timeout_hold;
    if (opts->max_idle_ms) {
        timeout = opts->max_idle_ms;
        timeout_hold = MIN(opts->max_idle_ms, config.SETTINGS.ADVANCED.ACCELERATE);
    } else {
        timeout = -1 /* infinite */;
        timeout_hold = config.SETTINGS.ADVANCED.ACCELERATE;
    }

    // Input event loop:
    while (!stop) {
        int num_events = epoll_wait(epoll_fd, epoll_event, device.DEVICE.EVENT,
                                    held ? timeout_hold : timeout);
        if (num_events == -1) {
            LOG_ERROR("input", "epoll wait error")
            continue;
        }

        // Read evdev input events and update pressed/released inputs.
        for (int i = 0; i < num_events; ++i) {
            struct input_event event;
            if (read(epoll_event[i].data.fd, &event, sizeof(event)) == -1) {
                LOG_ERROR("input", "epoll event read error")
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

        // Identify and invoke handlers for inputs whose state changed.
        tick = mux_tick();
        handle_inputs(opts);
        handle_combos(opts);

        // Invoke "idle" handler to run extra logic every iteration of the event loop.
        if (opts->idle_handler) {
            opts->idle_handler();
        }

        // Inputs pressed at the end of one iteration are held at the start of the next.
        held = pressed;
    }
}

uint32_t mux_input_tick(void) {
    return tick;
}

bool mux_input_pressed(mux_input_type type) {
    return pressed & BIT(type);
}

void mux_input_stop(void) {
    stop = true;
}
