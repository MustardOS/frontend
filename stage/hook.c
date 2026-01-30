#include <dlfcn.h>
#include "hook.h"
#include "../common/log.h"

#define RENDER_SDL "SDL_RenderPresent"
#define RENDER_GL  "SDL_GL_SwapWindow"

void (*real_SDL_RenderPresent)(SDL_Renderer *) = NULL;

void (*real_SDL_GL_SwapWindow)(SDL_Window *) = NULL;

__attribute__((constructor))
static void resolve_symbols(void) {
    real_SDL_RenderPresent = dlsym(RTLD_NEXT, RENDER_SDL);
    real_SDL_GL_SwapWindow = dlsym(RTLD_NEXT, RENDER_GL);

    if (!real_SDL_RenderPresent) LOG_ERROR("stage", "Failed to hook to renderer: " RENDER_SDL);
    if (!real_SDL_GL_SwapWindow) LOG_ERROR("stage", "Failed to hook to renderer: " RENDER_GL);
}
