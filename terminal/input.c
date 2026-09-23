#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "input.h"
#include "menu.h"
#include "osk.h"
#include "vt.h"

static int pty_fd = -1;
static int readonly_mode = 0;
static int shell_is_dead = 0;
static int visible_rows = 1;
static int quit_requested = 0;

void input_init(const int fd) {
    pty_fd = fd;
}

void input_set_context(const int readonly, const int shell_dead, const int rows) {
    readonly_mode = readonly;
    shell_is_dead = shell_dead;
    visible_rows = rows > 0 ? rows : 1;
}

void input_write(const char *data, const size_t length) {
    if (pty_fd < 0 || !data || length == 0 || readonly_mode) return;
    const ssize_t written = write(pty_fd, data, length);
    (void) written;
}

static void send_keyboard(const SDL_KeyboardEvent *key) {
    if (menu_is_active()) {
        switch (key->keysym.sym) {
            case SDLK_UP:
                menu_move(-1);
                break;
            case SDLK_DOWN:
                menu_move(1);
                break;
            case SDLK_LEFT:
                menu_adjust(-1);
                break;
            case SDLK_RIGHT:
                menu_adjust(1);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                menu_select();
                break;
            case SDLK_ESCAPE:
                menu_close();
                break;
            default:
                break;
        }
        return;
    }
    if (readonly_mode) return;

    const SDL_Keycode sym = key->keysym.sym;
    const SDL_Keymod mods = SDL_GetModState();
    const int ctrl = (mods & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
    const int shift = (mods & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;
    const int alt = (mods & (KMOD_LALT | KMOD_RALT)) != 0;

    if (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_RIGHT || sym == SDLK_LEFT) {
        char sequence[3] = {'\x1B', vt_cursor_keys_app() ? 'O' : '[', 'A'};
        if (sym == SDLK_DOWN) sequence[2] = 'B';
        if (sym == SDLK_RIGHT) sequence[2] = 'C';
        if (sym == SDLK_LEFT) sequence[2] = 'D';
        input_write(sequence, sizeof(sequence));
        return;
    }

    static const struct {
        SDL_Keycode key;
        const char *sequence;
    } special[] = {
        {SDLK_HOME, "\x1B[H"},      {SDLK_END, "\x1B[F"},     {SDLK_PAGEUP, "\x1B[5~"},
        {SDLK_PAGEDOWN, "\x1B[6~"}, {SDLK_INSERT, "\x1B[2~"}, {SDLK_DELETE, "\x1B[3~"},
    };

    for (size_t i = 0; i < sizeof(special) / sizeof(special[0]); i++) {
        if (sym == special[i].key) {
            input_write(special[i].sequence, strlen(special[i].sequence));
            return;
        }
    }

    static const char *function_keys[] = {
        "\x1BOP",   "\x1BOQ",   "\x1BOR",   "\x1BOS",   "\x1B[15~", "\x1B[17~",
        "\x1B[18~", "\x1B[19~", "\x1B[20~", "\x1B[21~", "\x1B[23~", "\x1B[24~",
    };

    if (sym >= SDLK_F1 && sym <= SDLK_F12) {
        const char *sequence = function_keys[sym - SDLK_F1];
        input_write(sequence, strlen(sequence));
        return;
    }

    if (ctrl) {
        if (sym >= SDLK_a && sym <= SDLK_z) {
            const char control = (char) (sym - SDLK_a + 1);
            input_write(&control, 1);
            return;
        }

        char control = 0;
        int matched = 1;
        switch (sym) {
            case SDLK_SPACE:
            case SDLK_2:
                control = 0x00;
                break;
            case SDLK_LEFTBRACKET:
                control = 0x1B;
                break;
            case SDLK_BACKSLASH:
                control = 0x1C;
                break;
            case SDLK_RIGHTBRACKET:
                control = 0x1D;
                break;
            case SDLK_6:
                control = 0x1E;
                break;
            case SDLK_SLASH:
            case SDLK_7:
                control = 0x1F;
                break;
            default:
                matched = 0;
                break;
        }
        if (matched) {
            input_write(&control, 1);
            return;
        }
    }

    if (alt) input_write("\x1B", 1);

    switch (sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            input_write("\r", 1);
            break;
        case SDLK_BACKSPACE:
            input_write("\x7F", 1);
            break;
        case SDLK_TAB:
            input_write(shift ? "\x1B[Z" : "\t", shift ? 3 : 1);
            break;
        case SDLK_ESCAPE:
            input_write("\x1B", 1);
            break;
        default:
            break;
    }
}

void input_handle_raw(const SDL_Event *event) {
    if (!event) return;

    switch (event->type) {
        case SDL_QUIT:
            quit_requested = 1;
            break;
        case SDL_TEXTINPUT:
            if (!menu_is_active() && !osk_is_visible()) {
                input_write(event->text.text, strlen(event->text.text));
                vt_scroll_set(0);
            }
            break;
        case SDL_KEYDOWN:
            if (event->key.keysym.sym == SDLK_MENU) {
                if (menu_is_active())
                    menu_close();
                else {
                    osk_close();
                    menu_open();
                }
            } else if (osk_is_visible() && !menu_is_active()) {
                break;
            } else if (event->key.keysym.sym == SDLK_ESCAPE && shell_is_dead && SDL_GetModState() == KMOD_NONE) {
                quit_requested = 1;
            } else if (event->key.keysym.sym == SDLK_PAGEUP) {
                vt_scroll_adjust(visible_rows / 2, visible_rows);
            } else if (event->key.keysym.sym == SDLK_PAGEDOWN) {
                vt_scroll_adjust(-(visible_rows / 2), visible_rows);
            } else {
                send_keyboard(&event->key);
            }
            break;
        default:
            break;
    }
}

int input_take_quit(void) {
    const int requested = quit_requested;
    quit_requested = 0;
    return requested;
}
