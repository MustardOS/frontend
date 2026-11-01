#include <string.h>
#include <SDL2/SDL.h>
#include <common/platform/input.h>
#include "../core/core.h"
#include "../core/libretro.h"
#include "../core/muxretro.h"
#include "kbm.h"

static uint8_t key_held[RETROK_LAST];
static int keyboard_queried = 0;
static int keys_released_for_pause = 0;

static int mouse_dx = 0;
static int mouse_dy = 0;
static int mouse_wheel_up = 0;
static int mouse_wheel_down = 0;
static int mouse_wheel_left = 0;
static int mouse_wheel_right = 0;
static uint32_t mouse_buttons = 0;

static int frame_dx = 0;
static int frame_dy = 0;
static int frame_wheel_up = 0;
static int frame_wheel_down = 0;
static int frame_wheel_left = 0;
static int frame_wheel_right = 0;

static unsigned retro_key_for(const SDL_Scancode sc) {
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return RETROK_a + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return RETROK_1 + (sc - SDL_SCANCODE_1);
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return RETROK_F1 + (sc - SDL_SCANCODE_F1);
    if (sc >= SDL_SCANCODE_F13 && sc <= SDL_SCANCODE_F15) return RETROK_F13 + (sc - SDL_SCANCODE_F13);
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) return RETROK_KP1 + (sc - SDL_SCANCODE_KP_1);

    switch (sc) {
        case SDL_SCANCODE_0:
            return RETROK_0;
        case SDL_SCANCODE_RETURN:
            return RETROK_RETURN;
        case SDL_SCANCODE_ESCAPE:
            return RETROK_ESCAPE;
        case SDL_SCANCODE_BACKSPACE:
            return RETROK_BACKSPACE;
        case SDL_SCANCODE_TAB:
            return RETROK_TAB;
        case SDL_SCANCODE_SPACE:
            return RETROK_SPACE;
        case SDL_SCANCODE_MINUS:
            return RETROK_MINUS;
        case SDL_SCANCODE_EQUALS:
            return RETROK_EQUALS;
        case SDL_SCANCODE_LEFTBRACKET:
            return RETROK_LEFTBRACKET;
        case SDL_SCANCODE_RIGHTBRACKET:
            return RETROK_RIGHTBRACKET;
        case SDL_SCANCODE_BACKSLASH:
            return RETROK_BACKSLASH;
        case SDL_SCANCODE_SEMICOLON:
            return RETROK_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE:
            return RETROK_QUOTE;
        case SDL_SCANCODE_GRAVE:
            return RETROK_BACKQUOTE;
        case SDL_SCANCODE_COMMA:
            return RETROK_COMMA;
        case SDL_SCANCODE_PERIOD:
            return RETROK_PERIOD;
        case SDL_SCANCODE_SLASH:
            return RETROK_SLASH;
        case SDL_SCANCODE_NONUSBACKSLASH:
            return RETROK_OEM_102;
        case SDL_SCANCODE_CAPSLOCK:
            return RETROK_CAPSLOCK;
        case SDL_SCANCODE_PRINTSCREEN:
            return RETROK_PRINT;
        case SDL_SCANCODE_SCROLLLOCK:
            return RETROK_SCROLLOCK;
        case SDL_SCANCODE_PAUSE:
            return RETROK_PAUSE;
        case SDL_SCANCODE_INSERT:
            return RETROK_INSERT;
        case SDL_SCANCODE_HOME:
            return RETROK_HOME;
        case SDL_SCANCODE_PAGEUP:
            return RETROK_PAGEUP;
        case SDL_SCANCODE_DELETE:
            return RETROK_DELETE;
        case SDL_SCANCODE_END:
            return RETROK_END;
        case SDL_SCANCODE_PAGEDOWN:
            return RETROK_PAGEDOWN;
        case SDL_SCANCODE_RIGHT:
            return RETROK_RIGHT;
        case SDL_SCANCODE_LEFT:
            return RETROK_LEFT;
        case SDL_SCANCODE_DOWN:
            return RETROK_DOWN;
        case SDL_SCANCODE_UP:
            return RETROK_UP;
        case SDL_SCANCODE_NUMLOCKCLEAR:
            return RETROK_NUMLOCK;
        case SDL_SCANCODE_KP_DIVIDE:
            return RETROK_KP_DIVIDE;
        case SDL_SCANCODE_KP_MULTIPLY:
            return RETROK_KP_MULTIPLY;
        case SDL_SCANCODE_KP_MINUS:
            return RETROK_KP_MINUS;
        case SDL_SCANCODE_KP_PLUS:
            return RETROK_KP_PLUS;
        case SDL_SCANCODE_KP_ENTER:
            return RETROK_KP_ENTER;
        case SDL_SCANCODE_KP_0:
            return RETROK_KP0;
        case SDL_SCANCODE_KP_PERIOD:
            return RETROK_KP_PERIOD;
        case SDL_SCANCODE_KP_EQUALS:
            return RETROK_KP_EQUALS;
        case SDL_SCANCODE_APPLICATION:
        case SDL_SCANCODE_MENU:
            return RETROK_MENU;
        case SDL_SCANCODE_SYSREQ:
            return RETROK_SYSREQ;
        case SDL_SCANCODE_LCTRL:
            return RETROK_LCTRL;
        case SDL_SCANCODE_LSHIFT:
            return RETROK_LSHIFT;
        case SDL_SCANCODE_LALT:
            return RETROK_LALT;
        case SDL_SCANCODE_LGUI:
            return RETROK_LSUPER;
        case SDL_SCANCODE_RCTRL:
            return RETROK_RCTRL;
        case SDL_SCANCODE_RSHIFT:
            return RETROK_RSHIFT;
        case SDL_SCANCODE_RALT:
            return RETROK_RALT;
        case SDL_SCANCODE_RGUI:
            return RETROK_RSUPER;
        default:
            return RETROK_UNKNOWN;
    }
}

