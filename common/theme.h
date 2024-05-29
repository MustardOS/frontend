#pragma once

extern struct theme_config theme;
extern struct mux_device device;

struct theme_config {
    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
    } SYSTEM;

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
        int16_t LIST_ICON_PAD_TOP;
        int16_t LIST_ICON_PAD_BOTTOM;
    } FONT;

    struct {
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
        int16_t PADDING_LEFT;
    } DATETIME;

    struct {
        uint32_t BACKGROUND;
        uint16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } FOOTER;

    struct {
        uint32_t BACKGROUND;
        uint16_t BACKGROUND_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
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
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } A;
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } B;
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } C;
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } X;
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } Y;
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } Z;
        struct {
            uint32_t GLYPH;
            int16_t GLYPH_ALPHA;
            uint32_t TEXT;
            int16_t TEXT_ALPHA;
        } MENU;
    } NAV;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        int16_t GRADIENT_START;
        int16_t GRADIENT_STOP;
        uint32_t INDICATOR;
        int16_t INDICATOR_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } LIST_DEFAULT;

    struct {
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } LIST_DISABLED;

    struct {
        uint32_t BACKGROUND;
        int16_t BACKGROUND_ALPHA;
        int16_t GRADIENT_START;
        int16_t GRADIENT_STOP;
        uint32_t INDICATOR;
        int16_t INDICATOR_ALPHA;
        uint32_t TEXT;
        int16_t TEXT_ALPHA;
    } LIST_FOCUS;

    struct {
        int16_t RADIUS;
        uint32_t RECOLOUR;
        int16_t RECOLOUR_ALPHA;
    } IMAGE_LIST;

    struct {
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
        uint32_t PANEL_BACKGROUND;
        int16_t PANEL_BACKGROUND_ALPHA;
        uint32_t PANEL_BORDER;
        int16_t PANEL_BORDER_ALPHA;
        int16_t PANEL_BORDER_RADIUS;
        uint32_t PROGRESS_MAIN_BACKGROUND;
        int16_t PROGRESS_MAIN_BACKGROUND_ALPHA;
        uint32_t PROGRESS_ACTIVE_BACKGROUND;
        int16_t PROGRESS_ACTIVE_BACKGROUND_ALPHA;
        int16_t PROGRESS_RADIUS;
        uint32_t ICON;
        int16_t ICON_ALPHA;
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
        int16_t STATIC_ALIGNMENT;
        int16_t ANIMATED_BACKGROUND;
        int16_t IMAGE_OVERLAY;
        int16_t NAVIGATION_TYPE;
        struct {
            int16_t PADDING_LEFT;
            int16_t WIDTH;
        } CONTENT;
    } MISC;
};

void load_theme(struct theme_config *theme, struct mux_device *device, char *mux_name);
