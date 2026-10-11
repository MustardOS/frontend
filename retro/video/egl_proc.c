#include <dlfcn.h>
#include <SDL2/SDL.h>
#include <common/base/function_pointer.h>

typedef void (*egl_proc_t)(void);
typedef egl_proc_t (*egl_get_proc_t)(const char *);

static _Thread_local int egl_proc_busy;

__attribute__((used, visibility("default"))) egl_proc_t eglGetProcAddress(const char *name) {
    static egl_get_proc_t real;
    static int resolved;

    if (!resolved) {
        MUOS_FUNCTION_ASSIGN(real, dlsym(RTLD_NEXT, "eglGetProcAddress"));
        resolved = 1;
    }

    egl_proc_t proc = real ? real(name) : NULL;
    if (proc || !name || egl_proc_busy) return proc;

    egl_proc_busy = 1;
    MUOS_FUNCTION_ASSIGN(proc, SDL_GL_GetProcAddress(name));
    egl_proc_busy = 0;

    return proc;
}
