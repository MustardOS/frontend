#pragma once

void manual_init(const char *core_path_arg, const char *content_path);

int manual_is_available(void);

const char *manual_get_path(void);

int manual_load_position(void);

void manual_save_position(int value);

int manual_load_font_size(int default_size);

void manual_save_font_size(int value);

int manual_load_wrap_enabled(void);

void manual_save_wrap_enabled(int value);
