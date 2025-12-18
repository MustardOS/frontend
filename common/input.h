#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>

extern int key_show;
extern bool swap_axis;

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
    MUX_INPUT_SWITCH,

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
typedef void (*mux_input_handler)(void);

// Callback function invoked in response to any arbitrary input type and action.
typedef void (*mux_input_catchall_handler)(mux_input_type, mux_input_action);

// Callback function invoked in response to a multi-input combo.
typedef void (*mux_input_catchall_combo_handler)(int, mux_input_action);

// Maximum number of combos allowed per input task.
#define MUX_INPUT_COMBO_COUNT 32

// Configuration for a multi-input combo.
typedef struct {
    // Bitmask of input types. Every input in this mask must be pressed for the combo to activate.
    // (The combo can activate even when inputs _not_ in the mask are _also_ pressed. This allows
    // hotkeys to function in cases like pressing volume while holding the D-pad in a game.)
    uint64_t type_mask;

    // Callback functions for combo. Fired in sequence: one press, zero or more holds, one release.
    //
    // Handlers may be NULL, in which case that input will be ignored.
    mux_input_handler press_handler;
    mux_input_handler hold_handler;
    mux_input_handler release_handler;
} mux_input_combo;

// Configuration for the muOS input subsystem.
typedef struct {
    // File descriptors for the underlying evdev devices.
    int general_fd;
    int power_fd;
    int volume_fd;
    int extra_fd;

    // If nonzero, the longest delay allowed without an input before the idle_handler is called.
    // (The idle_handler may still be called more frequently at times.)
    int max_idle_ms;

    // Whether to swap the A/B/X/Y buttons. False is the Japanese layout (A on the right) and true
    // is the Western layout (A on the bottom).
    bool swap_btn;

    // Whether to swap the up/down and left/right axes on the D-pad and sticks.
    bool swap_axis;

    // Whether to use the left or right stick for navigation (direction -> D-pad, click -> A button).
    bool left_stick_nav;
    bool right_stick_nav;

    // Callback functions for inputs. Fired in sequence: one press, zero or more holds, one release.
    //
    // Handlers may be NULL, in which case that input will be ignored.
    mux_input_handler press_handler[MUX_INPUT_COUNT];
    mux_input_handler hold_handler[MUX_INPUT_COUNT];
    mux_input_handler release_handler[MUX_INPUT_COUNT];

    // Generic handler for input press/hold/release events. For programs that want to "go offroad"
    // and handle every input event in a custom way.
    //
    // Most use cases are probably easier to read if they use the individual handlers above instead.
    //
    // May be NULL, in which case no input handler will be invoked.
    mux_input_catchall_handler input_handler;

    // Multi-input combo definitions. At most one combo can be active at once, so order matters.
    //
    // Generally, longer combos should be set before shorter ones. For example, a combo for A+B
    // should come before a combo for just A. (If the combo for A comes first, it will always
    // trigger, and the combo for A+B will be ignored even if B is also pressed.)
    mux_input_combo combo[MUX_INPUT_COMBO_COUNT];

    // Generic handler for combo press/hold/release events. Allows central handling of every combo.
    //
    // Most use cases should use the individual handlers instead the mux_input_combo struct instead.
    //
    // May be NULL, in which case no combo handler will be invoked.
    mux_input_catchall_combo_handler combo_handler;

    // Handler called after each iteration of the event loop, regardless of whether any input fired.
    //
    // May be NULL, in which case no idle handler will be invoked.
    mux_input_handler idle_handler;
} mux_input_options;

// Starts the main input event loop, which runs until terminated by calling mux_input_stop.
void mux_input_task(const mux_input_options *opts);

// Returns the system clock tick at the start of this iteration of the event loop. Provides a common
// reference point for computing hold duration, activity timestamps, etc.
uint32_t mux_input_tick(void);

// Returns whether or not the specified input is currently pressed.
bool mux_input_pressed(mux_input_type type);

// Causes the input task to exit at the start of the next iteration of the event loop (e.g., after
// processing currently pressed or held inputs).
void mux_input_stop(void);

typedef void (*key_event_callback)(struct input_event ev);

void register_key_event_callback(key_event_callback cb);
