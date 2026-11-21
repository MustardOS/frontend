#pragma once

#include <stdint.h>

typedef struct doc_engine doc_engine;

typedef struct {
    int width;
    int height;
    int stride;
    int components;
    const unsigned char *samples;
} doc_image;

typedef struct {
    char *title;
    int depth;
    int page;
} doc_toc_entry;

typedef struct {
    int view_w;
    int view_h;
    int fit_page;
    int zoom;
    int trim;
} doc_view;

typedef struct {
    int font_size;
    int margin;
    int line_spacing;
} doc_reflow;

int doc_extension_supported(const char *path);

doc_engine *doc_open(const char *path, int line_spacing);

doc_engine *doc_open_full(const char *path, int line_spacing, const char *font_path, const char *accel_path);

int doc_save_accelerator(doc_engine *doc, const char *accel_path);

void doc_set_cache(doc_engine *doc, int slots);

int doc_prefetch(doc_engine *doc, int page, const doc_view *view);

void doc_close(doc_engine *doc);

int doc_is_reflowable(const doc_engine *doc);

int doc_is_comic(const doc_engine *doc);

int doc_set_line_spacing(doc_engine *doc, int line_spacing);

int doc_set_font(doc_engine *doc, const char *font_path);

int doc_layout(doc_engine *doc, int view_w, int view_h, const doc_reflow *reflow);

int doc_page_count(const doc_engine *doc);

int doc_render(doc_engine *doc, int page, const doc_view *view, doc_image *out);

int64_t doc_bookmark(doc_engine *doc, int page);

int doc_bookmark_page(doc_engine *doc, int64_t mark);

int doc_position(doc_engine *doc, int page);

int doc_position_page(doc_engine *doc, int position);

int doc_has_toc(doc_engine *doc);

int doc_toc(doc_engine *doc, doc_toc_entry **entries);

void doc_toc_free(doc_toc_entry *entries, int count);

int doc_title(doc_engine *doc, char *title, int size);
