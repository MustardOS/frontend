#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <linux/input.h>
#include <SDL2/SDL.h>

#define BIT(n) (UINT64_C(1) << (n))

#define NAV_NONE         ((mux_nav_type)0x00)
#define NAV_DPAD         ((mux_nav_type)0x01)
#define NAV_LEFT_STICK   ((mux_nav_type)0x02)
#define NAV_RIGHT_STICK  ((mux_nav_type)0x04)

// Maximum number of combos allowed per input task.
#define MUX_INPUT_COMBO_COUNT 16

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
    MUX_INPUT_MENU,

    // System buttons:
    MUX_INPUT_POWER_SHORT,
    MUX_INPUT_POWER_LONG,

    // Lid (Hall Switch):
    MUX_INPUT_LID_OPEN,
    MUX_INPUT_LID_CLOSE,

    MUX_INPUT_COUNT,
} mux_input_type;

// Actions that can be performed on an input: press and release, as well as hold.
typedef enum {
    MUX_INPUT_PRESS,
    MUX_INPUT_HOLD,
    MUX_INPUT_RELEASE,
} mux_input_action;

typedef uint8_t mux_nav_type;

// Callback function invoked in response to a single specific input type and action.
typedef void (*mux_input_handler)(void);

// Callback function invoked in response to any arbitrary input type and action.
typedef void (*mux_input_generic_handler)(mux_input_type, mux_input_action);

// Callback function invoked in response to a multi-input combo.
typedef void (*mux_input_combo_handler)(int, mux_input_action);

typedef void (*mux_idle_handler)(void);

typedef void (*key_event_callback)(struct input_event);

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
    uint32_t max_idle_ms;

    // Whether to swap the A/B/X/Y buttons. False is the Japanese layout (A on the right) and true
    // is the Western layout (A on the bottom).
    int swap_btn;
    // Whether to swap the up/down and left/right axes on the D-pad and sticks.
    int swap_axis;
    int remap_to_dpad;

    // Whether to use the left or right stick for navigation (direction -> D-pad, click -> A button).
    mux_nav_type nav;

    // Callback functions for inputs. Fired in sequence: one press, zero or more holds, one release.
    //
    // Handlers may be NULL, in which case that input will be ignored.
    mux_input_handler press_handler[MUX_INPUT_COUNT];
    mux_input_handler hold_handler[MUX_INPUT_COUNT];
    mux_input_handler release_handler[MUX_INPUT_COUNT];

    mux_input_generic_handler input_handler;
    mux_input_combo_handler combo_handler;

    mux_idle_handler idle_handler;

    mux_input_combo combo[MUX_INPUT_COMBO_COUNT];
    int combo_count;
} mux_input_options;

extern int swap_axis;

void mux_input_task(const mux_input_options *opts);

void mux_input_stop(void);

void mux_input_flush_all(void);

void mux_input_resume(void);

uint32_t mux_input_tick(void);

int mux_input_pressed_any(uint64_t mask);

int mux_input_pressed(mux_input_type mux_type);

void append_combo(mux_input_options *opts, mux_input_combo combo);

mux_nav_type get_sticknav_mask(int sticknav_setting);

void register_key_event_callback(key_event_callback cb);

void ep_wait_wake(void);