static uint16_t retro_modifiers(const uint16_t mod) {
    uint16_t out = 0;

    if (mod & KMOD_SHIFT) out |= RETROKMOD_SHIFT;
    if (mod & KMOD_CTRL) out |= RETROKMOD_CTRL;
    if (mod & KMOD_ALT) out |= RETROKMOD_ALT;
    if (mod & KMOD_GUI) out |= RETROKMOD_META;
    if (mod & KMOD_NUM) out |= RETROKMOD_NUMLOCK;
    if (mod & KMOD_CAPS) out |= RETROKMOD_CAPSLOCK;

    return out;
}

static uint32_t key_character(const SDL_Keysym *keysym) {
    const SDL_Keycode sym = keysym->sym;
    if (sym < 32 || sym > 126) return 0;

    const int shift = (keysym->mod & KMOD_SHIFT) != 0;
    const int caps = (keysym->mod & KMOD_CAPS) != 0;

    if (sym >= 'a' && sym <= 'z') return shift != caps ? (uint32_t) (sym - 'a' + 'A') : (uint32_t) sym;

    return (uint32_t) sym;
}

static int keyboard_active(void) {
    return current_core.keyboard_event || keyboard_queried;
}

static int mouse_event_is_touch(const uint32_t which) {
    return which == SDL_TOUCH_MOUSEID;
}

static void send_key(const unsigned key, const int down, const uint32_t character, const uint16_t modifiers) {
    key_held[key] = down ? 1 : 0;
    if (current_core.keyboard_event) current_core.keyboard_event(down ? true : false, key, character, modifiers);
}

static void release_all_keys(void) {
    for (unsigned key = 0; key < RETROK_LAST; key++) {
        if (key_held[key]) send_key(key, 0, 0, 0);
    }
}

