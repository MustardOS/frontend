#pragma once

#include <stdint.h>
#include <SDL2/SDL.h>
#include "../core/libretro.h"

int hw_render_bridge_negotiate(struct retro_hw_render_callback *cb);

int hw_render_bridge_active(void);

void hw_render_bridge_configure(unsigned max_width, unsigned max_height);

uintptr_t hw_render_bridge_get_current_framebuffer(void);

retro_proc_address_t hw_render_bridge_get_proc_address(const char *sym);

void hw_render_bridge_notify_frame(unsigned width, unsigned height);

void hw_render_bridge_context_save(void);

void hw_render_bridge_context_restore(void);

void hw_render_bridge_enter_core_call(void);

void hw_render_bridge_exit_core_call(void);

void hw_render_bridge_draw(SDL_Renderer *renderer, const SDL_Rect *dest_rect);

void hw_render_bridge_shutdown(void);
