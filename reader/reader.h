#pragma once

#include "engine/document.h"
#include "settings.h"

extern reader_settings settings;

int reader_page(void);

int reader_page_count(void);

int reader_reflowable(void);

int reader_plain(void);

const char *reader_title(void);

void reader_go_to(int target);

int reader_has_toc(void);

int reader_toc(const doc_toc_entry **entries);

const char *reader_chapter_title(int page);

void reader_change_size(int delta);

void reader_change_margin(int delta);

void reader_change_spacing(int delta);

void reader_cycle_theme(int delta);

void reader_cycle_rotation(int delta);

void reader_toggle_fit(void);

void reader_cycle_status(int delta);

void reader_toggle_trim(void);

void reader_cycle_sharpness(int delta);

void reader_cycle_filter(int delta);

int reader_step_brightness(int delta);

int reader_step_contrast(int delta);

int reader_step_saturation(int delta);

int reader_step_hue(int delta);

int reader_step_gamma(int delta);

void reader_filter_changed(void);

void reader_filter_preview(void);

void reader_filter_restore(void);

typedef enum {
    reader_save_content,
    reader_save_directory,
    reader_save_global,
    reader_save_session,
    reader_save_discard,
} reader_save_choice;

int reader_settings_dirty(void);

int reader_settings_store(reader_save_choice choice);

void reader_save_state(void);

void reader_header_apply(void);

void reader_corners_init(void);

void reader_corners_update(void);

void reader_cycle_auto_turn(int delta);

int reader_bookmark_count(void);

int reader_bookmark_page(int index);

int reader_bookmark_find(int page);

int reader_bookmark_toggle(void);

int reader_bookmark_thumbnail(int index, char *path, size_t size);

void reader_bookmark_remove(int index);

void reader_set_font(const char *path);

int reader_reversed(void);

void reader_toggle_reversed(void);

void reader_quit(void);
