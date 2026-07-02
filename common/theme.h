#pragma once

#include "../lvgl/lvgl.h"
#include "options.h"

enum { theme_compat = 8 };

enum time_type { time_12_h, time_24_h };

extern const char *theme_base;
extern char *theme_back_compat[];

extern struct theme_config theme;
extern struct mux_config config;
extern struct mux_device device;

const char *get_theme_base(void);

struct pt_big {
    lv_obj_t *e;
    uint32_t c;
};

struct pt_small {
    lv_obj_t *e;
    int16_t c;
};

struct footer_glyph {
    uint32_t glyph;
    int16_t glyph_alpha;
    int16_t glyph_recolour_alpha;
    uint32_t text;
    int16_t text_alpha;
};

struct theme_config {
    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t background_gradient_color;
        int16_t background_gradient_start;
        int16_t background_gradient_stop;
        int16_t background_gradient_direction;
        int16_t background_gradient_dither;
        int16_t background_gradient_blur;
    } system;

    struct {
        int16_t animation_delay;
        int16_t animation_repeat;
        int16_t animation_foreground;
        int16_t animation_position;
        int16_t animation_alpha;
    } animation;

    struct {
        struct {
            int16_t count;
            int16_t height;
            int16_t panel;
        } item;
    } mux;

    struct {
        int16_t font_list_size;
        int16_t font_header_size;
        int16_t font_footer_size;
        int16_t font_panel_size;
        int16_t header_pad_top;
        int16_t header_pad_bottom;
        int16_t header_icon_pad_top;
        int16_t header_icon_pad_bottom;
        int16_t footer_pad_top;
        int16_t footer_pad_bottom;
        int16_t footer_icon_pad_top;
        int16_t footer_icon_pad_bottom;
        int16_t message_pad_top;
        int16_t message_pad_bottom;
        int16_t message_icon_pad_top;
        int16_t message_icon_pad_bottom;
        int16_t list_pad_top;
        int16_t list_pad_bottom;
        int16_t list_pad_left;
        int16_t list_pad_right;
        int16_t list_icon_pad_top;
        int16_t list_icon_pad_bottom;
    } font;

    struct {
        int16_t align;
        int16_t padding_left;
        int16_t padding_right;
        struct {
            uint32_t normal;
            uint32_t active;
            uint32_t low;
            int16_t normal_alpha;
            int16_t active_alpha;
            int16_t low_alpha;
        } battery;
        struct {
            uint32_t normal;
            uint32_t active;
            int16_t normal_alpha;
            int16_t active_alpha;
        } network;
        struct {
            uint32_t normal;
            uint32_t active;
            int16_t normal_alpha;
            int16_t active_alpha;
        } bluetooth;
    } status;

    struct {
        uint32_t text;
        int16_t alpha;
        int16_t align;
        int16_t padding_left;
        int16_t padding_right;
    } datetime;

    struct {
        int16_t height;
        uint32_t background;
        uint16_t background_alpha;
        uint32_t text;
        int16_t text_alpha;
    } footer;

    struct {
        int16_t height;
        uint32_t background;
        uint16_t background_alpha;
        uint32_t text;
        int16_t text_alpha;
        int16_t text_align;
        int16_t padding_left;
        int16_t padding_right;
    } header;

    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t border;
        int16_t border_alpha;
        uint32_t content;
        uint32_t title;
        int16_t radius;
    } help;

    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t border;
        int16_t border_alpha;
        uint32_t title;
        uint32_t content;
        uint32_t option;
        int16_t dim_alpha;
        uint32_t selection;
        int16_t selection_alpha;
        struct {
            int16_t main;
            int16_t selected;
        } radius;
        uint32_t shadow_colour;
        int16_t shadow_alpha;
        int16_t shadow_x_offset;
        int16_t shadow_y_offset;
        uint32_t shadow_colour_focus;
        int16_t shadow_alpha_focus;
        int16_t shadow_x_offset_focus;
        int16_t shadow_y_offset_focus;
    } dialogue;

    struct {
        uint16_t alignment;
        uint16_t spacing;
        struct footer_glyph lr;
        struct footer_glyph ud;
        struct footer_glyph a;
        struct footer_glyph b;
        struct footer_glyph c;
        struct footer_glyph x;
        struct footer_glyph y;
        struct footer_glyph z;
        struct footer_glyph menu;
    } nav;

    struct {
        int16_t radius;
        uint32_t background;
        uint32_t background_gradient;
        int16_t background_alpha;
        int16_t gradient_start;
        int16_t gradient_stop;
        int16_t gradient_direction;
        int16_t border_width;
        int16_t border_side;
        uint32_t indicator;
        int16_t indicator_alpha;
        uint32_t text;
        int16_t text_alpha;
        int16_t glyph_padding_left;
        int16_t glyph_alpha;
        uint32_t glyph_recolour;
        int16_t glyph_recolour_alpha;
        int16_t label_long_mode;
        uint32_t shadow_colour;
        int16_t shadow_alpha;
        int16_t shadow_x_offset;
        int16_t shadow_y_offset;
    } list_default;

    struct {
        uint32_t background;
        uint32_t background_gradient;
        int16_t background_alpha;
        int16_t gradient_start;
        int16_t gradient_stop;
        int16_t gradient_direction;
        int16_t border_width;
        int16_t border_side;
        uint32_t indicator;
        int16_t indicator_alpha;
        uint32_t text;
        int16_t text_alpha;
        int16_t glyph_alpha;
        uint32_t glyph_recolour;
        int16_t glyph_recolour_alpha;
        uint32_t shadow_colour;
        int16_t shadow_alpha;
        int16_t shadow_x_offset;
        int16_t shadow_y_offset;
    } list_focus;

    struct {
        uint32_t text;
        int16_t text_alpha;
    } list_disabled;

    struct {
        int16_t navigation_type;
        int enabled;
        int16_t alignment;
        int16_t alignment_x_offset;
        int16_t alignment_y_offset;
        uint32_t background;
        int16_t background_alpha;
        int16_t location_x;
        int16_t location_y;
        int16_t column_count;
        int16_t column_width;
        int16_t column_padding;
        int16_t row_count;
        int16_t row_height;
        int16_t row_padding;
        struct {
            int16_t alignment;
            int16_t width;
            int16_t height;
            int16_t offset_x;
            int16_t offset_y;
            int16_t radius;
            int16_t border_width;
            uint32_t border;
            int16_t border_alpha;
            uint32_t background;
            int16_t background_alpha;
            uint32_t background_gradient_color;
            int16_t background_gradient_start;
            int16_t background_gradient_stop;
            int16_t background_gradient_direction;
            uint32_t shadow;
            int16_t shadow_width;
            int16_t shadow_x_offset;
            int16_t shadow_y_offset;
            int16_t label_long_mode;
            uint32_t text;
            int16_t text_alpha;
            int16_t text_alignment;
            int16_t text_line_spacing;
            int16_t text_padding_left;
            int16_t text_padding_top;
            int16_t text_padding_right;
            int16_t text_padding_bottom;
        } current_item_label;
        struct {
            int16_t column_align;
            int16_t row_align;
            int16_t width;
            int16_t height;
            int16_t radius;
            int16_t border_width;
            int16_t image_padding_top;
            uint32_t shadow;
            int16_t shadow_width;
            int16_t shadow_x_offset;
            int16_t shadow_y_offset;
            int16_t text_padding_side;
            int16_t text_padding_bottom;
            int16_t text_line_spacing;
        } cell;
        struct {
            uint32_t background;
            int16_t background_alpha;
            uint32_t background_gradient_color;
            int16_t background_gradient_start;
            int16_t background_gradient_stop;
            int16_t background_gradient_direction;
            uint32_t border;
            int16_t border_alpha;
            uint32_t text;
            int16_t text_alpha;
            int16_t image_alpha;
            uint32_t image_recolour;
            int16_t image_recolour_alpha;
        } cell_default;
        struct {
            uint32_t background;
            int16_t background_alpha;
            uint32_t background_gradient_color;
            int16_t background_gradient_start;
            int16_t background_gradient_stop;
            int16_t background_gradient_direction;
            uint32_t border;
            int16_t border_alpha;
            uint32_t text;
            int16_t text_alpha;
            int16_t image_alpha;
            uint32_t image_recolour;
            int16_t image_recolour_alpha;
        } cell_focus;
    } grid;

    struct {
        int16_t alpha;
        int16_t radius;
        uint32_t recolour;
        int16_t recolour_alpha;
        int16_t pad_top;
        int16_t pad_bottom;
        int16_t pad_left;
        int16_t pad_right;
    } image_list;

    struct {
        int16_t alpha;
        int16_t radius;
        uint32_t recolour;
        int16_t recolour_alpha;
    } image_preview;

    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t text;
        int16_t text_alpha;
        int16_t y_pos;
    } charger;

    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t text;
        int16_t text_alpha;
        int16_t y_pos;
    } verbose_boot;

    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t border;
        int16_t border_alpha;
        int16_t radius;
        uint32_t text;
        int16_t text_alpha;
        uint32_t text_focus;
        int16_t text_focus_alpha;
        struct {
            uint32_t background;
            int16_t background_alpha;
            uint32_t background_focus;
            int16_t background_focus_alpha;
            uint32_t border;
            int16_t border_alpha;
            uint32_t border_focus;
            int16_t border_focus_alpha;
            int16_t radius;
            uint32_t shadow_colour;
            int16_t shadow_alpha;
            int16_t shadow_x_offset;
            int16_t shadow_y_offset;
            uint32_t shadow_colour_focus;
            int16_t shadow_alpha_focus;
            int16_t shadow_x_offset_focus;
            int16_t shadow_y_offset_focus;
        } item;
    } osk;

    struct {
        uint32_t background;
        int16_t background_alpha;
        uint32_t border;
        int16_t border_alpha;
        int16_t radius;
        uint32_t text;
        int16_t text_alpha;
    } message;

    struct {
        int16_t panel_width;
        int16_t panel_height;
        uint32_t panel_background;
        int16_t panel_background_alpha;
        uint32_t panel_border;
        int16_t panel_border_alpha;
        int16_t panel_border_radius;
        int16_t progress_width;
        int16_t progress_height;
        uint32_t progress_main_background;
        int16_t progress_main_background_alpha;
        uint32_t progress_active_background;
        int16_t progress_active_background_alpha;
        int16_t progress_radius;
        uint32_t icon;
        int16_t icon_alpha;
        int16_t y_pos;
    } bar;

    struct {
        uint32_t text;
        int16_t text_alpha;
        uint32_t background;
        int16_t background_alpha;
        int16_t radius;
        uint32_t select_text;
        int16_t select_text_alpha;
        uint32_t select_background;
        int16_t select_background_alpha;
        int16_t select_radius;
        uint32_t border_colour;
        int16_t border_alpha;
        int16_t border_radius;
    } roll;

    struct {
        uint16_t alignment;
        int16_t padding_around;
        int16_t padding_side;
        int16_t padding_top;
        uint32_t border_colour;
        int16_t border_alpha;
        int16_t border_width;
        int16_t radius;
        uint32_t background;
        uint32_t background_gradient;
        int16_t background_alpha;
        uint32_t text;
        int16_t text_alpha;
        int16_t text_fade_time;
        char text_separator[MAX_BUFFER_SIZE];
    } counter;

    struct {
        int16_t static_alignment;
        int16_t animated_background;
        int16_t random_background;
        int16_t image_overlay;
        int16_t navigation_type;
        struct {
            int16_t size_to_content;
            int16_t alignment;
            int16_t padding_left;
            int16_t padding_top;
            int16_t height;
            int16_t width;
        } content;
        int16_t antialiasing;
        int16_t label_width;
    } misc;

    struct {
        int16_t list;
        int16_t footer;
        int16_t header;
        int16_t grid;
    } glyph;

    struct {
        char font_size[MAX_BUFFER_SIZE];
        char font_hint[MAX_BUFFER_SIZE];
        char foreground[MAX_BUFFER_SIZE];
        char background[MAX_BUFFER_SIZE];
    } terminal;

    struct {
        int16_t texture_blend_mode;
        int16_t draw_blend_mode;
        struct {
            float offset_x;
            float offset_y;
        } render;
        struct {
            int16_t r;
            int16_t g;
            int16_t b;
        } solid;
    } sdl;
};

