#pragma once

#include <stdbool.h>

// Every input (button, D-pad, or stick direction) we support.
typedef enum {
    // Gamepad buttons:
    MUX_INPUT_A,
    MUX_INPUT_B,
    MUX_INPUT_C,
    MUX_INPUT_X,
    MUX_INPUT_Y,
    MUX_INPUT_Z,
    MUX_INPUT_L1,
    MUX_INPUT_L2,
    MUX_INPUT_L3,
    MUX_INPUT_R1,
    MUX_INPUT_R2,
    MUX_INPUT_R3,
    MUX_INPUT_SELECT,
    MUX_INPUT_START,

    // D-pad:
    MUX_INPUT_DPAD_UP,
    MUX_INPUT_DPAD_DOWN,
    MUX_INPUT_DPAD_LEFT,
    MUX_INPUT_DPAD_RIGHT,

    // Left stick:
    MUX_INPUT_LS_UP,
    MUX_INPUT_LS_DOWN,
    MUX_INPUT_LS_LEFT,
    MUX_INPUT_LS_RIGHT,

    // Right stick:
    MUX_INPUT_RS_UP,
    MUX_INPUT_RS_DOWN,
    MUX_INPUT_RS_LEFT,
    MUX_INPUT_RS_RIGHT,

    // Volume buttons:
    MUX_INPUT_VOL_UP,
    MUX_INPUT_VOL_DOWN,

    // Function buttons:
    MUX_INPUT_MENU_SHORT,
    MUX_INPUT_MENU_LONG,

    // System buttons:
    MUX_INPUT_POWER_SHORT,
    MUX_INPUT_POWER_LONG,

    MUX_INPUT_COUNT,
} mux_input_type;

// Actions that can be performed on an input: press and release, as well as hold.
typedef enum {
    MUX_INPUT_PRESS,
    MUX_INPUT_HOLD,
    MUX_INPUT_RELEASE,
} mux_input_action;

// Callback function invoked in response to a single specific input type and action.
typedef void (*mux_input_handler)();

// Callback function invoked in response to any arbitrary input type and action.
typedef void (*mux_input_catchall_handler)(mux_input_type, mux_input_action);

// Configuration for the muOS input subsystem.
typedef struct {
    // File descriptors for the underlying evdev devices.
    int gamepad_fd;
    int system_fd;

    // Whether to swap the A & B buttons.
    bool swap_nav;

    // Whether to swap the up/down and left/right axes on the D-pad and sticks.
    bool swap_axis;

    // Callback functions for inputs. Fired in sequence: one press, zero or more holds, one release.
    //
    // Handlers may be NULL, in which case that input will be ignored.
    mux_input_handler press_handler[MUX_INPUT_COUNT];
    mux_input_handler hold_handler[MUX_INPUT_COUNT];
    mux_input_handler release_handler[MUX_INPUT_COUNT];

    // Generic handler for input press/hold/release events. For programs that want to "go offroad"
    // and handle every input event in a custom way.
    //
    // Most use case are probably easier to read if they use the individual handlers above instead.
    //
    // May be NULL, in which case no input handler will be invoked.
    mux_input_catchall_handler input_handler;

    // Handler called after each iteration of the event loop, regardless of whether any input fired.
    //
    // May be NULL, in which case no idle handler will be invoked.
    mux_input_handler idle_handler;
} mux_input_options;

// Starts the main input event loop, which runs until terminated by calling mux_input_stop.
void mux_input_task(const mux_input_options *opts);

// Returns whether or not the specified input is currently pressed.
bool mux_input_pressed(mux_input_type type);

// Causes the input task to exit at the start of the next iteration of the event loop (e.g., after
// processing currently pressed or held inputs).
void mux_input_stop();
