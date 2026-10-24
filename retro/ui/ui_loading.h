#pragma once

int loading_splash_show(const char *content_path);

void loading_startup_progress(const char *message, int show_message);

void loading_message_show(const char *message);

void loading_message_hide(void);

void loading_prompt_close(const char *title, const char *description, const char *message, const char *nav_b);
