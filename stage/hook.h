#pragma once

#include <SDL2/SDL.h>

__attribute__((visibility("hidden")))
extern void (*real_SDL_RenderPresent)(SDL_Renderer *);

__attribute__((visibility("hidden")))
extern void (*real_SDL_GL_SwapWindow)(SDL_Window *);
