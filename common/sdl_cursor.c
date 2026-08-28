#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Surface SDL_Surface;

typedef int (*show_cursor_fn)(int toggle);
typedef int (*init_fn)(uint32_t flags);
typedef SDL_Window *(*create_window_fn)(const char *title, int x, int y, int w, int h, uint32_t flags);
typedef SDL_Surface *(*set_video_mode_fn)(int width, int height, int bpp, uint32_t flags);

#define SDL_INIT_VIDEO 0x00000020u

static void load_symbol(void *target, const char *name, const size_t target_size) {
    void *symbol = dlsym(RTLD_NEXT, name);
    if (target_size == sizeof(symbol)) memcpy(target, &symbol, sizeof(symbol));
}

static show_cursor_fn real_show_cursor(void) {
    static show_cursor_fn function;
    static int resolved;

    if (!resolved) {
        load_symbol(&function, "SDL_ShowCursor", sizeof(function));
        resolved = 1;
    }

    return function;
}

static void hide_cursor(void) {
    show_cursor_fn function = real_show_cursor();
    if (function) function(0);
}

int SDL_ShowCursor(const int toggle) {
    show_cursor_fn function = real_show_cursor();
    if (!function) return -1;

    return function(toggle < 0 ? toggle : 0);
}

int SDL_Init(const uint32_t flags) {
    static init_fn function;
    static int resolved;

    if (!resolved) {
        load_symbol(&function, "SDL_Init", sizeof(function));
        resolved = 1;
    }

    if (!function) return -1;

    const int result = function(flags);
    if (result == 0 && (flags & SDL_INIT_VIDEO)) hide_cursor();

    return result;
}

int SDL_InitSubSystem(const uint32_t flags) {
    static init_fn function;
    static int resolved;

    if (!resolved) {
        load_symbol(&function, "SDL_InitSubSystem", sizeof(function));
        resolved = 1;
    }

    if (!function) return -1;

    const int result = function(flags);
    if (result == 0 && (flags & SDL_INIT_VIDEO)) hide_cursor();

    return result;
}

SDL_Window *
SDL_CreateWindow(const char *title, const int x, const int y, const int w, const int h, const uint32_t flags) {
    static create_window_fn function;
    static int resolved;

    if (!resolved) {
        load_symbol(&function, "SDL_CreateWindow", sizeof(function));
        resolved = 1;
    }

    if (!function) return NULL;

    SDL_Window *window = function(title, x, y, w, h, flags);
    if (window) hide_cursor();

    return window;
}

SDL_Surface *SDL_SetVideoMode(const int width, const int height, const int bpp, const uint32_t flags) {
    static set_video_mode_fn function;
    static int resolved;

    if (!resolved) {
        load_symbol(&function, "SDL_SetVideoMode", sizeof(function));
        resolved = 1;
    }

    if (!function) return NULL;

    SDL_Surface *surface = function(width, height, bpp, flags);
    if (surface) hide_cursor();

    return surface;
}
