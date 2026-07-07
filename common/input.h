#pragma once

#include <stdint.h>
#include <linux/input.h>
#include <SDL2/SDL.h>

#define BIT(n) (UINT64_C(1) << (n))

#define NAV_NONE        ((mux_nav_type) 0x00)
#define NAV_DPAD        ((mux_nav_type) 0x01)
#define NAV_LEFT_STICK  ((mux_nav_type) 0x02)
#define NAV_RIGHT_STICK ((mux_nav_type) 0x04)

// Maximum number of combos allowed per input task.
#define MUX_INPUT_COMBO_COUNT 32

// Every input (button, D-pad, or stick direction) we support.
typedef enum {
    // Gamepad buttons:
    mux_input_a,
    mux_input_b,
    mux_input_c,
    mux_input_x,
    mux_input_y,
    mux_input_z,
    mux_input_l1,
    mux_input_l2,
    mux_input_l3,
    mux_input_r1,
    mux_input_r2,
    mux_input_r3,
    mux_input_select,
    mux_input_start,
    mux_input_switch,

    // D-pad:
    mux_input_dpad_up,
    mux_input_dpad_down,
    mux_input_dpad_left,
    mux_input_dpad_right,

    // Left stick:
    mux_input_ls_up,
    mux_input_ls_down,
    mux_input_ls_left,
    mux_input_ls_right,

    // Right stick:
    mux_input_rs_up,
    mux_input_rs_down,
    mux_input_rs_left,
    mux_input_rs_right,

    // Volume buttons:
    mux_input_vol_up,
    mux_input_vol_down,

    // Function buttons:
    mux_input_menu,

    // System buttons:
    mux_input_power_short,
    mux_input_power_long,

    // Lid (Hall Switch):
    mux_input_lid_open,
    mux_input_lid_close,

    mux_input_count,
} mux_input_type;

// Actions that can be performed on an input: press and release, as well as hold.
typedef enum {
    mux_input_press,
    mux_input_hold,
    mux_input_release,
} mux_input_action;

typedef uint8_t mux_nav_type;

// Callback function invoked in response to a single specific input type and action.
typedef void (*mux_input_handler)(void);

// Callback function invoked in response to any arbitrary input type and action.
typedef void (*mux_input_generic_handler)(mux_input_type, mux_input_action);

// Callback function invoked in response to a multi-input combo.
typedef void (*mux_input_combo_handler)(int, mux_input_action);

typedef void (*mux_idle_handler)(void);

// Callback function invoked once per input poll cycle with the current raw stick positions.
// Each axis is in the SDL int16_t range [-32768, 32767]. +Y points DOWN (screen-space) so
// values can be added directly to coordinates without inversion.
typedef void (*mux_input_analog_handler)(int16_t ls_x, int16_t ls_y, int16_t rs_x, int16_t rs_y);

// Callback invoked for every raw SDL event before normal input processing.
// Use for capture modes that need unfiltered event access (e.g. input remap).
typedef void (*mux_raw_event_handler)(const SDL_Event *ev);

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

    // Whether to swap the up/down and left/right axes on the D-pad and sticks.
    int swap_axis;
    int remap_to_dpad;

    // Whether to use the left or right stick for navigation (direction -> D-pad, click -> A button).
    mux_nav_type nav;

    // Callback functions for inputs. Fired in sequence: one press, zero or more holds, one release.
    //
    // Handlers may be NULL, in which case that input will be ignored.
    mux_input_handler press_handler[mux_input_count];
    mux_input_handler hold_handler[mux_input_count];
    mux_input_handler release_handler[mux_input_count];

    // Alt ("hold") modifier. By default the core treats hold_input (which defaults to
    // mux_input_l2 when left as 0) as a modifier rather than an ordinary input: its raw
    // pressed state is sampled at the start of every poll cycle, before any other input is
    // dispatched, so timing and ordering are deterministic regardless of which buttons are
    // pressed together. While the modifier is held the global hold_call is set and every
    // other input is routed through the alt_* tables below instead of the normal ones. A NULL
    // alt handler means that input does nothing while the modifier is held (i.e. it is halted),
    // which is the common case. Populate alt_* to give buttons a different action while held.
    //
    // Set hold_disabled to keep the modifier as an ordinary input.
    int hold_disabled;
    mux_input_type hold_input;
    mux_input_handler alt_press_handler[mux_input_count];
    mux_input_handler alt_hold_handler[mux_input_count];
    mux_input_handler alt_release_handler[mux_input_count];

    mux_input_generic_handler input_handler;
    mux_input_combo_handler combo_handler;

    mux_idle_handler idle_handler;

    // This is optional.  If set, called once per poll cycle with raw stick positioning
    mux_input_analog_handler analog_handler;

    // This is optional.  If set, called for every raw SDL_Event before normal processing.
    mux_raw_event_handler raw_event_handler;

    mux_input_combo combo[MUX_INPUT_COMBO_COUNT];
    int combo_count;
} mux_input_options;

extern int swap_axis;

// Set by the input core while the alt ("hold") modifier is held.
// This gates actions or swap the on-screen nav items whenever implemented.
extern int hold_call;

void mux_input_reload_mappings(void);

void mux_input_task(const mux_input_options *opts);

void mux_input_open(void);

void mux_input_close(void);

void mux_input_poll(void);

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