static void handle_event(const SDL_Event *ev) {
    switch (ev->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            if (ev->key.repeat) break;

            const unsigned key = retro_key_for(ev->key.keysym.scancode);
            if (key == RETROK_UNKNOWN || key >= RETROK_LAST) break;

            const int down = ev->type == SDL_KEYDOWN;
            if (down && pause_menu_is_active()) break;
            if (!down && !key_held[key]) break;

            send_key(
                key, down, down ? key_character(&ev->key.keysym) : 0, retro_modifiers((uint16_t) ev->key.keysym.mod)
            );
            break;
        }
        case SDL_MOUSEMOTION:
            if (mouse_event_is_touch(ev->motion.which)) break;
            mouse_dx += ev->motion.xrel;
            mouse_dy += ev->motion.yrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (mouse_event_is_touch(ev->button.which) || ev->button.button >= 32) break;
            if (ev->type == SDL_MOUSEBUTTONDOWN) {
                mouse_buttons |= 1u << ev->button.button;
            } else {
                mouse_buttons &= ~(1u << ev->button.button);
            }
            break;
        case SDL_MOUSEWHEEL:
            if (mouse_event_is_touch(ev->wheel.which)) break;
            if (ev->wheel.y > 0) mouse_wheel_up = 1;
            if (ev->wheel.y < 0) mouse_wheel_down = 1;
            if (ev->wheel.x > 0) mouse_wheel_right = 1;
            if (ev->wheel.x < 0) mouse_wheel_left = 1;
            break;
        default:
            break;
    }
}

void kbm_init(void) {
    memset(key_held, 0, sizeof(key_held));
    keyboard_queried = 0;
    keys_released_for_pause = 0;
    mouse_buttons = 0;

    mux_input_set_poll_hook(handle_event);
}

void kbm_begin_frame(void) {
    const int paused = pause_menu_is_active();

    if (paused && !keys_released_for_pause) release_all_keys();
    keys_released_for_pause = paused;

    mux_input_set_key_mapping(paused || !keyboard_active());

    frame_dx = paused ? 0 : mouse_dx;
    frame_dy = paused ? 0 : mouse_dy;
    frame_wheel_up = mouse_wheel_up;
    frame_wheel_down = mouse_wheel_down;
    frame_wheel_left = mouse_wheel_left;
    frame_wheel_right = mouse_wheel_right;

    mouse_dx = 0;
    mouse_dy = 0;
    mouse_wheel_up = 0;
    mouse_wheel_down = 0;
    mouse_wheel_left = 0;
    mouse_wheel_right = 0;
}

int kbm_handles(const unsigned device) {
    return device == RETRO_DEVICE_KEYBOARD || device == RETRO_DEVICE_MOUSE;
}

static int16_t clamp_delta(const int value) {
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t) value;
}

int16_t kbm_state(const unsigned port, const unsigned device, const unsigned index, const unsigned id) {
    (void) index;

    if (port != 0) return 0;

    if (device == RETRO_DEVICE_KEYBOARD) {
        keyboard_queried = 1;
        return id < RETROK_LAST ? key_held[id] : 0;
    }

    if (device != RETRO_DEVICE_MOUSE) return 0;

    switch (id) {
        case RETRO_DEVICE_ID_MOUSE_X:
            return clamp_delta(frame_dx);
        case RETRO_DEVICE_ID_MOUSE_Y:
            return clamp_delta(frame_dy);
        case RETRO_DEVICE_ID_MOUSE_LEFT:
            return (mouse_buttons >> SDL_BUTTON_LEFT) & 1;
        case RETRO_DEVICE_ID_MOUSE_RIGHT:
            return (mouse_buttons >> SDL_BUTTON_RIGHT) & 1;
        case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            return (mouse_buttons >> SDL_BUTTON_MIDDLE) & 1;
        case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            return (mouse_buttons >> SDL_BUTTON_X1) & 1;
        case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            return (mouse_buttons >> SDL_BUTTON_X2) & 1;
        case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            return (int16_t) frame_wheel_up;
        case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            return (int16_t) frame_wheel_down;
        case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            return (int16_t) frame_wheel_right;
        case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            return (int16_t) frame_wheel_left;
        default:
            return 0;
    }
}
