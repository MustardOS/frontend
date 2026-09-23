#pragma once

#include <SDL2/SDL.h>

void input_init(int pty_fd);

void input_handle_raw(const SDL_Event *event);

void input_set_context(int readonly, int shell_dead, int visible_rows);

int input_take_quit(void);

void input_write(const char *data, size_t length);
