#pragma once

#include "options.h"

extern struct theme_config theme;
extern struct mux_config config;
extern struct mux_device device;

struct pt_big {
    lv_obj_t *e;
    uint32_t c;
};

struct pt_small {
    lv_obj_t *e;
    int16_t c;
};

struct footer_glyph {
    uint32_t GLYPH;
    int16_t GLYPH_ALPHA;
    int16_t GLYPH_RECOLOUR_ALPHA;
    uint32_t TEXT;
    int16_t TEXT_ALPHA;
};

struct theme_config {
    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        uint32_t BACKGROUND_GRADIENT_COLOR;
        int16_t BACKGROUND_GRADIENT_START;
        int16_t BACKGROUND_GRADIENT_STOP;
        int16_t BACKGROUND_GRADIENT_DIRECTION;
        int16_t BACKGROUND_GRADIENT_DITHER;
        int16_t BACKGROUND_GRADIENT_BLUR;
    } SYSTEM;

    struct {
        int16_t ANIMATION_DELAY;
        int16_t ANIMATION_REPEAT;
    } ANIMATION;

    struct {
        struct {
            int16_t COUNT;
            int16_t HEIGHT;
            int16_t PANEL;
        } ITEM;
    } MUX;

    struct {
        int16_t HEADER_PAD_TOP;
        int16_t HEADER_PAD_BOTTOM;
        int16_t HEADER_ICON_PAD_TOP;
        int16_t HEADER_ICON_PAD_BOTTOM;
        int16_t FOOTER_PAD_TOP;
        int16_t FOOTER_PAD_BOTTOM;
        int16_t FOOTER_ICON_PAD_TOP;
        int16_t FOOTER_ICON_PAD_BOTTOM;
        int16_t MESSAGE_PAD_TOP;
        int16_t MESSAGE_PAD_BOTTOM;
        int16_t MESSAGE_ICON_PAD_TOP;
        int16_t MESSAGE_ICON_PAD_BOTTOM;
        int16_t LIST_PAD_TOP;
        int16_t LIST_PAD_BOTTOM;
        int16_t LIST_PAD_LEFT;
        int16_t LIST_PAD_RIGHT;
        int16_t LIST_ICON_PAD_TOP;
        int16_t LIST_ICON_PAD_BOTTOM;
    } FONT;

    struct {
        int16_t ALIGN;
        int16_t PADDING_LEFT;
        int16_t PADDING_RIGHT;
        struct {
            uint32_t NORMAL;
            uint32_t ACTIVE;
            uint32_t LOW;
            int16_t NORMAL_ALPHA;
            int16_t ACTIVE_ALPHA;
            int16_t LOW_ALPHA;
        } BATTERY;
        struct {
            uint32_t NORMAL;
            uint32_t ACTIVE;
            int16_t NORMAL_ALPHA;
            int16_t ACTIVE_ALPHA;
        } NETWORK;
        struct {
            uint32_t NORMAL;
            uint32_t ACTIVE;
            int16_t NORMAL_ALPHA;
            int16_t ACTIVE_ALPHA;
        } BLUETOOTH;
    } STATUS;

    struct {
        uint32_t TEXT;
        int16_t ALPHA;
        int16_t ALIGN;
        int16_t PADDING_LEFT;
        int16_t PADDING_RIGHT;
    } DATETIME;

    struct {
        int16_t HEIGHT;
        uint32_t BACKGROUND;
        uint16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } FOOTER;

    struct {
        int16_t HEIGHT;
        uint32_t BACKGROUND;
        uint16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        int16_t TEXT_ALIGN;
        int16_t PADDING_LEFT;
        int16_t PADDING_RIGHT;
    } HEADER;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        uint32_t BORDER;
        int16_t BORDER_ALPHA;
        uint32_t CONTENT;
        uint32_t TITLE;
        int16_t RADIUS;
    } HELP;

    struct {
        uint16_t ALIGNMENT;
        uint16_t SPACING;
        struct footer_glyph LR;
        struct footer_glyph UD;
        struct footer_glyph A;
        struct footer_glyph B;
        struct footer_glyph C;
        struct footer_glyph X;
        struct footer_glyph Y;
        struct footer_glyph Z;
        struct footer_glyph MENU;
    } NAV;

    struct {
        int16_t RADIUS;
        uint32_t BACKGROUND;
        uint32_t BACKGROUND_GRADIENT;
        int16_t BACKGROUND_ALPHA;
        int16_t GRADIENT_START;
        int16_t GRADIENT_STOP;
        int16_t GRADIENT_DIRECTION;
        int16_t BORDER_WIDTH;
        int16_t BORDER_SIDE;
        uint32_t INDICATOR;
        int16_t INDICATOR_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        int16_t GLYPH_PADDING_LEFT;
        int16_t GLYPH_ALPHA;
        uint32_t GLYPH_RECOLOUR;
        int16_t GLYPH_RECOLOUR_ALPHA;
        int16_t LABEL_LONG_MODE;
    } LIST_DEFAULT;

    struct {
        uint32_t BACKGROUND;
        uint32_t BACKGROUND_GRADIENT;
        int16_t BACKGROUND_ALPHA;
        int16_t GRADIENT_START;
        int16_t GRADIENT_STOP;
        int16_t GRADIENT_DIRECTION;
        int16_t BORDER_WIDTH;
        int16_t BORDER_SIDE;
        uint32_t INDICATOR;
        int16_t INDICATOR_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        int16_t GLYPH_ALPHA;
        uint32_t GLYPH_RECOLOUR;
        int16_t GLYPH_RECOLOUR_ALPHA;
    } LIST_FOCUS;

    struct {
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } LIST_DISABLED;

    struct {
        int16_t NAVIGATION_TYPE;
        bool ENABLED;
        int16_t ALIGNMENT;
        int16_t ALIGNMENT_X_OFFSET;
        int16_t ALIGNMENT_Y_OFFSET;
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        int16_t LOCATION_X;
        int16_t LOCATION_Y;
        int16_t COLUMN_COUNT;
        int16_t COLUMN_WIDTH;
        int16_t COLUMN_PADDING;
        int16_t ROW_COUNT;
        int16_t ROW_HEIGHT;
        int16_t ROW_PADDING;
        struct {
            int16_t ALIGNMENT;
            int16_t WIDTH;
            int16_t HEIGHT;
            int16_t OFFSET_X;
            int16_t OFFSET_Y;
            int16_t RADIUS;
            int16_t BORDER_WIDTH;
            uint32_t BORDER;
            int16_t BORDER_ALPHA;
            uint32_t BACKGROUND;
            int16_t BACKGROUND_ALPHA;
            uint32_t BACKGROUND_GRADIENT_COLOR;
            int16_t BACKGROUND_GRADIENT_START;
            int16_t BACKGROUND_GRADIENT_STOP;
            int16_t BACKGROUND_GRADIENT_DIRECTION;
            uint32_t SHADOW;
            int16_t SHADOW_WIDTH;
            int16_t SHADOW_X_OFFSET;
            int16_t SHADOW_Y_OFFSET;
            int16_t LABEL_LONG_MODE;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
            int16_t TEXT_ALIGNMENT;
            int16_t TEXT_LINE_SPACING;
            int16_t TEXT_PADDING_LEFT;
            int16_t TEXT_PADDING_TOP;
            int16_t TEXT_PADDING_RIGHT;
            int16_t TEXT_PADDING_BOTTOM;
        } CURRENT_ITEM_LABEL;
        struct {
            int16_t COLUMN_ALIGN;
            int16_t ROW_ALIGN;
            int16_t WIDTH;
            int16_t HEIGHT;
            int16_t RADIUS;
            int16_t BORDER_WIDTH;
            int16_t IMAGE_PADDING_TOP;
            uint32_t SHADOW;
            int16_t SHADOW_WIDTH;
            int16_t SHADOW_X_OFFSET;
            int16_t SHADOW_Y_OFFSET;
            int16_t TEXT_PADDING_SIDE;
            int16_t TEXT_PADDING_BOTTOM;
            int16_t TEXT_LINE_SPACING;
        } CELL;
        struct {
            uint32_t BACKGROUND;
            int16_t BACKGROUND_ALPHA;
            uint32_t BACKGROUND_GRADIENT_COLOR;
            int16_t BACKGROUND_GRADIENT_START;
            int16_t BACKGROUND_GRADIENT_STOP;
            int16_t BACKGROUND_GRADIENT_DIRECTION;
            uint32_t BORDER;
            int16_t BORDER_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
            int16_t IMAGE_ALPHA;
            uint32_t IMAGE_RECOLOUR;
            int16_t IMAGE_RECOLOUR_ALPHA;
        } CELL_DEFAULT;
        struct {
            uint32_t BACKGROUND;
            int16_t BACKGROUND_ALPHA;
            uint32_t BACKGROUND_GRADIENT_COLOR;
            int16_t BACKGROUND_GRADIENT_START;
            int16_t BACKGROUND_GRADIENT_STOP;
            int16_t BACKGROUND_GRADIENT_DIRECTION;
            uint32_t BORDER;
            int16_t BORDER_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
            int16_t IMAGE_ALPHA;
            uint32_t IMAGE_RECOLOUR;
            int16_t IMAGE_RECOLOUR_ALPHA;
        } CELL_FOCUS;
    } GRID;

    struct {
        int16_t ALPHA;
        int16_t RADIUS;
        uint32_t RECOLOUR;
        int16_t RECOLOUR_ALPHA;
        int16_t PAD_TOP;
        int16_t PAD_BOTTOM;
        int16_t PAD_LEFT;
        int16_t PAD_RIGHT;
    } IMAGE_LIST;

    struct {
        int16_t ALPHA;
        int16_t RADIUS;
        uint32_t RECOLOUR;
        int16_t RECOLOUR_ALPHA;
    } IMAGE_PREVIEW;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        int16_t Y_POS;
    } CHARGER;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        int16_t Y_POS;
    } VERBOSE_BOOT;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        uint32_t BORDER;
        int16_t BORDER_ALPHA;
        int16_t RADIUS;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        uint32_t TEXT_FOCUS;
        int16_t TEXT_FOCUS_ALPHA;
        struct {
            uint32_t BACKGROUND;
            int16_t BACKGROUND_ALPHA;
            uint32_t BACKGROUND_FOCUS;
            int16_t BACKGROUND_FOCUS_ALPHA;
            uint32_t BORDER;
            int16_t BORDER_ALPHA;
            uint32_t BORDER_FOCUS;
            int16_t BORDER_FOCUS_ALPHA;
            int16_t RADIUS;
        } ITEM;
    } OSK;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        uint32_t BORDER;
        int16_t BORDER_ALPHA;
        int16_t RADIUS;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } MESSAGE;

    struct {
        int16_t PANEL_WIDTH;
        int16_t PANEL_HEIGHT;
        uint32_t PANEL_BACKGROUND;
        int16_t PANEL_BACKGROUND_ALPHA;
        uint32_t PANEL_BORDER;
        int16_t PANEL_BORDER_ALPHA;
        int16_t PANEL_BORDER_RADIUS;
        int16_t PROGRESS_WIDTH;
        int16_t PROGRESS_HEIGHT;
        uint32_t PROGRESS_MAIN_BACKGROUND;
        int16_t PROGRESS_MAIN_BACKGROUND_ALPHA;
        uint32_t PROGRESS_ACTIVE_BACKGROUND;
        int16_t PROGRESS_ACTIVE_BACKGROUND_ALPHA;
        int16_t PROGRESS_RADIUS;
        uint32_t ICON;
        int16_t ICON_ALPHA;
        int16_t Y_POS;
    } BAR;

    struct {
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        int16_t RADIUS;
        uint32_t SELECT_TEXT;
        int16_t SELECT_TEXT_ALPHA;
        uint32_t SELECT_BACKGROUND;
        int16_t SELECT_BACKGROUND_ALPHA;
        int16_t SELECT_RADIUS;
        uint32_t BORDER_COLOUR;
        int16_t BORDER_ALPHA;
        int16_t BORDER_RADIUS;
    } ROLL;

    struct {
        uint16_t ALIGNMENT;
        int16_t PADDING_AROUND;
        int16_t PADDING_SIDE;
        int16_t PADDING_TOP;
        uint32_t BORDER_COLOUR;
        int16_t BORDER_ALPHA;
        int16_t BORDER_WIDTH;
        int16_t RADIUS;
        uint32_t BACKGROUND;
        uint32_t BACKGROUND_GRADIENT;
        int16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
        int16_t TEXT_FADE_TIME;
        char TEXT_SEPARATOR[MAX_BUFFER_SIZE];
    } COUNTER;

    struct {
        int16_t STATIC_ALIGNMENT;
        int16_t ANIMATED_BACKGROUND;
        int16_t RANDOM_BACKGROUND;
        int16_t IMAGE_OVERLAY;
        int16_t NAVIGATION_TYPE;
        struct {
            int16_t SIZE_TO_CONTENT;
            int16_t ALIGNMENT;
            int16_t PADDING_LEFT;
            int16_t PADDING_TOP;
            int16_t HEIGHT;
            int16_t WIDTH;
        } CONTENT;
        int16_t ANTIALIASING;
    } MISC;

    struct {
        char FONT_SIZE[MAX_BUFFER_SIZE];
        char FOREGROUND[MAX_BUFFER_SIZE];
        char BACKGROUND[MAX_BUFFER_SIZE];
    } TERMINAL;
};

