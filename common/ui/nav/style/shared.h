#pragma once

#include <common/display/theme.h>

#include <common/ui/nav/menu_style.h>

#define STYLE_ITEM_COUNT MENU_ITEM_COUNT

typedef struct {
    double cx, cy;
    int w, h, radius, rotate_x, rotate_y, angle, opacity, glyph;
    const char *label;
} style_slot;

void style_layout_set(const int *values, int mirror, int width, int height);

void style_json_root(const char *fields);

int style_slots_written(void);

int style_items_written(void);

#define SLICE_FOCUS 55
#define SLICE_SLANT 11

int style_number_default(int id);

int style_number_value(int id);

double style_size(void);

double style_spacing(void);

double style_tilt(void);

int style_swell(int base);

int style_position(void);

int style_perch(void);

int style_slice_text(void);

int style_bounce_height(void);

int style_bounce_speed(void);

int style_names(void);

int style_pile(void);

int style_large(void);

void style_json_reset(void);

const char *style_json(void);

void style_json_add(const char *format, ...);

void style_add_slot(int index, const style_slot *slot);

void style_add_item(int index, double x, double y, int w, int h, int radius);

void style_add_glyph_item(int index, double x, double y, int w, int h, int radius, int glyph, const char *label);

void style_add_circles(const double *centre_x, const double *centre_y, int diameter, int focus_zoom);

void style_set_cells(
    struct theme_config *theme, int rows, int columns, int column_width, int row_height, int cell, int x, int y
);

void style_solid_cells(struct theme_config *theme);

void style_hide_name_bar(struct theme_config *theme);

void style_show_cell_text(struct theme_config *theme);

void style_place_label(struct theme_config *theme, double x, double y, double label_width, int width, int height);

int style_content_bottom(const struct theme_config *theme, int height);
