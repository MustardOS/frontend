#pragma once

#include <stdint.h>

int resolve_glyph_size(int16_t runtime_size, int16_t section_size, int auto_px);

void append_glyph_size_hint(char *embed, size_t embed_size, int size);

int glyph_explicit_px(int16_t runtime_size, int16_t section_size);

void apply_glyph_scale(lv_obj_t *img, const char *embed, int box_w, int box_h);

void set_list_glyph_image(lv_obj_t *img, const char *embed);

int get_glyph_path(
    const char *mux_module, const char *glyph_name, char *glyph_image_embed, size_t glyph_image_embed_size
);

void apply_app_glyph(const char *app_folder, const char *glyph_name, lv_obj_t *ui_lbl_item_glyph);

void get_app_grid_glyph(
    const char *app_folder, const char *glyph_name, const char *fallback_name, char *glyph_image_path,
    size_t glyph_image_path_size
);