int load_scheme(const char *theme_base, const char *mux_dim, const char *file_name, char *scheme, size_t scheme_size);

void load_theme(struct theme_config *theme, const struct mux_config *config, struct mux_device *device);

void set_label_long_mode(const struct theme_config *theme, lv_obj_t *ui_lbl_item, int scroll_mode);

void apply_text_long_dot(const struct theme_config *theme, lv_obj_t *ui_lbl_item);

void apply_size_to_content(
    const struct theme_config *c_theme, const lv_obj_t *ui_pnl_content, lv_obj_t *ui_lbl_item,
    lv_obj_t *ui_lbl_item_glyph, const char *item_text
);

void apply_theme_list_panel(lv_obj_t *ui_pnl_list);

void apply_theme_list_item(const struct theme_config *theme, lv_obj_t *ui_lbl_item, const char *item_text);

void apply_theme_option_item_label(const struct theme_config *theme, lv_obj_t *ui_lbl_item, const char *item_text);

void apply_option_label_long_dot(lv_obj_t *ui_lbl_item);

void set_option_label_scroll_mode(lv_obj_t *ui_lbl_item);

void apply_theme_list_value(const struct theme_config *lv_theme, lv_obj_t *ui_lbl_item_value, const char *item_text);

void apply_theme_list_drop_down(const struct theme_config *d_theme, lv_obj_t *ui_lbl_item_value, const char *options);

void apply_theme_list_glyph(
    const struct theme_config *theme, lv_obj_t *ui_lbl_item_glyph, const char *screen_name, const char *item_glyph
);

void apply_pass_theme(
    lv_obj_t *ui_rol_combo_one, lv_obj_t *ui_rol_combo_two, lv_obj_t *ui_rol_combo_three, lv_obj_t *ui_rol_combo_four,
    lv_obj_t *ui_rol_combo_five, lv_obj_t *ui_rol_combo_six
);

void init_panel_style(const struct theme_config *theme);

void init_item_animation();

void init_item_style(struct theme_config *theme);

void init_glyph_style(const struct theme_config *theme);

int get_theme_preview_path(
    char *base_path, char *base_file_name, char *image_path, size_t image_path_size, int preview_index
);