int load_scheme(const char *theme_base, const char *mux_dimension, const char *file_name, char *scheme, size_t scheme_size);

void load_theme(struct theme_config *theme, struct mux_config *config, struct mux_device *device);

void set_label_long_mode(struct theme_config *theme, lv_obj_t *ui_lblItem);

void apply_text_long_dot(struct theme_config *theme, lv_obj_t *ui_pnlContent, lv_obj_t *ui_lblItem);

void apply_size_to_content(struct theme_config *c_theme, lv_obj_t *ui_pnlContent, lv_obj_t *ui_lblItem,
                           lv_obj_t *ui_lblItemGlyph, const char *item_text);

void apply_theme_list_panel(lv_obj_t *ui_pnlList);

void apply_theme_list_item(struct theme_config *theme, lv_obj_t *ui_lblItem, const char *item_text);

void apply_theme_list_value(struct theme_config *lv_theme, lv_obj_t *ui_lblItemValue, const char *item_text);

void apply_theme_list_drop_down(struct theme_config *d_theme, lv_obj_t *ui_lblItemValue, char *options);

void apply_theme_list_glyph(struct theme_config *theme, lv_obj_t *ui_lblItemGlyph,
                            const char *screen_name, const char *item_glyph);

void apply_pass_theme(lv_obj_t *ui_rolComboOne, lv_obj_t *ui_rolComboTwo, lv_obj_t *ui_rolComboThree,
                      lv_obj_t *ui_rolComboFour, lv_obj_t *ui_rolComboFive, lv_obj_t *ui_rolComboSix);

void init_panel_style(struct theme_config *theme);

void init_item_animation();

void init_item_style(struct theme_config *theme);

void init_glyph_style(struct theme_config *theme);

int get_theme_preview_path(char *base_path, char *base_file_name, 
                           char *image_path, size_t image_path_size, int preview_index);
