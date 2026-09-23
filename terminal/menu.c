#include <stdio.h>
#include <string.h>
#include <common/base/options.h>
#include <common/display/theme.h>
#include <common/ui/common.h>
#include <common/ui/font.h>
#include <module/muxshare.h>
#include "menu.h"

typedef enum {
    ITEM_TERM_FONT_SIZE,
    ITEM_FONT_HINTING,
    ITEM_FG_COLOUR,
    ITEM_BG_COLOUR,
    ITEM_RESET_TERMINAL,
    ITEM_QUIT,
    ITEM_COUNT
} MenuItem;

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *value;
} MenuRow;

static const SDL_Color fg_colours[] = {
    {255, 255, 255, 255}, {255, 200, 0, 255},  {255, 240, 80, 255},  {50, 220, 80, 255},
    {160, 230, 50, 255},  {80, 220, 220, 255}, {140, 180, 255, 255}, {80, 120, 255, 255},
    {255, 140, 200, 255}, {220, 80, 220, 255}, {255, 150, 40, 255},  {200, 200, 200, 255},
};

static const SDL_Color bg_colours[] = {
    {0, 0, 0, 255},    {22, 22, 22, 255}, {35, 35, 35, 255}, {8, 16, 40, 255}, {0, 10, 30, 255}, {8, 28, 16, 255},
    {10, 25, 10, 255}, {20, 8, 36, 255},  {28, 8, 28, 255},  {30, 8, 8, 255},  {8, 28, 28, 255}, {28, 16, 4, 255},
};

static MenuRow rows[ITEM_COUNT];
static MuxtermConfig *term_config = NULL;
static lv_obj_t *dim = NULL;
static int active = 0;
static int selected = 0;
static int quit_requested = 0;
static int reset_requested = 0;
static MenuChange pending_change = MENU_CHANGE_NONE;

static int colour_index(const SDL_Color *colours, const size_t count, const SDL_Color colour) {
    for (size_t i = 0; i < count; i++) {
        if (colours[i].r == colour.r && colours[i].g == colour.g && colours[i].b == colour.b) return (int) i;
    }
    return 0;
}

static const char *fg_name(const int index) {
    const char *names[] = {
        lang.muxterm.colour.white,      lang.muxterm.colour.amber,  lang.muxterm.colour.yellow,
        lang.muxterm.colour.green,      lang.muxterm.colour.lime,   lang.muxterm.colour.cyan,
        lang.muxterm.colour.light_blue, lang.muxterm.colour.blue,   lang.muxterm.colour.pink,
        lang.muxterm.colour.magenta,    lang.muxterm.colour.orange, lang.muxterm.colour.light_grey,
    };
    return names[index];
}

static const char *bg_name(const int index) {
    const char *names[] = {
        lang.muxterm.colour.black,     lang.muxterm.colour.dark_grey,   lang.muxterm.colour.charcoal,
        lang.muxterm.colour.dark_blue, lang.muxterm.colour.navy,        lang.muxterm.colour.dark_green,
        lang.muxterm.colour.forest,    lang.muxterm.colour.dark_purple, lang.muxterm.colour.plum,
        lang.muxterm.colour.dark_red,  lang.muxterm.colour.dark_teal,   lang.muxterm.colour.dark_brown,
    };
    return names[index];
}

static const char *hint_name(const int hint) {
    switch (hint) {
        case 1:
            return lang.muxterm.hint_light;
        case 2:
            return lang.muxterm.hint_mono;
        case 3:
            return lang.generic.none;
        default:
            return lang.generic.normal;
    }
}

static void refresh_values(void) {
    char value[64];
    snprintf(value, sizeof(value), "%d pt", term_config->term_font_size);
    lv_label_set_text(rows[ITEM_TERM_FONT_SIZE].value, value);
    lv_label_set_text(rows[ITEM_FONT_HINTING].value, hint_name(term_config->font_hinting));

    const int fg = colour_index(fg_colours, A_SIZE(fg_colours), term_config->solid_fg);
    const int bg = colour_index(bg_colours, A_SIZE(bg_colours), term_config->solid_bg);
    lv_label_set_text(rows[ITEM_FG_COLOUR].value, fg_name(fg));
    lv_label_set_text(rows[ITEM_BG_COLOUR].value, bg_name(bg));
}

static void refresh_focus(void) {
    for (int i = 0; i < ITEM_COUNT; i++) {
        lv_obj_clear_state(rows[i].panel, LV_STATE_FOCUSED);
        lv_obj_clear_state(rows[i].label, LV_STATE_FOCUSED);
        lv_obj_clear_state(rows[i].value, LV_STATE_FOCUSED);
    }

    lv_obj_add_state(rows[selected].panel, LV_STATE_FOCUSED);
    lv_obj_add_state(rows[selected].label, LV_STATE_FOCUSED);
    lv_obj_add_state(rows[selected].value, LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(rows[selected].panel, LV_ANIM_OFF);

    const int adjustable = selected < ITEM_RESET_TERMINAL;
    nav_show_lr(adjustable);
    nav_show_a(!adjustable, lang.generic.select);
}

static void create_row(const int index, const char *label, const int has_value) {
    rows[index].panel = lv_obj_create(ui_pnl_content);
    lv_obj_set_width(rows[index].panel, theme.misc.content.width);
    apply_theme_list_panel(rows[index].panel);

    rows[index].label = lv_label_create(rows[index].panel);
    apply_theme_option_item_label(&theme, rows[index].label, label, has_value);

    rows[index].value = lv_label_create(rows[index].panel);
    apply_theme_list_value(&theme, rows[index].value, "");
    if (!has_value) lv_obj_add_flag(rows[index].value, LV_OBJ_FLAG_HIDDEN);
}

void menu_show_nav(void) {
    nav_hide_all();
    lv_obj_set_align(ui_pnl_footer, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(ui_pnl_footer, lv_color_hex(theme.footer.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_footer, theme.footer.background_alpha, MU_OBJ_MAIN_DEFAULT);
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.close, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.reset, 0},
                                  {NULL, NULL, 0}});
    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_pnl_footer);
    refresh_focus();
}

