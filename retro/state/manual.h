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

int manual_is_document(void);

int manual_load_pdf_fit(void);

void manual_save_pdf_fit(int value);

int manual_load_pdf_zoom(int default_step);

void manual_save_pdf_zoom(int value);
