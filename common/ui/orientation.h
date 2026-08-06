#pragma once

int orientation_introduce(const char *module, const char *title, const char *text);

int orientation_handle_skip(void);

int orientation_should_show(const char *module);

int orientation_pending(void);

void orientation_accept(void);

void orientation_decline(void);

void orientation_mark_shown(const char *module);

int orientation_showing(void);

void orientation_dismiss(void);

void orientation_clear_showing(void);