void menu_init(MuxtermConfig *cfg) {
    term_config = cfg;

    dim = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(dim);
    lv_obj_set_size(dim, device.mux.width, device.mux.height);
    lv_obj_set_style_bg_color(dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dim, LV_OPA_60, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(dim);

    create_row(ITEM_TERM_FONT_SIZE, lang.muxterm.terminal_font_size, 1);
    create_row(ITEM_FONT_HINTING, lang.muxterm.font_hinting, 1);
    create_row(ITEM_FG_COLOUR, lang.muxterm.foreground_colour, 1);
    create_row(ITEM_BG_COLOUR, lang.muxterm.background_colour, 1);
    create_row(ITEM_RESET_TERMINAL, lang.muxterm.reset_terminal, 0);
    create_row(ITEM_QUIT, lang.muxterm.quit, 0);

    refresh_values();
    refresh_focus();

    header_and_footer_setup();
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
}

void menu_open(void) {
    if (active) return;
    active = 1;
    lv_label_set_text(ui_lbl_title, lang.muxterm.title);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dim);
    lv_obj_move_foreground(ui_pnl_header);
    lv_obj_move_foreground(ui_pnl_content);
    lv_obj_move_foreground(ui_pnl_footer);
    refresh_values();
    menu_show_nav();
}

void menu_close(void) {
    if (!active) return;
    active = 0;
    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    if (config_save_managed(term_config)) toast_message(lang.muxterm.saved, 1024);
}

static void menu_hide(void) {
    if (!active) return;
    active = 0;
    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
}

int menu_is_active(void) {
    return active;
}

void menu_move(const int direction) {
    if (!active || direction == 0) return;
    selected = (selected + ITEM_COUNT + (direction < 0 ? -1 : 1)) % ITEM_COUNT;
    refresh_focus();
}

void menu_adjust(const int direction) {
    if (!active || direction == 0) return;

    switch ((MenuItem) selected) {
        case ITEM_TERM_FONT_SIZE:
            term_config->term_font_size += direction < 0 ? -1 : 1;
            if (term_config->term_font_size < 6) term_config->term_font_size = 6;
            if (term_config->term_font_size > 72) term_config->term_font_size = 72;
            term_config->term_font_size_explicit = 1;
            pending_change = MENU_CHANGE_FONT;
            break;
        case ITEM_FONT_HINTING:
            term_config->font_hinting = (term_config->font_hinting + 4 + (direction < 0 ? -1 : 1)) % 4;
            term_config->font_hinting_explicit = 1;
            pending_change = MENU_CHANGE_FONT;
            break;
        case ITEM_FG_COLOUR: {
            int index = colour_index(fg_colours, A_SIZE(fg_colours), term_config->solid_fg);
            index = (index + (int) A_SIZE(fg_colours) + (direction < 0 ? -1 : 1)) % (int) A_SIZE(fg_colours);
            term_config->solid_fg = fg_colours[index];
            term_config->use_solid_fg = 1;
            term_config->foreground_explicit = 1;
            pending_change = MENU_CHANGE_COLOUR;
            break;
        }
        case ITEM_BG_COLOUR: {
            int index = colour_index(bg_colours, A_SIZE(bg_colours), term_config->solid_bg);
            index = (index + (int) A_SIZE(bg_colours) + (direction < 0 ? -1 : 1)) % (int) A_SIZE(bg_colours);
            term_config->solid_bg = bg_colours[index];
            term_config->use_solid_bg = 1;
            term_config->background_explicit = 1;
            pending_change = MENU_CHANGE_COLOUR;
            break;
        }
        case ITEM_RESET_TERMINAL:
        case ITEM_QUIT:
        default:
            return;
    }

    refresh_values();
}

void menu_select(void) {
    if (!active) return;
    if (selected == ITEM_RESET_TERMINAL) {
        reset_requested = 1;
        menu_hide();
    } else if (selected == ITEM_QUIT) {
        quit_requested = 1;
    }
}

void menu_reset(void) {
    if (!active) return;
    if (!config_reset_managed(term_config, &config)) return;
    pending_change = MENU_CHANGE_FONT;
    refresh_values();
    toast_message(lang.muxterm.reset, 1024);
}

int menu_take_quit(void) {
    const int requested = quit_requested;
    quit_requested = 0;
    return requested;
}

int menu_take_reset(void) {
    const int requested = reset_requested;
    reset_requested = 0;
    return requested;
}

MenuChange menu_take_change(void) {
    const MenuChange change = pending_change;
    pending_change = MENU_CHANGE_NONE;
    return change;
}
