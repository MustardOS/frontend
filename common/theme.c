#include <stdio.h>
#include "init.h"
#include "options.h"
#include "theme.h"
#include "config.h"
#include "device.h"
#include "log.h"
#include "mini/mini.h"
#include "fileio.h"
#include "strutil.h"
#include "ini.h"
#include "ui/glyph.h"

const char *theme_base;
char *theme_back_compat[theme_compat];

static void compat_theme_init(void) {
    theme_back_compat[0] = config.system.version;
    theme_back_compat[1] = "2601.1_FUNKY_JACARANDA";
    theme_back_compat[2] = "2601.0_JACARANDA";
    theme_back_compat[3] = "2508.4_LOOSE_GOOSE";
    theme_back_compat[4] = "2508.3_GOLDEN_GOOSE";
    theme_back_compat[5] = "2508.2_SILLY_GOOSE";
    theme_back_compat[6] = "2508.1_CANADA_GOOSE";
    theme_back_compat[7] = "2508.0_GOOSE";
}

const char *get_theme_base(void) {
    compat_theme_init();

    static char storage_theme[MAX_BUFFER_SIZE];
    char theme_version_file[MAX_BUFFER_SIZE];

    const char *paths[2];
    int path_count = 0;

    if (!config.boot.factory_reset) {
        snprintf(storage_theme, sizeof(storage_theme), RUN_STORAGE_PATH "theme/%s", config.theme.active);
        paths[path_count++] = storage_theme;
    }

    paths[path_count++] = INTERNAL_THEME;

    for (int p = 0; p < path_count; p++) {
        snprintf(theme_version_file, sizeof(theme_version_file), "%s/version.txt", paths[p]);
        if (!file_exist(theme_version_file)) continue;

        char *theme_version = read_line_char_from(theme_version_file, 1);
        if (!theme_version || !*theme_version) continue;

        for (size_t i = 0; i < theme_compat; i++) {
            const char *compat = theme_back_compat[i];
            if (!compat) continue;
            if (str_startswith(compat, theme_version)) return paths[p];
        }

        LOG_WARN(mux_module, "Incompatible Theme Detected (%s): %s", paths[p], theme_version);
        break;
    }

    LOG_WARN(mux_module, "Missing or incompatible theme version, falling back to internal");
    return INTERNAL_THEME;
}

static lv_style_t style_list_panel_default;
static lv_style_t style_list_panel_focused;

static lv_anim_t style_list_item_animation;
static lv_style_t style_list_item_default;
static lv_style_t style_list_item_focused;

static lv_style_t style_list_glyph_default;
static lv_style_t style_list_glyph_focused;

int load_scheme(
    const char *theme_base, const char *mux_dim, const char *file_name, char *scheme, const size_t scheme_size
) {
    return (snprintf(scheme, scheme_size, "%s/%sscheme/%s.ini", theme_base, mux_dim, file_name) && file_exist(scheme))
           || (snprintf(scheme, scheme_size, "%s/scheme/%s.ini", theme_base, file_name) && file_exist(scheme));
}

void init_theme_config(struct theme_config *theme, const struct mux_device *device) {
    theme->system.background = 0xC69200;
    theme->system.background_alpha = 255;
    theme->system.background_gradient_color = 0xC69200;
    theme->system.background_gradient_start = 0;
    theme->system.background_gradient_stop = 200;
    theme->system.background_gradient_direction = 0;
    theme->system.background_gradient_dither = 0;
    theme->system.background_gradient_blur = 0;

    theme->animation.animation_delay = 100;
    theme->animation.animation_repeat = 0;
    theme->animation.animation_foreground = 0;
    theme->animation.animation_position = 4;
    theme->animation.animation_alpha = 255;

    theme->font.font_list_size = 0;
    theme->font.font_header_size = 0;
    theme->font.font_footer_size = 0;
    theme->font.font_panel_size = 0;
    theme->font.header_pad_top = 0;
    theme->font.header_pad_bottom = 0;
    theme->font.header_icon_pad_top = 0;
    theme->font.header_icon_pad_bottom = 0;
    theme->font.footer_pad_top = 0;
    theme->font.footer_pad_bottom = 0;
    theme->font.footer_icon_pad_top = 0;
    theme->font.footer_icon_pad_bottom = 0;
    theme->font.message_pad_top = 0;
    theme->font.message_pad_bottom = 0;
    theme->font.message_icon_pad_top = 0;
    theme->font.message_icon_pad_bottom = 0;
    theme->font.list_pad_top = 0;
    theme->font.list_pad_bottom = 0;
    theme->font.list_pad_left = 32;
    theme->font.list_pad_right = 6;
    theme->font.list_icon_pad_top = 0;
    theme->font.list_icon_pad_bottom = 0;

    theme->status.align = 1;
    theme->status.padding_left = 0;
    theme->status.padding_right = 0;

    theme->status.battery.normal = 0xFFFFFF;
    theme->status.battery.active = 0x85F718;
    theme->status.battery.low = 0xD35C54;
    theme->status.battery.normal_alpha = 255;
    theme->status.battery.active_alpha = 255;
    theme->status.battery.low_alpha = 255;

    theme->status.network.normal = 0x282525;
    theme->status.network.active = 0xF7E318;
    theme->status.network.normal_alpha = 255;
    theme->status.network.active_alpha = 255;

    theme->status.bluetooth.normal = 0x282525;
    theme->status.bluetooth.active = 0xF7E318;
    theme->status.bluetooth.normal_alpha = 255;
    theme->status.bluetooth.active_alpha = 255;

    theme->datetime.text = 0xFFFFFF;
    theme->datetime.alpha = 255;
    theme->datetime.align = 1;
    theme->datetime.padding_left = 0;
    theme->datetime.padding_right = 0;

    theme->footer.height = 42;
    theme->footer.background = 0x100808;
    theme->footer.background_alpha = 255;
    theme->footer.text = 0xFFFFFF;
    theme->footer.text_alpha = 255;

    theme->header.height = 42;
    theme->header.background = 0x100808;
    theme->header.background_alpha = 255;
    theme->header.text = 0xFFFFFF;
    theme->header.text_alpha = 255;
    theme->header.text_align = 2;
    theme->header.padding_left = 0;
    theme->header.padding_right = 0;

    theme->help.background = 0x282525;
    theme->help.background_alpha = 255;
    theme->help.border = 0x100808;
    theme->help.border_alpha = 255;
    theme->help.content = 0xA5B2B5;
    theme->help.title = 0xF7E318;
    theme->help.radius = 3;

    theme->dialogue.background = 0x282525;
    theme->dialogue.background_alpha = 255;
    theme->dialogue.border = 0x100808;
    theme->dialogue.border_alpha = 255;
    theme->dialogue.title = 0xF7E318;
    theme->dialogue.content = 0xA5B2B5;
    theme->dialogue.option = 0x282525;
    theme->dialogue.dim_alpha = 128;
    theme->dialogue.selection = 0xF7E318;
    theme->dialogue.selection_alpha = 255;
    theme->dialogue.radius.main = 3;
    theme->dialogue.radius.selected = 0;
    theme->dialogue.shadow_colour = 0x000000;
    theme->dialogue.shadow_alpha = 255;
    theme->dialogue.shadow_x_offset = 2;
    theme->dialogue.shadow_y_offset = 2;
    theme->dialogue.shadow_colour_focus = 0x222222;
    theme->dialogue.shadow_alpha_focus = 96;
    theme->dialogue.shadow_x_offset_focus = 2;
    theme->dialogue.shadow_y_offset_focus = 2;

    theme->nav.alignment = 255;
    theme->nav.spacing = 5;

    theme->nav.lr.glyph = 0xF7E318;
    theme->nav.lr.glyph_alpha = 255;
    theme->nav.lr.glyph_recolour_alpha = 255;
    theme->nav.lr.text = 0xFFFFFF;
    theme->nav.lr.text_alpha = 255;

    theme->nav.ud.glyph = 0xF7E318;
    theme->nav.ud.glyph_alpha = 255;
    theme->nav.ud.glyph_recolour_alpha = 255;
    theme->nav.ud.text = 0xFFFFFF;
    theme->nav.ud.text_alpha = 255;

    theme->nav.a.glyph = 0xF7E318;
    theme->nav.a.glyph_alpha = 255;
    theme->nav.a.glyph_recolour_alpha = 255;
    theme->nav.a.text = 0xFFFFFF;
    theme->nav.a.text_alpha = 255;

    theme->nav.b.glyph = 0xF7E318;
    theme->nav.b.glyph_alpha = 255;
    theme->nav.b.glyph_recolour_alpha = 255;
    theme->nav.b.text = 0xFFFFFF;
    theme->nav.b.text_alpha = 255;

    theme->nav.c.glyph = 0xF7E318;
    theme->nav.c.glyph_alpha = 255;
    theme->nav.c.glyph_recolour_alpha = 255;
    theme->nav.c.text = 0xFFFFFF;
    theme->nav.c.text_alpha = 255;

    theme->nav.x.glyph = 0xF7E318;
    theme->nav.x.glyph_alpha = 255;
    theme->nav.x.glyph_recolour_alpha = 255;
    theme->nav.x.text = 0xFFFFFF;
    theme->nav.x.text_alpha = 255;

    theme->nav.y.glyph = 0xF7E318;
    theme->nav.y.glyph_alpha = 255;
    theme->nav.y.glyph_recolour_alpha = 255;
    theme->nav.y.text = 0xFFFFFF;
    theme->nav.y.text_alpha = 255;

    theme->nav.z.glyph = 0xF7E318;
    theme->nav.z.glyph_alpha = 255;
    theme->nav.z.glyph_recolour_alpha = 255;
    theme->nav.z.text = 0xFFFFFF;
    theme->nav.z.text_alpha = 255;

    theme->nav.menu.glyph = 0xF7E318;
    theme->nav.menu.glyph_alpha = 255;
    theme->nav.menu.glyph_recolour_alpha = 255;
    theme->nav.menu.text = 0xFFFFFF;
    theme->nav.menu.text_alpha = 255;

    theme->grid.navigation_type = 2;
    theme->grid.background = 0xC69200;
    theme->grid.background_alpha = 0;
    theme->grid.alignment = 0;
    theme->grid.alignment_x_offset = 0;
    theme->grid.alignment_y_offset = 0;
    theme->grid.location_x = 0;
    theme->grid.location_y = 0;
    theme->grid.column_count = 0;
    theme->grid.column_padding = 0;
    theme->grid.row_count = 0;
    theme->grid.row_padding = 0;
    theme->grid.enabled = 0;

    theme->grid.current_item_label.alignment = LV_ALIGN_BOTTOM_MID;
    theme->grid.current_item_label.width = (int16_t) (device->mux.width * .8);
    theme->grid.current_item_label.height = 0;
    theme->grid.current_item_label.offset_x = 0;
    theme->grid.current_item_label.offset_y = (int16_t) -(theme->footer.height * 2);
    theme->grid.current_item_label.radius = 10;
    theme->grid.current_item_label.border_width = 5;
    theme->grid.current_item_label.border = 0xF7E318;
    theme->grid.current_item_label.border_alpha = 0;
    theme->grid.current_item_label.background = 0xF7E318;
    theme->grid.current_item_label.background_alpha = 0;
    theme->grid.current_item_label.background_gradient_color = 0xF7E318;
    theme->grid.current_item_label.background_gradient_start = 255;
    theme->grid.current_item_label.background_gradient_stop = 255;
    theme->grid.current_item_label.background_gradient_direction = 0;
    theme->grid.current_item_label.shadow = 0x000000;
    theme->grid.current_item_label.shadow_width = 0;
    theme->grid.current_item_label.shadow_x_offset = 10;
    theme->grid.current_item_label.shadow_y_offset = 10;
    theme->grid.current_item_label.label_long_mode = LV_LABEL_LONG_WRAP;
    theme->grid.current_item_label.text = 0x100808;
    theme->grid.current_item_label.text_alpha = 0;
    theme->grid.current_item_label.text_alignment = LV_TEXT_ALIGN_CENTER;
    theme->grid.current_item_label.text_line_spacing = 0;
    theme->grid.current_item_label.text_padding_bottom = 0;
    theme->grid.current_item_label.text_padding_left = 0;
    theme->grid.current_item_label.text_padding_right = 0;
    theme->grid.current_item_label.text_padding_top = 0;

    theme->grid.row_height = 0;
    theme->grid.column_width = 0;

    theme->grid.cell.column_align = LV_GRID_ALIGN_CENTER;
    theme->grid.cell.row_align = LV_GRID_ALIGN_CENTER;
    theme->grid.cell.width = 200;
    theme->grid.cell.height = 200;
    theme->grid.cell.radius = 10;
    theme->grid.cell.border_width = 5;
    theme->grid.cell.image_padding_top = 5;
    theme->grid.cell.shadow = 0x000000;
    theme->grid.cell.shadow_width = 0;
    theme->grid.cell.shadow_x_offset = 10;
    theme->grid.cell.shadow_y_offset = 10;
    theme->grid.cell.text_padding_bottom = 5;
    theme->grid.cell.text_padding_side = 5;
    theme->grid.cell.text_line_spacing = 0;

    theme->grid.cell_default.background = 0xC69200;
    theme->grid.cell_default.background_alpha = 255;
    theme->grid.cell_default.background_gradient_color = 0xC69200;
    theme->grid.cell_default.background_gradient_start = 0;
    theme->grid.cell_default.background_gradient_stop = 200;
    theme->grid.cell_default.background_gradient_direction = 0;
    theme->grid.cell_default.border = 0xC69200;
    theme->grid.cell_default.border_alpha = 255;
    theme->grid.cell_default.image_alpha = 255;
    theme->grid.cell_default.image_recolour = 0xFFFFFF;
    theme->grid.cell_default.image_recolour_alpha = 0;
    theme->grid.cell_default.text = 0xFFFFFF;
    theme->grid.cell_default.text_alpha = 255;

    theme->grid.cell_focus.background = 0xF7E318;
    theme->grid.cell_focus.background_alpha = 255;
    theme->grid.cell_focus.background_gradient_color = 0xF7E318;
    theme->grid.cell_focus.background_gradient_start = 0;
    theme->grid.cell_focus.background_gradient_stop = 200;
    theme->grid.cell_focus.background_gradient_direction = 0;
    theme->grid.cell_focus.border = 0xF7E318;
    theme->grid.cell_focus.border_alpha = 255;
    theme->grid.cell_focus.image_alpha = 255;
    theme->grid.cell_focus.image_recolour = 0xFFFFFF;
    theme->grid.cell_focus.image_recolour_alpha = 0;
    theme->grid.cell_focus.text = 0x100808;
    theme->grid.cell_focus.text_alpha = 255;

    theme->list_default.radius = 0;
    theme->list_default.background = 0xC69200;
    theme->list_default.background_alpha = 255;
    theme->list_default.gradient_start = 0;
    theme->list_default.gradient_stop = 200;
    theme->list_default.gradient_direction = 0;
    theme->list_default.border_width = 5;
    theme->list_default.border_side = LV_BORDER_SIDE_LEFT;
    theme->list_default.indicator = 0xA5B2B5;
    theme->list_default.indicator_alpha = 255;
    theme->list_default.text = 0xFFFFFF;
    theme->list_default.text_alpha = 255;
    theme->list_default.background_gradient = theme->system.background;
    theme->list_default.glyph_padding_left = 19;
    theme->list_default.glyph_alpha = 255;
    theme->list_default.glyph_recolour = 0xFFFFFF;
    theme->list_default.glyph_recolour_alpha = 0;
    theme->list_default.label_long_mode = LV_LABEL_LONG_SCROLL_CIRCULAR;
    theme->list_default.shadow_colour = 0x000000;
    theme->list_default.shadow_alpha = 255;
    theme->list_default.shadow_x_offset = 2;
    theme->list_default.shadow_y_offset = 2;

    theme->list_disabled.text = 0x808080;
    theme->list_disabled.text_alpha = 255;

    theme->list_focus.background = 0xF7E318;
    theme->list_focus.background_alpha = 255;
    theme->list_focus.gradient_start = 0;
    theme->list_focus.gradient_stop = 200;
    theme->list_focus.gradient_direction = 0;
    theme->list_focus.border_width = 5;
    theme->list_focus.border_side = LV_BORDER_SIDE_LEFT;
    theme->list_focus.indicator = 0xF7E318;
    theme->list_focus.indicator_alpha = 255;
    theme->list_focus.text = 0x100808;
    theme->list_focus.text_alpha = 255;
    theme->list_focus.background_gradient = theme->system.background;
    theme->list_focus.glyph_alpha = 255;
    theme->list_focus.glyph_recolour = 0x100808;
    theme->list_focus.glyph_recolour_alpha = 0;
    theme->list_focus.shadow_colour = 0x222222;
    theme->list_focus.shadow_alpha = 96;
    theme->list_focus.shadow_x_offset = 2;
    theme->list_focus.shadow_y_offset = 2;

    theme->image_list.alpha = 255;
    theme->image_list.radius = 3;
    theme->image_list.recolour = 0xF7E318;
    theme->image_list.recolour_alpha = 255;
    theme->image_list.pad_top = 0;
    theme->image_list.pad_bottom = 0;
    theme->image_list.pad_left = 0;
    theme->image_list.pad_right = 0;

    theme->image_preview.alpha = 255;
    theme->image_preview.radius = 3;
    theme->image_preview.recolour = 0xF7E318;
    theme->image_preview.recolour_alpha = 255;

    theme->charger.background = 0x282525;
    theme->charger.background_alpha = 255;
    theme->charger.text = 0xFFFFFF;
    theme->charger.text_alpha = 255;
    theme->charger.y_pos = 0;

    theme->verbose_boot.background = 0x000000;
    theme->verbose_boot.background_alpha = 255;
    theme->verbose_boot.text = 0xFFFFFF;
    theme->verbose_boot.text_alpha = 255;
    theme->verbose_boot.y_pos = 0;

    theme->osk.background = 0x100808;
    theme->osk.background_alpha = 255;
    theme->osk.border = 0x100808;
    theme->osk.border_alpha = 255;
    theme->osk.radius = 3;
    theme->osk.text = 0xFFFFFF;
    theme->osk.text_alpha = 255;
    theme->osk.text_focus = 0x100808;
    theme->osk.text_focus_alpha = 255;
    theme->osk.item.background = 0x282525;
    theme->osk.item.background_alpha = 255;
    theme->osk.item.background_focus = 0xF7E318;
    theme->osk.item.background_focus_alpha = 255;
    theme->osk.item.border = 0x282525;
    theme->osk.item.border_alpha = 255;
    theme->osk.item.border_focus = 0xF7E318;
    theme->osk.item.border_focus_alpha = 255;
    theme->osk.item.radius = 3;
    theme->osk.item.shadow_colour = 0x000000;
    theme->osk.item.shadow_alpha = 255;
    theme->osk.item.shadow_x_offset = 2;
    theme->osk.item.shadow_y_offset = 2;
    theme->osk.item.shadow_colour_focus = 0x222222;
    theme->osk.item.shadow_alpha_focus = 96;
    theme->osk.item.shadow_x_offset_focus = 2;
    theme->osk.item.shadow_y_offset_focus = 2;

    theme->message.background = 0x100808;
    theme->message.background_alpha = 255;
    theme->message.border = 0x100808;
    theme->message.border_alpha = 255;
    theme->message.radius = 3;
    theme->message.text = 0xFFFFFF;
    theme->message.text_alpha = 255;

    theme->bar.panel_width = (int16_t) (device->mux.width - 25);
    theme->bar.panel_height = 42;
    theme->bar.panel_background = 0x100808;
    theme->bar.panel_background_alpha = 255;
    theme->bar.panel_border = 0x100808;
    theme->bar.panel_border_alpha = 255;
    theme->bar.panel_border_radius = 3;
    theme->bar.progress_width = (int16_t) (device->mux.width - 90);
    theme->bar.progress_height = 16;
    theme->bar.progress_main_background = 0x7E730C;
    theme->bar.progress_main_background_alpha = 255;
    theme->bar.progress_active_background = 0xF7E318;
    theme->bar.progress_active_background_alpha = 255;
    theme->bar.progress_radius = 3;
    theme->bar.icon = 0xF7E318;
    theme->bar.icon_alpha = 255;
    theme->bar.y_pos = (int16_t) (device->mux.height - 96);

    theme->roll.text = 0x7E730C;
    theme->roll.text_alpha = 255;
    theme->roll.background = 0x100808;
    theme->roll.background_alpha = 255;
    theme->roll.radius = 3;
    theme->roll.select_text = 0xF7E318;
    theme->roll.select_text_alpha = 255;
    theme->roll.select_background = 0x7E730C;
    theme->roll.select_background_alpha = 255;
    theme->roll.select_radius = 3;
    theme->roll.border_colour = 0x100808;
    theme->roll.border_alpha = 255;
    theme->roll.border_radius = 255;

    theme->counter.alignment = 0;
    theme->counter.padding_around = 5;
    theme->counter.padding_side = 15;
    theme->counter.padding_top = 0;
    theme->counter.border_colour = 0x100808;
    theme->counter.border_alpha = 0;
    theme->counter.border_width = 2;
    theme->counter.radius = 0;
    theme->counter.background = 0x100808;
    theme->counter.background_alpha = 0;
    theme->counter.text = 0xFFFFFF;
    theme->counter.text_alpha = 0;
    theme->counter.text_fade_time = 0;
    snprintf(theme->counter.text_separator, MAX_BUFFER_SIZE, "%s", " / ");

    theme->misc.static_alignment = 255;
    theme->mux.item.count = 11;
    theme->mux.item.height = 0;
    theme->misc.content.size_to_content = 0;
    theme->misc.content.alignment = 0;
    theme->misc.content.padding_left = 0;
    theme->misc.content.padding_top = 0;

    theme->misc.content.height = (int16_t) (device->mux.height - theme->header.height - theme->footer.height - 4);
    theme->misc.content.width = device->mux.width;

    theme->misc.animated_background = 0;
    theme->misc.random_background = 0;
    theme->misc.image_overlay = 0;
    theme->misc.navigation_type = 0;
    theme->misc.antialiasing = 1;
    theme->misc.label_width = 57;

    theme->glyph.list = 0;
    theme->glyph.footer = 0;
    theme->glyph.header = 0;
    theme->glyph.grid = 0;

    snprintf(theme->terminal.font_size, MAX_BUFFER_SIZE, "%s", "16");
    snprintf(theme->terminal.font_hint, MAX_BUFFER_SIZE, "%s", "mono");
    snprintf(theme->terminal.foreground, MAX_BUFFER_SIZE, "%s", "FFFFFF");
    snprintf(theme->terminal.background, MAX_BUFFER_SIZE, "%s", "000000");

    theme->sdl.texture_blend_mode = 1;
    theme->sdl.draw_blend_mode = 0;
    theme->sdl.render.offset_x = 0.0f;
    theme->sdl.render.offset_y = 0.0f;
    theme->sdl.solid.r = 0;
    theme->sdl.solid.g = 0;
    theme->sdl.solid.b = 0;
}

void load_theme_from_scheme(const char *scheme, struct theme_config *theme, const struct mux_device *device) {
    mini_t *muos_theme = mini_try_load(scheme);

    theme->system.background = get_ini_hex(muos_theme, "background", "BACKGROUND", theme->system.background);
    theme->system.background_alpha =
        get_ini_int(muos_theme, "background", "BACKGROUND_ALPHA", theme->system.background_alpha);
    theme->system.background_gradient_color =
        get_ini_hex(muos_theme, "background", "BACKGROUND_GRADIENT_COLOR", theme->system.background_gradient_color);
    theme->system.background_gradient_start =
        get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_START", theme->system.background_gradient_start);
    theme->system.background_gradient_stop =
        get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_STOP", theme->system.background_gradient_stop);
    theme->system.background_gradient_direction = get_ini_int(
        muos_theme, "background", "BACKGROUND_GRADIENT_DIRECTION", theme->system.background_gradient_direction
    );
    theme->system.background_gradient_dither =
        get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_DITHER", theme->system.background_gradient_dither);
    theme->system.background_gradient_blur =
        get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_BLUR", theme->system.background_gradient_blur);

    theme->animation.animation_delay =
        get_ini_int(muos_theme, "animation", "ANIMATION_DELAY", theme->animation.animation_delay);
    if (theme->animation.animation_delay < 10) theme->animation.animation_delay = 10;
    theme->animation.animation_repeat =
        get_ini_int(muos_theme, "animation", "ANIMATION_REPEAT", theme->animation.animation_repeat);
    theme->animation.animation_foreground =
        get_ini_int(muos_theme, "animation", "ANIMATION_FOREGROUND", theme->animation.animation_foreground);
    theme->animation.animation_position =
        get_ini_int(muos_theme, "animation", "ANIMATION_POSITION", theme->animation.animation_position);
    theme->animation.animation_alpha =
        get_ini_int(muos_theme, "animation", "ANIMATION_ALPHA", theme->animation.animation_alpha);
    if (theme->animation.animation_alpha < 0) theme->animation.animation_alpha = 0;
    if (theme->animation.animation_alpha > 255) theme->animation.animation_alpha = 255;

    theme->font.font_list_size = get_ini_int(muos_theme, "font", "FONT_LIST_SIZE", theme->font.font_list_size);
    if (theme->font.font_list_size < 0) theme->font.font_list_size = 0;
    theme->font.font_header_size = get_ini_int(muos_theme, "font", "FONT_HEADER_SIZE", theme->font.font_header_size);
    if (theme->font.font_header_size < 0) theme->font.font_header_size = 0;
    theme->font.font_footer_size = get_ini_int(muos_theme, "font", "FONT_FOOTER_SIZE", theme->font.font_footer_size);
    if (theme->font.font_footer_size < 0) theme->font.font_footer_size = 0;
    theme->font.font_panel_size = get_ini_int(muos_theme, "font", "FONT_PANEL_SIZE", theme->font.font_panel_size);
    if (theme->font.font_panel_size < 0) theme->font.font_panel_size = 0;

    theme->font.header_pad_top = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_TOP", theme->font.header_pad_top);
    theme->font.header_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_BOTTOM", theme->font.header_pad_bottom);
    theme->font.header_icon_pad_top =
        get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_TOP", theme->font.header_icon_pad_top);
    theme->font.header_icon_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_BOTTOM", theme->font.header_icon_pad_bottom);
    theme->font.footer_pad_top = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_TOP", theme->font.footer_pad_top);
    theme->font.footer_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_BOTTOM", theme->font.footer_pad_bottom);
    theme->font.footer_icon_pad_top =
        get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_TOP", theme->font.footer_icon_pad_top);
    theme->font.footer_icon_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_BOTTOM", theme->font.footer_icon_pad_bottom);
    theme->font.message_pad_top = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_TOP", theme->font.message_pad_top);
    theme->font.message_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_BOTTOM", theme->font.message_pad_bottom);
    theme->font.message_icon_pad_top =
        get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_TOP", theme->font.message_icon_pad_top);
    theme->font.message_icon_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_BOTTOM", theme->font.message_icon_pad_bottom);
    theme->font.list_pad_top = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_TOP", theme->font.list_pad_top);
    theme->font.list_pad_bottom = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_BOTTOM", theme->font.list_pad_bottom);
    theme->font.list_pad_left = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_LEFT", theme->font.list_pad_left);
    theme->font.list_pad_right = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_RIGHT", theme->font.list_pad_right);
    theme->font.list_icon_pad_top =
        get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_TOP", theme->font.list_icon_pad_top);
    theme->font.list_icon_pad_bottom =
        get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_BOTTOM", theme->font.list_icon_pad_bottom);

    theme->status.align = get_ini_int(muos_theme, "status", "ALIGN", theme->status.align);
    theme->status.padding_left = get_ini_int(muos_theme, "status", "PADDING_LEFT", theme->status.padding_left);
    theme->status.padding_right = get_ini_int(muos_theme, "status", "PADDING_RIGHT", theme->status.padding_right);

    theme->status.battery.normal = get_ini_hex(muos_theme, "battery", "BATTERY_NORMAL", theme->status.battery.normal);
    theme->status.battery.active = get_ini_hex(muos_theme, "battery", "BATTERY_ACTIVE", theme->status.battery.active);
    theme->status.battery.low = get_ini_hex(muos_theme, "battery", "BATTERY_LOW", theme->status.battery.low);
    theme->status.battery.normal_alpha =
        get_ini_int(muos_theme, "battery", "BATTERY_NORMAL_ALPHA", theme->status.battery.normal_alpha);
    theme->status.battery.active_alpha =
        get_ini_int(muos_theme, "battery", "BATTERY_ACTIVE_ALPHA", theme->status.battery.active_alpha);
    theme->status.battery.low_alpha =
        get_ini_int(muos_theme, "battery", "BATTERY_LOW_ALPHA", theme->status.battery.low_alpha);

    theme->status.network.normal = get_ini_hex(muos_theme, "network", "NETWORK_NORMAL", theme->status.network.normal);
    theme->status.network.active = get_ini_hex(muos_theme, "network", "NETWORK_ACTIVE", theme->status.network.active);
    theme->status.network.normal_alpha =
        get_ini_int(muos_theme, "network", "NETWORK_NORMAL_ALPHA", theme->status.network.normal_alpha);
    theme->status.network.active_alpha =
        get_ini_int(muos_theme, "network", "NETWORK_ACTIVE_ALPHA", theme->status.network.active_alpha);

    theme->status.bluetooth.normal =
        get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_NORMAL", theme->status.bluetooth.normal);
    theme->status.bluetooth.active =
        get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE", theme->status.bluetooth.active);
    theme->status.bluetooth.normal_alpha =
        get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_NORMAL_ALPHA", theme->status.bluetooth.normal_alpha);
    theme->status.bluetooth.active_alpha =
        get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE_ALPHA", theme->status.bluetooth.active_alpha);

    theme->datetime.text = get_ini_hex(muos_theme, "date", "DATETIME_TEXT", theme->datetime.text);
    theme->datetime.alpha = get_ini_int(muos_theme, "date", "DATETIME_ALPHA", theme->datetime.alpha);
    theme->datetime.align = get_ini_int(muos_theme, "date", "DATETIME_ALIGN", theme->datetime.align);
    theme->datetime.padding_left = get_ini_int(muos_theme, "date", "PADDING_LEFT", theme->datetime.padding_left);
    theme->datetime.padding_right = get_ini_int(muos_theme, "date", "PADDING_RIGHT", theme->datetime.padding_right);

    theme->footer.height = get_ini_int(muos_theme, "footer", "FOOTER_HEIGHT", theme->footer.height);
    theme->footer.background = get_ini_hex(muos_theme, "footer", "FOOTER_BACKGROUND", theme->footer.background);
    theme->footer.background_alpha =
        get_ini_uint(muos_theme, "footer", "FOOTER_BACKGROUND_ALPHA", theme->footer.background_alpha);
    theme->footer.text = get_ini_hex(muos_theme, "footer", "FOOTER_TEXT", theme->footer.text);
    theme->footer.text_alpha = get_ini_int(muos_theme, "footer", "FOOTER_TEXT_ALPHA", theme->footer.text_alpha);

    theme->header.height = get_ini_int(muos_theme, "header", "HEADER_HEIGHT", theme->header.height);
    theme->header.background = get_ini_hex(muos_theme, "header", "HEADER_BACKGROUND", theme->header.background);
    theme->header.background_alpha =
        get_ini_uint(muos_theme, "header", "HEADER_BACKGROUND_ALPHA", theme->header.background_alpha);
    theme->header.text = get_ini_hex(muos_theme, "header", "HEADER_TEXT", theme->header.text);
    theme->header.text_alpha = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALPHA", theme->header.text_alpha);
    theme->header.text_align = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALIGN", theme->header.text_align);
    theme->header.padding_left = get_ini_int(muos_theme, "header", "PADDING_LEFT", theme->header.padding_left);
    theme->header.padding_right = get_ini_int(muos_theme, "header", "PADDING_RIGHT", theme->header.padding_right);

    theme->help.background = get_ini_hex(muos_theme, "help", "HELP_BACKGROUND", theme->help.background);
    theme->help.background_alpha =
        get_ini_int(muos_theme, "help", "HELP_BACKGROUND_ALPHA", theme->help.background_alpha);
    theme->help.border = get_ini_hex(muos_theme, "help", "HELP_BORDER", theme->help.border);
    theme->help.border_alpha = get_ini_int(muos_theme, "help", "HELP_BORDER_ALPHA", theme->help.border_alpha);
    theme->help.content = get_ini_hex(muos_theme, "help", "HELP_CONTENT", theme->help.content);
    theme->help.title = get_ini_hex(muos_theme, "help", "HELP_TITLE", theme->help.title);
    theme->help.radius = get_ini_int(muos_theme, "help", "HELP_RADIUS", theme->help.radius);

    theme->dialogue.background = get_ini_hex(muos_theme, "dialogue", "DIALOGUE_BACKGROUND", theme->dialogue.background);
    theme->dialogue.background_alpha =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_BACKGROUND_ALPHA", theme->dialogue.background_alpha);
    theme->dialogue.border = get_ini_hex(muos_theme, "dialogue", "DIALOGUE_BORDER", theme->dialogue.border);
    theme->dialogue.border_alpha =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_BORDER_ALPHA", theme->dialogue.border_alpha);
    theme->dialogue.title = get_ini_hex(muos_theme, "dialogue", "DIALOGUE_TITLE", theme->dialogue.title);
    theme->dialogue.content = get_ini_hex(muos_theme, "dialogue", "DIALOGUE_CONTENT", theme->dialogue.content);
    theme->dialogue.option = get_ini_hex(muos_theme, "dialogue", "DIALOGUE_OPTION", theme->dialogue.option);
    theme->dialogue.dim_alpha = get_ini_int(muos_theme, "dialogue", "DIALOGUE_DIM_ALPHA", theme->dialogue.dim_alpha);
    theme->dialogue.selection = get_ini_hex(muos_theme, "dialogue", "DIALOGUE_SELECTION", theme->dialogue.selection);
    theme->dialogue.selection_alpha =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SELECTION_ALPHA", theme->dialogue.selection_alpha);
    theme->dialogue.radius.main =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_RADIUS_MAIN", theme->dialogue.radius.main);
    theme->dialogue.radius.selected =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_RADIUS_SELECTED", theme->dialogue.radius.selected);
    theme->dialogue.shadow_colour =
        get_ini_hex(muos_theme, "dialogue", "DIALOGUE_SHADOW_COLOUR", theme->dialogue.shadow_colour);
    theme->dialogue.shadow_alpha =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SHADOW_ALPHA", theme->dialogue.shadow_alpha);
    if (theme->dialogue.shadow_alpha < 0) theme->dialogue.shadow_alpha = 0;
    if (theme->dialogue.shadow_alpha > 255) theme->dialogue.shadow_alpha = 255;
    theme->dialogue.shadow_x_offset =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SHADOW_X_OFFSET", theme->dialogue.shadow_x_offset);
    theme->dialogue.shadow_y_offset =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SHADOW_Y_OFFSET", theme->dialogue.shadow_y_offset);
    theme->dialogue.shadow_colour_focus =
        get_ini_hex(muos_theme, "dialogue", "DIALOGUE_SHADOW_COLOUR_FOCUS", theme->dialogue.shadow_colour_focus);
    theme->dialogue.shadow_alpha_focus =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SHADOW_ALPHA_FOCUS", theme->dialogue.shadow_alpha_focus);
    if (theme->dialogue.shadow_alpha_focus < 0) theme->dialogue.shadow_alpha_focus = 0;
    if (theme->dialogue.shadow_alpha_focus > 255) theme->dialogue.shadow_alpha_focus = 255;
    theme->dialogue.shadow_x_offset_focus =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SHADOW_X_OFFSET_FOCUS", theme->dialogue.shadow_x_offset_focus);
    theme->dialogue.shadow_y_offset_focus =
        get_ini_int(muos_theme, "dialogue", "DIALOGUE_SHADOW_Y_OFFSET_FOCUS", theme->dialogue.shadow_y_offset_focus);

    theme->nav.alignment = get_ini_uint(muos_theme, "navigation", "ALIGNMENT", theme->nav.alignment);
    theme->nav.spacing = get_ini_uint(muos_theme, "navigation", "SPACING", theme->nav.spacing);

    theme->nav.lr.glyph = get_ini_hex(muos_theme, "navigation", "NAV_LR_GLYPH", theme->nav.lr.glyph);
    theme->nav.lr.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_LR_GLYPH_ALPHA", theme->nav.lr.glyph_alpha);
    theme->nav.lr.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_LR_GLYPH_RECOLOUR_ALPHA", theme->nav.lr.glyph_recolour_alpha);
    theme->nav.lr.text = get_ini_hex(muos_theme, "navigation", "NAV_LR_TEXT", theme->nav.lr.text);
    theme->nav.lr.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_LR_TEXT_ALPHA", theme->nav.lr.text_alpha);

    theme->nav.ud.glyph = get_ini_hex(muos_theme, "navigation", "NAV_UD_GLYPH", theme->nav.ud.glyph);
    theme->nav.ud.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_UD_GLYPH_ALPHA", theme->nav.ud.glyph_alpha);
    theme->nav.ud.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_UD_GLYPH_RECOLOUR_ALPHA", theme->nav.ud.glyph_recolour_alpha);
    theme->nav.ud.text = get_ini_hex(muos_theme, "navigation", "NAV_UD_TEXT", theme->nav.ud.text);
    theme->nav.ud.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_UD_TEXT_ALPHA", theme->nav.ud.text_alpha);

    theme->nav.a.glyph = get_ini_hex(muos_theme, "navigation", "NAV_A_GLYPH", theme->nav.a.glyph);
    theme->nav.a.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_ALPHA", theme->nav.a.glyph_alpha);
    theme->nav.a.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_RECOLOUR_ALPHA", theme->nav.a.glyph_recolour_alpha);
    theme->nav.a.text = get_ini_hex(muos_theme, "navigation", "NAV_A_TEXT", theme->nav.a.text);
    theme->nav.a.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_A_TEXT_ALPHA", theme->nav.a.text_alpha);

    theme->nav.b.glyph = get_ini_hex(muos_theme, "navigation", "NAV_B_GLYPH", theme->nav.b.glyph);
    theme->nav.b.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_ALPHA", theme->nav.b.glyph_alpha);
    theme->nav.b.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_RECOLOUR_ALPHA", theme->nav.b.glyph_recolour_alpha);
    theme->nav.b.text = get_ini_hex(muos_theme, "navigation", "NAV_B_TEXT", theme->nav.b.text);
    theme->nav.b.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_B_TEXT_ALPHA", theme->nav.b.text_alpha);

    theme->nav.c.glyph = get_ini_hex(muos_theme, "navigation", "NAV_C_GLYPH", theme->nav.c.glyph);
    theme->nav.c.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_ALPHA", theme->nav.c.glyph_alpha);
    theme->nav.c.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_RECOLOUR_ALPHA", theme->nav.c.glyph_recolour_alpha);
    theme->nav.c.text = get_ini_hex(muos_theme, "navigation", "NAV_C_TEXT", theme->nav.c.text);
    theme->nav.c.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_C_TEXT_ALPHA", theme->nav.c.text_alpha);

    theme->nav.x.glyph = get_ini_hex(muos_theme, "navigation", "NAV_X_GLYPH", theme->nav.x.glyph);
    theme->nav.x.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_ALPHA", theme->nav.x.glyph_alpha);
    theme->nav.x.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_RECOLOUR_ALPHA", theme->nav.x.glyph_recolour_alpha);
    theme->nav.x.text = get_ini_hex(muos_theme, "navigation", "NAV_X_TEXT", theme->nav.x.text);
    theme->nav.x.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_X_TEXT_ALPHA", theme->nav.x.text_alpha);

    theme->nav.y.glyph = get_ini_hex(muos_theme, "navigation", "NAV_Y_GLYPH", theme->nav.y.glyph);
    theme->nav.y.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_ALPHA", theme->nav.y.glyph_alpha);
    theme->nav.y.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_RECOLOUR_ALPHA", theme->nav.y.glyph_recolour_alpha);
    theme->nav.y.text = get_ini_hex(muos_theme, "navigation", "NAV_Y_TEXT", theme->nav.y.text);
    theme->nav.y.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_Y_TEXT_ALPHA", theme->nav.y.text_alpha);

    theme->nav.z.glyph = get_ini_hex(muos_theme, "navigation", "NAV_Z_GLYPH", theme->nav.z.glyph);
    theme->nav.z.glyph_alpha = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_ALPHA", theme->nav.z.glyph_alpha);
    theme->nav.z.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_RECOLOUR_ALPHA", theme->nav.z.glyph_recolour_alpha);
    theme->nav.z.text = get_ini_hex(muos_theme, "navigation", "NAV_Z_TEXT", theme->nav.z.text);
    theme->nav.z.text_alpha = get_ini_int(muos_theme, "navigation", "NAV_Z_TEXT_ALPHA", theme->nav.z.text_alpha);

    theme->nav.menu.glyph = get_ini_hex(muos_theme, "navigation", "NAV_MENU_GLYPH", theme->nav.menu.glyph);
    theme->nav.menu.glyph_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_ALPHA", theme->nav.menu.glyph_alpha);
    theme->nav.menu.glyph_recolour_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_RECOLOUR_ALPHA", theme->nav.menu.glyph_recolour_alpha);
    theme->nav.menu.text = get_ini_hex(muos_theme, "navigation", "NAV_MENU_TEXT", theme->nav.menu.text);
    theme->nav.menu.text_alpha =
        get_ini_int(muos_theme, "navigation", "NAV_MENU_TEXT_ALPHA", theme->nav.menu.text_alpha);

    theme->grid.navigation_type = get_ini_int(muos_theme, "grid", "NAVIGATION_TYPE", theme->grid.navigation_type);
    theme->grid.background = get_ini_hex(muos_theme, "grid", "BACKGROUND", theme->grid.background);
    theme->grid.background_alpha = get_ini_int(muos_theme, "grid", "BACKGROUND_ALPHA", theme->grid.background_alpha);
    theme->grid.alignment = get_ini_int(muos_theme, "grid", "ALIGNMENT", theme->grid.alignment);
    theme->grid.alignment_x_offset =
        get_ini_int(muos_theme, "grid", "ALIGNMENT_X_OFFSET", theme->grid.alignment_x_offset);
    theme->grid.alignment_y_offset =
        get_ini_int(muos_theme, "grid", "ALIGNMENT_Y_OFFSET", theme->grid.alignment_y_offset);
    theme->grid.location_x = get_ini_int(muos_theme, "grid", "LOCATION_X", theme->grid.location_x);
    theme->grid.location_y = get_ini_int(muos_theme, "grid", "LOCATION_Y", theme->grid.location_y);
    theme->grid.column_count = get_ini_int(muos_theme, "grid", "COLUMN_COUNT", theme->grid.column_count);
    theme->grid.column_padding = get_ini_int(muos_theme, "grid", "COLUMN_PADDING", theme->grid.column_padding);
    theme->grid.row_count = get_ini_int(muos_theme, "grid", "ROW_COUNT", theme->grid.row_count);
    theme->grid.row_padding = get_ini_int(muos_theme, "grid", "ROW_PADDING", theme->grid.row_padding);

    theme->grid.current_item_label.alignment =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_ALIGNMENT", theme->grid.current_item_label.alignment);
    theme->grid.current_item_label.width =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_WIDTH", theme->grid.current_item_label.width);
    theme->grid.current_item_label.height =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_HEIGHT", theme->grid.current_item_label.height);
    theme->grid.current_item_label.offset_x =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_OFFSET_X", theme->grid.current_item_label.offset_x);
    theme->grid.current_item_label.offset_y =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_OFFSET_Y", theme->grid.current_item_label.offset_y);
    theme->grid.current_item_label.radius =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_RADIUS", theme->grid.current_item_label.radius);
    theme->grid.current_item_label.border_width =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_BORDER_WIDTH", theme->grid.current_item_label.border_width);
    theme->grid.current_item_label.border =
        get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_BORDER", theme->grid.current_item_label.border);
    theme->grid.current_item_label.border_alpha =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_BORDER_ALPHA", theme->grid.current_item_label.border_alpha);
    theme->grid.current_item_label.background =
        get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND", theme->grid.current_item_label.background);
    theme->grid.current_item_label.background_alpha = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND_ALPHA", theme->grid.current_item_label.background_alpha
    );
    theme->grid.current_item_label.background_gradient_color = get_ini_hex(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND_GRADIENT_COLOR",
        theme->grid.current_item_label.background_gradient_color
    );
    theme->grid.current_item_label.background_gradient_start = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND_GRADIENT_START",
        theme->grid.current_item_label.background_gradient_start
    );
    theme->grid.current_item_label.background_gradient_stop = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND_GRADIENT_STOP",
        theme->grid.current_item_label.background_gradient_stop
    );
    theme->grid.current_item_label.background_gradient_direction = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND_GRADIENT_DIRECTION",
        theme->grid.current_item_label.background_gradient_direction
    );
    theme->grid.current_item_label.shadow =
        get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_SHADOW", theme->grid.current_item_label.shadow);
    theme->grid.current_item_label.shadow_width =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_SHADOW_WIDTH", theme->grid.current_item_label.shadow_width);
    theme->grid.current_item_label.shadow_x_offset = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_SHADOW_X_OFFSET", theme->grid.current_item_label.shadow_x_offset
    );
    theme->grid.current_item_label.shadow_y_offset = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_SHADOW_Y_OFFSET", theme->grid.current_item_label.shadow_y_offset
    );
    theme->grid.current_item_label.label_long_mode = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_LABEL_LONG_MODE", theme->grid.current_item_label.label_long_mode
    );
    theme->grid.current_item_label.text =
        get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT", theme->grid.current_item_label.text);
    theme->grid.current_item_label.text_alpha =
        get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_ALPHA", theme->grid.current_item_label.text_alpha);
    theme->grid.current_item_label.text_alignment = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_ALIGNMENT", theme->grid.current_item_label.text_alignment
    );
    theme->grid.current_item_label.text_line_spacing = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_LINE_SPACING", theme->grid.current_item_label.text_line_spacing
    );
    theme->grid.current_item_label.text_padding_bottom = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_PADDING_BOTTOM", theme->grid.current_item_label.text_padding_bottom
    );
    theme->grid.current_item_label.text_padding_left = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_PADDING_LEFT", theme->grid.current_item_label.text_padding_left
    );
    theme->grid.current_item_label.text_padding_right = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_PADDING_RIGHT", theme->grid.current_item_label.text_padding_right
    );
    theme->grid.current_item_label.text_padding_top = get_ini_int(
        muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_PADDING_TOP", theme->grid.current_item_label.text_padding_top
    );

    theme->grid.row_height = get_ini_int(muos_theme, "grid", "ROW_HEIGHT", theme->grid.row_height);
    theme->grid.column_width = get_ini_int(muos_theme, "grid", "COLUMN_WIDTH", theme->grid.column_width);

    theme->grid.cell.column_align = get_ini_int(muos_theme, "grid", "CELL_COLUMN_ALIGN", theme->grid.cell.column_align);
    theme->grid.cell.row_align = get_ini_int(muos_theme, "grid", "CELL_ROW_ALIGN", theme->grid.cell.row_align);
    theme->grid.cell.width = get_ini_int(muos_theme, "grid", "CELL_WIDTH", theme->grid.cell.width);
    theme->grid.cell.height = get_ini_int(muos_theme, "grid", "CELL_HEIGHT", theme->grid.cell.height);
    theme->grid.cell.radius = get_ini_int(muos_theme, "grid", "CELL_RADIUS", theme->grid.cell.radius);
    theme->grid.cell.border_width = get_ini_int(muos_theme, "grid", "CELL_BORDER_WIDTH", theme->grid.cell.border_width);
    theme->grid.cell.image_padding_top =
        get_ini_int(muos_theme, "grid", "CELL_IMAGE_PADDING_TOP", theme->grid.cell.image_padding_top);
    theme->grid.cell.text_padding_bottom =
        get_ini_int(muos_theme, "grid", "CELL_TEXT_PADDING_BOTTOM", theme->grid.cell.text_padding_bottom);
    theme->grid.cell.text_padding_side =
        get_ini_int(muos_theme, "grid", "CELL_TEXT_PADDING_SIDE", theme->grid.cell.text_padding_side);
    theme->grid.cell.text_line_spacing =
        get_ini_int(muos_theme, "grid", "CELL_TEXT_LINE_SPACING", theme->grid.cell.text_line_spacing);
    theme->grid.cell.shadow = get_ini_hex(muos_theme, "grid", "CELL_SHADOW", theme->grid.cell.shadow);
    theme->grid.cell.shadow_width = get_ini_int(muos_theme, "grid", "CELL_SHADOW_WIDTH", theme->grid.cell.shadow_width);
    theme->grid.cell.shadow_x_offset =
        get_ini_int(muos_theme, "grid", "CELL_SHADOW_X_OFFSET", theme->grid.cell.shadow_x_offset);
    theme->grid.cell.shadow_y_offset =
        get_ini_int(muos_theme, "grid", "CELL_SHADOW_Y_OFFSET", theme->grid.cell.shadow_y_offset);

    theme->grid.cell_default.background =
        get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND", theme->grid.cell_default.background);
    theme->grid.cell_default.background_alpha =
        get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_ALPHA", theme->grid.cell_default.background_alpha);
    theme->grid.cell_default.background_gradient_color = get_ini_hex(
        muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_COLOR", theme->grid.cell_default.background_gradient_color
    );
    theme->grid.cell_default.background_gradient_start = get_ini_int(
        muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_START", theme->grid.cell_default.background_gradient_start
    );
    theme->grid.cell_default.background_gradient_stop = get_ini_int(
        muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_STOP", theme->grid.cell_default.background_gradient_stop
    );
    theme->grid.cell_default.background_gradient_direction = get_ini_int(
        muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_DIRECTION",
        theme->grid.cell_default.background_gradient_direction
    );
    theme->grid.cell_default.border =
        get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BORDER", theme->grid.cell_default.border);
    theme->grid.cell_default.border_alpha =
        get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BORDER_ALPHA", theme->grid.cell_default.border_alpha);
    theme->grid.cell_default.image_alpha =
        get_ini_int(muos_theme, "grid", "CELL_DEFAULT_IMAGE_ALPHA", theme->grid.cell_default.image_alpha);
    theme->grid.cell_default.image_recolour =
        get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_IMAGE_RECOLOUR", theme->grid.cell_default.image_recolour);
    theme->grid.cell_default.image_recolour_alpha = get_ini_int(
        muos_theme, "grid", "CELL_DEFAULT_IMAGE_RECOLOUR_ALPHA", theme->grid.cell_default.image_recolour_alpha
    );
    theme->grid.cell_default.text = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_TEXT", theme->grid.cell_default.text);
    theme->grid.cell_default.text_alpha =
        get_ini_int(muos_theme, "grid", "CELL_DEFAULT_TEXT_ALPHA", theme->grid.cell_default.text_alpha);

    theme->grid.cell_focus.background =
        get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BACKGROUND", theme->grid.cell_focus.background);
    theme->grid.cell_focus.background_alpha =
        get_ini_int(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_ALPHA", theme->grid.cell_focus.background_alpha);
    theme->grid.cell_focus.background_gradient_color = get_ini_hex(
        muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_COLOR", theme->grid.cell_focus.background_gradient_color
    );
    theme->grid.cell_focus.background_gradient_start = get_ini_int(
        muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_START", theme->grid.cell_focus.background_gradient_start
    );
    theme->grid.cell_focus.background_gradient_stop = get_ini_int(
        muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_STOP", theme->grid.cell_focus.background_gradient_stop
    );
    theme->grid.cell_focus.background_gradient_direction = get_ini_int(
        muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_DIRECTION",
        theme->grid.cell_focus.background_gradient_direction
    );
    theme->grid.cell_focus.border = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BORDER", theme->grid.cell_focus.border);
    theme->grid.cell_focus.border_alpha =
        get_ini_int(muos_theme, "grid", "CELL_FOCUS_BORDER_ALPHA", theme->grid.cell_focus.border_alpha);
    theme->grid.cell_focus.image_alpha =
        get_ini_int(muos_theme, "grid", "CELL_FOCUS_IMAGE_ALPHA", theme->grid.cell_focus.image_alpha);
    theme->grid.cell_focus.image_recolour =
        get_ini_hex(muos_theme, "grid", "CELL_FOCUS_IMAGE_RECOLOUR", theme->grid.cell_focus.image_recolour);
    theme->grid.cell_focus.image_recolour_alpha =
        get_ini_int(muos_theme, "grid", "CELL_FOCUS_IMAGE_RECOLOUR_ALPHA", theme->grid.cell_focus.image_recolour_alpha);
    theme->grid.cell_focus.text = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_TEXT", theme->grid.cell_focus.text);
    theme->grid.cell_focus.text_alpha =
        get_ini_int(muos_theme, "grid", "CELL_FOCUS_TEXT_ALPHA", theme->grid.cell_focus.text_alpha);

    theme->list_default.radius = get_ini_int(muos_theme, "list", "LIST_DEFAULT_RADIUS", theme->list_default.radius);
    theme->list_default.background =
        get_ini_hex(muos_theme, "list", "LIST_DEFAULT_BACKGROUND", theme->list_default.background);
    theme->list_default.background_alpha =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_BACKGROUND_ALPHA", theme->list_default.background_alpha);
    theme->list_default.gradient_start =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_START", theme->list_default.gradient_start);
    theme->list_default.gradient_stop =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_STOP", theme->list_default.gradient_stop);
    theme->list_default.gradient_direction =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_DIRECTION", theme->list_default.gradient_direction);
    theme->list_default.border_width =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_BORDER_WIDTH", theme->list_default.border_width);
    theme->list_default.border_side =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_BORDER_SIDE", theme->list_default.border_side);
    theme->list_default.indicator =
        get_ini_hex(muos_theme, "list", "LIST_DEFAULT_INDICATOR", theme->list_default.indicator);
    theme->list_default.indicator_alpha =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_INDICATOR_ALPHA", theme->list_default.indicator_alpha);
    theme->list_default.text = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_TEXT", theme->list_default.text);
    theme->list_default.text_alpha =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_TEXT_ALPHA", theme->list_default.text_alpha);
    theme->list_default.background_gradient =
        theme->list_default.gradient_start == 255 ? theme->list_default.background : theme->system.background;
    theme->list_default.glyph_padding_left =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_PAD_LEFT", theme->list_default.glyph_padding_left);
    theme->list_default.glyph_alpha =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_ALPHA", theme->list_default.glyph_alpha);
    theme->list_default.glyph_recolour =
        get_ini_hex(muos_theme, "list", "LIST_DEFAULT_GLYPH_RECOLOUR", theme->list_default.glyph_recolour);
    theme->list_default.glyph_recolour_alpha =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_RECOLOUR_ALPHA", theme->list_default.glyph_recolour_alpha);
    theme->list_default.label_long_mode =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_LABEL_LONG_MODE", theme->list_default.label_long_mode);
    theme->list_default.shadow_colour =
        get_ini_hex(muos_theme, "list", "LIST_DEFAULT_SHADOW_COLOUR", theme->list_default.shadow_colour);
    theme->list_default.shadow_alpha =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_SHADOW_ALPHA", theme->list_default.shadow_alpha);
    if (theme->list_default.shadow_alpha < 0) theme->list_default.shadow_alpha = 0;
    if (theme->list_default.shadow_alpha > 255) theme->list_default.shadow_alpha = 255;
    theme->list_default.shadow_x_offset =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_SHADOW_X_OFFSET", theme->list_default.shadow_x_offset);
    theme->list_default.shadow_y_offset =
        get_ini_int(muos_theme, "list", "LIST_DEFAULT_SHADOW_Y_OFFSET", theme->list_default.shadow_y_offset);

    theme->list_disabled.text = get_ini_hex(muos_theme, "list", "LIST_DISABLED_TEXT", theme->list_disabled.text);
    theme->list_disabled.text_alpha =
        get_ini_int(muos_theme, "list", "LIST_DISABLED_TEXT_ALPHA", theme->list_disabled.text_alpha);

    theme->list_focus.background =
        get_ini_hex(muos_theme, "list", "LIST_FOCUS_BACKGROUND", theme->list_focus.background);
    theme->list_focus.background_alpha =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_BACKGROUND_ALPHA", theme->list_focus.background_alpha);
    theme->list_focus.gradient_start =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_START", theme->list_focus.gradient_start);
    theme->list_focus.gradient_stop =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_STOP", theme->list_focus.gradient_stop);
    theme->list_focus.gradient_direction =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_DIRECTION", theme->list_focus.gradient_direction);
    theme->list_focus.border_width =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_BORDER_WIDTH", theme->list_focus.border_width);
    theme->list_focus.border_side =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_BORDER_SIDE", theme->list_focus.border_side);
    theme->list_focus.indicator = get_ini_hex(muos_theme, "list", "LIST_FOCUS_INDICATOR", theme->list_focus.indicator);
    theme->list_focus.indicator_alpha =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_INDICATOR_ALPHA", theme->list_focus.indicator_alpha);
    theme->list_focus.text = get_ini_hex(muos_theme, "list", "LIST_FOCUS_TEXT", theme->list_focus.text);
    theme->list_focus.text_alpha =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_TEXT_ALPHA", theme->list_focus.text_alpha);
    theme->list_focus.background_gradient =
        theme->list_focus.gradient_start == 255 ? theme->list_focus.background : theme->system.background;
    theme->list_focus.glyph_alpha =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_GLYPH_ALPHA", theme->list_focus.glyph_alpha);
    theme->list_focus.glyph_recolour =
        get_ini_hex(muos_theme, "list", "LIST_FOCUS_GLYPH_RECOLOUR", theme->list_focus.glyph_recolour);
    theme->list_focus.glyph_recolour_alpha =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_GLYPH_RECOLOUR_ALPHA", theme->list_focus.glyph_recolour_alpha);
    theme->list_focus.shadow_colour =
        get_ini_hex(muos_theme, "list", "LIST_FOCUS_SHADOW_COLOUR", theme->list_focus.shadow_colour);
    theme->list_focus.shadow_alpha =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_SHADOW_ALPHA", theme->list_focus.shadow_alpha);
    if (theme->list_focus.shadow_alpha < 0) theme->list_focus.shadow_alpha = 0;
    if (theme->list_focus.shadow_alpha > 255) theme->list_focus.shadow_alpha = 255;
    theme->list_focus.shadow_x_offset =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_SHADOW_X_OFFSET", theme->list_focus.shadow_x_offset);
    theme->list_focus.shadow_y_offset =
        get_ini_int(muos_theme, "list", "LIST_FOCUS_SHADOW_Y_OFFSET", theme->list_focus.shadow_y_offset);

    theme->image_list.alpha = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_ALPHA", theme->image_list.alpha);
    theme->image_list.radius = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RADIUS", theme->image_list.radius);
    theme->image_list.recolour =
        get_ini_hex(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR", theme->image_list.recolour);
    theme->image_list.recolour_alpha =
        get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR_ALPHA", theme->image_list.recolour_alpha);
    theme->image_list.pad_top = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_TOP", theme->image_list.pad_top);
    theme->image_list.pad_bottom =
        get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_BOTTOM", theme->image_list.pad_bottom);
    theme->image_list.pad_left =
        get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_LEFT", theme->image_list.pad_left);
    theme->image_list.pad_right =
        get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_RIGHT", theme->image_list.pad_right);

    theme->image_preview.alpha =
        get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_ALPHA", theme->image_preview.alpha);
    theme->image_preview.radius =
        get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RADIUS", theme->image_preview.radius);
    theme->image_preview.recolour =
        get_ini_hex(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR", theme->image_preview.recolour);
    theme->image_preview.recolour_alpha =
        get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR_ALPHA", theme->image_preview.recolour_alpha);

    theme->charger.background = get_ini_hex(muos_theme, "charging", "CHARGER_BACKGROUND", theme->charger.background);
    theme->charger.background_alpha =
        get_ini_int(muos_theme, "charging", "CHARGER_BACKGROUND_ALPHA", theme->charger.background_alpha);
    theme->charger.text = get_ini_hex(muos_theme, "charging", "CHARGER_TEXT", theme->charger.text);
    theme->charger.text_alpha = get_ini_int(muos_theme, "charging", "CHARGER_TEXT_ALPHA", theme->charger.text_alpha);
    theme->charger.y_pos = get_ini_int(muos_theme, "charging", "CHARGER_Y_POS", theme->charger.y_pos);

    theme->verbose_boot.background =
        get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND", theme->verbose_boot.background);
    theme->verbose_boot.background_alpha =
        get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND_ALPHA", theme->verbose_boot.background_alpha);
    theme->verbose_boot.text = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_TEXT", theme->verbose_boot.text);
    theme->verbose_boot.text_alpha =
        get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_TEXT_ALPHA", theme->verbose_boot.text_alpha);
    theme->verbose_boot.y_pos = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_Y_POS", theme->verbose_boot.y_pos);

    theme->osk.background = get_ini_hex(muos_theme, "keyboard", "OSK_BACKGROUND", theme->osk.background);
    theme->osk.background_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_BACKGROUND_ALPHA", theme->osk.background_alpha);
    theme->osk.border = get_ini_hex(muos_theme, "keyboard", "OSK_BORDER", theme->osk.border);
    theme->osk.border_alpha = get_ini_int(muos_theme, "keyboard", "OSK_BORDER_ALPHA", theme->osk.border_alpha);
    theme->osk.radius = get_ini_int(muos_theme, "keyboard", "OSK_RADIUS", theme->osk.radius);
    theme->osk.text = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT", theme->osk.text);
    theme->osk.text_alpha = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_ALPHA", theme->osk.text_alpha);
    theme->osk.text_focus = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT_FOCUS", theme->osk.text_focus);
    theme->osk.text_focus_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_TEXT_FOCUS_ALPHA", theme->osk.text_focus_alpha);
    theme->osk.item.background = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND", theme->osk.item.background);
    theme->osk.item.background_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_ALPHA", theme->osk.item.background_alpha);
    theme->osk.item.background_focus =
        get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS", theme->osk.item.background_focus);
    theme->osk.item.background_focus_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS_ALPHA", theme->osk.item.background_focus_alpha);
    theme->osk.item.border = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER", theme->osk.item.border);
    theme->osk.item.border_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_ALPHA", theme->osk.item.border_alpha);
    theme->osk.item.border_focus =
        get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS", theme->osk.item.border_focus);
    theme->osk.item.border_focus_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS_ALPHA", theme->osk.item.border_focus_alpha);
    theme->osk.item.radius = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_RADIUS", theme->osk.item.radius);
    theme->osk.item.shadow_colour =
        get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_SHADOW_COLOUR", theme->osk.item.shadow_colour);
    theme->osk.item.shadow_alpha =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_SHADOW_ALPHA", theme->osk.item.shadow_alpha);
    if (theme->osk.item.shadow_alpha < 0) theme->osk.item.shadow_alpha = 0;
    if (theme->osk.item.shadow_alpha > 255) theme->osk.item.shadow_alpha = 255;
    theme->osk.item.shadow_x_offset =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_SHADOW_X_OFFSET", theme->osk.item.shadow_x_offset);
    theme->osk.item.shadow_y_offset =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_SHADOW_Y_OFFSET", theme->osk.item.shadow_y_offset);
    theme->osk.item.shadow_colour_focus =
        get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_SHADOW_COLOUR_FOCUS", theme->osk.item.shadow_colour_focus);
    theme->osk.item.shadow_alpha_focus =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_SHADOW_ALPHA_FOCUS", theme->osk.item.shadow_alpha_focus);
    if (theme->osk.item.shadow_alpha_focus < 0) theme->osk.item.shadow_alpha_focus = 0;
    if (theme->osk.item.shadow_alpha_focus > 255) theme->osk.item.shadow_alpha_focus = 255;
    theme->osk.item.shadow_x_offset_focus =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_SHADOW_X_OFFSET_FOCUS", theme->osk.item.shadow_x_offset_focus);
    theme->osk.item.shadow_y_offset_focus =
        get_ini_int(muos_theme, "keyboard", "OSK_ITEM_SHADOW_Y_OFFSET_FOCUS", theme->osk.item.shadow_y_offset_focus);

    theme->message.background = get_ini_hex(muos_theme, "notification", "MSG_BACKGROUND", theme->message.background);
    theme->message.background_alpha =
        get_ini_int(muos_theme, "notification", "MSG_BACKGROUND_ALPHA", theme->message.background_alpha);
    theme->message.border = get_ini_hex(muos_theme, "notification", "MSG_BORDER", theme->message.border);
    theme->message.border_alpha =
        get_ini_int(muos_theme, "notification", "MSG_BORDER_ALPHA", theme->message.border_alpha);
    theme->message.radius = get_ini_int(muos_theme, "notification", "MSG_RADIUS", theme->message.radius);
    theme->message.text = get_ini_hex(muos_theme, "notification", "MSG_TEXT", theme->message.text);
    theme->message.text_alpha = get_ini_int(muos_theme, "notification", "MSG_TEXT_ALPHA", theme->message.text_alpha);

    theme->bar.panel_width = get_ini_int(muos_theme, "bar", "BAR_WIDTH", theme->bar.panel_width);
    theme->bar.panel_height = get_ini_int(muos_theme, "bar", "BAR_HEIGHT", theme->bar.panel_height);
    theme->bar.panel_background = get_ini_hex(muos_theme, "bar", "BAR_BACKGROUND", theme->bar.panel_background);
    theme->bar.panel_background_alpha =
        get_ini_int(muos_theme, "bar", "BAR_BACKGROUND_ALPHA", theme->bar.panel_background_alpha);
    theme->bar.panel_border = get_ini_hex(muos_theme, "bar", "BAR_BORDER", theme->bar.panel_border);
    theme->bar.panel_border_alpha = get_ini_int(muos_theme, "bar", "BAR_BORDER_ALPHA", theme->bar.panel_border_alpha);
    theme->bar.panel_border_radius = get_ini_int(muos_theme, "bar", "BAR_RADIUS", theme->bar.panel_border_radius);
    theme->bar.progress_width = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_WIDTH", theme->bar.progress_width);
    theme->bar.progress_height = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_HEIGHT", theme->bar.progress_height);
    theme->bar.progress_main_background =
        get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND", theme->bar.progress_main_background);
    theme->bar.progress_main_background_alpha =
        get_ini_int(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND_ALPHA", theme->bar.progress_main_background_alpha);
    theme->bar.progress_active_background =
        get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND", theme->bar.progress_active_background);
    theme->bar.progress_active_background_alpha = get_ini_int(
        muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND_ALPHA", theme->bar.progress_active_background_alpha
    );
    theme->bar.progress_radius = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_RADIUS", theme->bar.progress_radius);
    theme->bar.icon = get_ini_hex(muos_theme, "bar", "BAR_ICON", theme->bar.icon);
    theme->bar.icon_alpha = get_ini_int(muos_theme, "bar", "BAR_ICON_ALPHA", theme->bar.icon_alpha);
    theme->bar.y_pos = get_ini_int(muos_theme, "bar", "BAR_Y_POS", theme->bar.y_pos);

    theme->roll.text = get_ini_hex(muos_theme, "roll", "ROLL_TEXT", theme->roll.text);
    theme->roll.text_alpha = get_ini_int(muos_theme, "roll", "ROLL_TEXT_ALPHA", theme->roll.text_alpha);
    theme->roll.background = get_ini_hex(muos_theme, "roll", "ROLL_BACKGROUND", theme->roll.background);
    theme->roll.background_alpha =
        get_ini_int(muos_theme, "roll", "ROLL_BACKGROUND_ALPHA", theme->roll.background_alpha);
    theme->roll.radius = get_ini_int(muos_theme, "roll", "ROLL_RADIUS", theme->roll.radius);
    theme->roll.select_text = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_TEXT", theme->roll.select_text);
    theme->roll.select_text_alpha =
        get_ini_int(muos_theme, "roll", "ROLL_SELECT_TEXT_ALPHA", theme->roll.select_text_alpha);
    theme->roll.select_background =
        get_ini_hex(muos_theme, "roll", "ROLL_SELECT_BACKGROUND", theme->roll.select_background);
    theme->roll.select_background_alpha =
        get_ini_int(muos_theme, "roll", "ROLL_SELECT_BACKGROUND_ALPHA", theme->roll.select_background_alpha);
    theme->roll.select_radius = get_ini_int(muos_theme, "roll", "ROLL_SELECT_RADIUS", theme->roll.select_radius);
    theme->roll.border_colour = get_ini_hex(muos_theme, "roll", "ROLL_BORDER_COLOUR", theme->roll.border_colour);
    theme->roll.border_alpha = get_ini_int(muos_theme, "roll", "ROLL_BORDER_ALPHA", theme->roll.border_alpha);
    theme->roll.border_radius = get_ini_int(muos_theme, "roll", "ROLL_BORDER_RADIUS", theme->roll.border_radius);

    theme->counter.alignment = get_ini_uint(muos_theme, "counter", "COUNTER_ALIGNMENT", theme->counter.alignment);
    theme->counter.padding_around =
        get_ini_int(muos_theme, "counter", "COUNTER_PADDING_AROUND", theme->counter.padding_around);
    theme->counter.padding_side =
        get_ini_int(muos_theme, "counter", "COUNTER_PADDING_SIDE", theme->counter.padding_side);
    theme->counter.padding_top = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_TOP", theme->counter.padding_top);
    theme->counter.border_colour =
        get_ini_hex(muos_theme, "counter", "COUNTER_BORDER_COLOUR", theme->counter.border_colour);
    theme->counter.border_alpha =
        get_ini_int(muos_theme, "counter", "COUNTER_BORDER_ALPHA", theme->counter.border_alpha);
    theme->counter.border_width =
        get_ini_int(muos_theme, "counter", "COUNTER_BORDER_WIDTH", theme->counter.border_width);
    theme->counter.radius = get_ini_int(muos_theme, "counter", "COUNTER_RADIUS", theme->counter.radius);
    theme->counter.background = get_ini_hex(muos_theme, "counter", "COUNTER_BACKGROUND", theme->counter.background);
    theme->counter.background_alpha =
        get_ini_int(muos_theme, "counter", "COUNTER_BACKGROUND_ALPHA", theme->counter.background_alpha);
    theme->counter.text = get_ini_hex(muos_theme, "counter", "COUNTER_TEXT", theme->counter.text);
    theme->counter.text_alpha = get_ini_int(muos_theme, "counter", "COUNTER_TEXT_ALPHA", theme->counter.text_alpha);
    theme->counter.text_fade_time =
        get_ini_int(muos_theme, "counter", "COUNTER_TEXT_FADE_TIME", theme->counter.text_fade_time);
    snprintf(
        theme->counter.text_separator, MAX_BUFFER_SIZE, "%s",
        get_ini_string(muos_theme, "counter", "COUNTER_TEXT_SEPARATOR", theme->counter.text_separator)
    );

    theme->misc.static_alignment = get_ini_int(muos_theme, "misc", "STATIC_ALIGNMENT", theme->misc.static_alignment);
    theme->mux.item.count = get_ini_int(muos_theme, "misc", "CONTENT_ITEM_COUNT", theme->mux.item.count);
    theme->mux.item.height = get_ini_int(muos_theme, "misc", "CONTENT_ITEM_HEIGHT", theme->mux.item.height);
    theme->misc.content.size_to_content =
        get_ini_int(muos_theme, "misc", "CONTENT_SIZE_TO_CONTENT", theme->misc.content.size_to_content);
    theme->misc.content.alignment = get_ini_int(muos_theme, "misc", "CONTENT_ALIGNMENT", theme->misc.content.alignment);
    theme->misc.content.padding_left =
        get_ini_int(muos_theme, "misc", "CONTENT_PADDING_LEFT", theme->misc.content.padding_left);
    theme->misc.content.padding_top =
        get_ini_int(muos_theme, "misc", "CONTENT_PADDING_TOP", theme->misc.content.padding_top);
    theme->misc.content.height = get_ini_int(muos_theme, "misc", "CONTENT_HEIGHT", theme->misc.content.height);
    theme->misc.content.width = get_ini_int(
        muos_theme, "misc", "CONTENT_WIDTH",
        config.visual.content_width ? device->screen.width : theme->misc.content.width
    );
    theme->misc.animated_background =
        get_ini_int(muos_theme, "misc", "ANIMATED_BACKGROUND", theme->misc.animated_background);
    theme->misc.random_background = get_ini_int(muos_theme, "misc", "RANDOM_BACKGROUND", theme->misc.random_background);
    theme->misc.image_overlay = get_ini_int(muos_theme, "misc", "IMAGE_OVERLAY", theme->misc.image_overlay);
    theme->misc.navigation_type = get_ini_int(muos_theme, "misc", "NAVIGATION_TYPE", theme->misc.navigation_type);
    theme->misc.antialiasing = get_ini_int(muos_theme, "misc", "ANTIALIASING", theme->misc.antialiasing);
    theme->misc.label_width = get_ini_int(muos_theme, "misc", "LABEL_WIDTH", theme->misc.label_width);

    theme->glyph.list = get_ini_int(muos_theme, "glyph", "LIST", theme->glyph.list);
    theme->glyph.footer = get_ini_int(muos_theme, "glyph", "FOOTER", theme->glyph.footer);
    theme->glyph.header = get_ini_int(muos_theme, "glyph", "HEADER", theme->glyph.header);
    theme->glyph.grid = get_ini_int(muos_theme, "glyph", "GRID", theme->glyph.grid);

    snprintf(
        theme->terminal.font_size, MAX_BUFFER_SIZE, "%s",
        get_ini_string(muos_theme, "terminal", "FONT_SIZE", theme->terminal.font_size)
    );
    snprintf(
        theme->terminal.foreground, MAX_BUFFER_SIZE, "%s",
        get_ini_string(muos_theme, "terminal", "FOREGROUND", theme->terminal.foreground)
    );
    snprintf(
        theme->terminal.background, MAX_BUFFER_SIZE, "%s",
        get_ini_string(muos_theme, "terminal", "BACKGROUND", theme->terminal.background)
    );

    theme->sdl.texture_blend_mode = get_ini_int(muos_theme, "sdl", "TEXTURE_BLEND_MODE", theme->sdl.texture_blend_mode);
    theme->sdl.draw_blend_mode = get_ini_int(muos_theme, "sdl", "DRAW_BLEND_MODE", theme->sdl.draw_blend_mode);
    theme->sdl.render.offset_x = get_ini_float(muos_theme, "sdl", "RENDER_OFFSET_X", theme->sdl.render.offset_x);
    theme->sdl.render.offset_y = get_ini_float(muos_theme, "sdl", "RENDER_OFFSET_Y", theme->sdl.render.offset_y);
    theme->sdl.solid.r = get_ini_int(muos_theme, "sdl", "SOLID_R", theme->sdl.solid.r);
    theme->sdl.solid.g = get_ini_int(muos_theme, "sdl", "SOLID_G", theme->sdl.solid.g);
    theme->sdl.solid.b = get_ini_int(muos_theme, "sdl", "SOLID_B", theme->sdl.solid.b);

    mini_free(muos_theme);
}

int get_alt_scheme_path(char *alt_scheme_path, const size_t alt_scheme_path_size) {
    char active_path[MAX_BUFFER_SIZE];
    snprintf(active_path, sizeof(active_path), "%s/active.txt", theme_base);
    if (file_exist(active_path)) {
        snprintf(
            alt_scheme_path, alt_scheme_path_size, "%s/alternate/%s.ini", theme_base,
            str_replace(read_line_char_from(active_path, 1), "\r", "")
        );
        return file_exist(alt_scheme_path);
    }
    return 0;
}

void scale_theme(struct mux_device *device) {
    const int16_t target_width = device->mux.width;
    const int16_t target_height = device->mux.height;

    const struct {
        int16_t width;
        int16_t height;
    } dims[] = {
        {640, 480}, {720, 480}, {720, 576}, {720, 720}, {1024, 768}, {1280, 720}, {1920, 1080},
    };

    char theme_device_folder[MAX_BUFFER_SIZE];
    for (size_t i = 0; i < A_SIZE(dims); i++) {
        if (target_width == dims[i].height && target_width == dims[i].width) continue;
        snprintf(
            theme_device_folder, sizeof(theme_device_folder), "%s/%dx%d", theme_base, dims[i].width, dims[i].height
        );
        if (!dir_exist(theme_device_folder)) continue;

        // Compare aspect ratios
        if (target_width * dims[i].height == target_height * dims[i].width) {
            device->mux.width = dims[i].width;
            device->mux.height = dims[i].height;
            device->screen.zoom = (float) target_width / (float) dims[i].width;
            device->screen.zoom_width = device->screen.zoom;
            device->screen.zoom_height = device->screen.zoom;
            LOG_INFO(
                mux_module, "Scaling Resolution: %dx%d to %dx%d", dims[i].width, dims[i].height, target_width,
                target_height
            );
            LOG_INFO(mux_module, "Calculated Scale Factor: %.2f", device->screen.zoom);
            return;
        }
    }

    for (size_t i = 0; i < A_SIZE(dims); i++) {
        if (target_width == dims[i].height && target_width == dims[i].width) continue;
        snprintf(
            theme_device_folder, sizeof(theme_device_folder), "%s/%dx%d", theme_base, dims[i].width, dims[i].height
        );
        if (!dir_exist(theme_device_folder)) continue;

        device->mux.width = dims[i].width;
        device->mux.height = dims[i].height;

        // Calculate scale factor to ensure it fits within target dims
        const float scale_width = (float) target_width / (float) dims[i].width;
        const float scale_height = (float) target_height / (float) dims[i].height;
        device->screen.zoom =
            scale_width < scale_height ? scale_width : scale_height; // Ensure neither dimension exceeds target
        device->screen.zoom_width = scale_width;
        device->screen.zoom_height = scale_height;
        LOG_INFO(
            mux_module, "Scaling Resolution: %dx%d to %dx%d", dims[i].width, dims[i].height, target_width, target_height
        );
        LOG_INFO(mux_module, "Calculated Scale Factor: %.2f", device->screen.zoom);
        return;
    }
}

void load_theme(struct theme_config *theme, const struct mux_config *config, struct mux_device *device) {
    char scheme[MAX_BUFFER_SIZE];
    snprintf(mux_dim, sizeof(mux_dim), "%dx%d/", device->mux.width, device->mux.height);

    // If theme does not support device resolution fallback to default but only after factory reset
    if (!config->boot.factory_reset) {
        char theme_device_folder[MAX_BUFFER_SIZE];

        if (config->settings.general.theme_resolution > 0) {
            snprintf(
                theme_device_folder, sizeof(theme_device_folder), "%s/%dx%d", theme_base,
                config->settings.general.theme_resolution_width, config->settings.general.theme_resolution_height
            );
            if (dir_exist(theme_device_folder)) {
                device->mux.width = config->settings.general.theme_resolution_width;
                device->mux.height = config->settings.general.theme_resolution_height;
                snprintf(mux_dim, sizeof(mux_dim), "%dx%d/", device->mux.width, device->mux.height);

                const float scale_width = (float) device->screen.width / (float) device->mux.width;
                const float scale_height = (float) device->screen.height / (float) device->mux.height;

                // Ensure neither dimension exceeds target
                device->screen.zoom = scale_width < scale_height ? scale_width : scale_height;
                device->screen.zoom_width = scale_width;
                device->screen.zoom_height = scale_height;
                LOG_INFO(mux_module, "Calculated Scale Factor: %.2f", device->screen.zoom);
            }
        }

        snprintf(theme_device_folder, sizeof(theme_device_folder), "%s/%s", theme_base, mux_dim);
        if (!dir_exist(theme_device_folder)) scale_theme(device);

        LOG_INFO("muxfrontend", "Loading Theme Resolution: %dx%d", device->mux.width, device->mux.height);
    }

    init_theme_config(theme, device);
    const char *schemes[] = {"global", "default", mux_module};
    int scheme_loaded = 0;

    for (size_t i = 0; i < A_SIZE(schemes); i++) {
        if (load_scheme(theme_base, mux_dim, schemes[i], scheme, sizeof(scheme))) {
            scheme_loaded = 1;
            load_theme_from_scheme(scheme, theme, device);
            LOG_INFO("muxfrontend", "Loading Theme Scheme: %s", scheme);
        }
    }

    if (scheme_loaded) {
        char alternate_scheme_path[MAX_BUFFER_SIZE];
        if (get_alt_scheme_path(alternate_scheme_path, sizeof(alternate_scheme_path))) {
            load_theme_from_scheme(alternate_scheme_path, theme, device);
        }
    }

    char scheme_override[MAX_BUFFER_SIZE];
    snprintf(scheme_override, sizeof(scheme_override), RUN_STORAGE_PATH "theme/override/%s.ini", mux_module);
    if (file_exist(scheme_override)) load_theme_from_scheme(scheme_override, theme, device);

    theme->grid.enabled = theme->grid.column_count > 0 && theme->grid.row_count > 0;
    if (theme->misc.content.width == 0) theme->misc.content.width = device->mux.width;
    if (theme->misc.content.height > device->mux.height) theme->misc.content.height = device->mux.height;
    if (theme->mux.item.count < 1) theme->mux.item.count = 1;
    if (theme->mux.item.height > 0) {
        theme->mux.item.panel = (int16_t) (theme->mux.item.height + 2);
        theme->mux.item.count = (int16_t) (theme->misc.content.height / theme->mux.item.panel);
    } else {
        theme->mux.item.panel = (int16_t) (theme->misc.content.height / theme->mux.item.count);
        theme->mux.item.height = (int16_t) (theme->mux.item.panel - 2);
    }

    // Adjusts height if user picks a height that is not evenly divisible by item count.
    // Prevents seeing a few pixels of the next item.
    theme->misc.content.height = (int16_t) (theme->mux.item.panel * theme->mux.item.count);

    if (config->settings.themeopt.header_height >= 0)
        theme->header.height = (int16_t) config->settings.themeopt.header_height;
    if (config->settings.themeopt.footer_height >= 0)
        theme->footer.height = (int16_t) config->settings.themeopt.footer_height;

    const int any_theme_opt = config->settings.themeopt.header_height >= 0
                              || config->settings.themeopt.footer_height >= 0
                              || config->settings.themeopt.content_item_count > 0;

    if (any_theme_opt) {
        theme->misc.content.height = (int16_t) (device->mux.height - theme->header.height - theme->footer.height - 4);
        if (theme->misc.content.height < 0) theme->misc.content.height = 0;

        if (config->settings.themeopt.content_item_count > 0) {
            theme->mux.item.count = (int16_t) config->settings.themeopt.content_item_count;
            if (theme->mux.item.count < 1) theme->mux.item.count = 1;
            theme->mux.item.panel = (int16_t) (theme->misc.content.height / theme->mux.item.count);
            theme->mux.item.height = (int16_t) (theme->mux.item.panel - 2);
        } else {
            theme->mux.item.panel = (int16_t) (theme->mux.item.height + 2);
            if (theme->mux.item.panel > 0)
                theme->mux.item.count = (int16_t) (theme->misc.content.height / theme->mux.item.panel);
        }

        if (theme->mux.item.count < 1) theme->mux.item.count = 1;
        theme->misc.content.height = (int16_t) (theme->mux.item.panel * theme->mux.item.count);
    }

    // When auto SVG glyph size is active we scale to 75% height for breathing room.
    // Then expand the LIST_PAD_LEFT value so there is at least 4px of gap the sides
    // of the glyph, then center GLYPH_PADDING_LEFT in the column for equal space on
    // either side of the glyph, just to make it nice looking
    int16_t eff_glyph_size = config->settings.themeopt.glyph_size_list;
    if (eff_glyph_size == -2) eff_glyph_size = theme->glyph.list;

    if (eff_glyph_size == 0 && theme->mux.item.height > 0) {
        const int16_t auto_size = (int16_t) (theme->mux.item.height * 3 / 4);
        const int16_t half_auto = (int16_t) (auto_size / 2);
        const int16_t needed = (int16_t) (auto_size + 6);

        if (needed > theme->font.list_pad_left) theme->font.list_pad_left = needed;
        int16_t glyph_center = (int16_t) (theme->font.list_pad_left / 2);

        if (glyph_center < half_auto) glyph_center = half_auto;
        theme->list_default.glyph_padding_left = (int16_t) (glyph_center + 4);
    }
}

static void apply_label_scroll_speed(lv_obj_t *ui_lbl_item, const int is_bounce) {
    static int inited = 0;
    static lv_anim_t tpl[3][2]; // [speed_idx][is_bounce]

    if (!inited) {
        static const uint32_t delays_cont[3] = {2000, 1000, 500};
        static const uint32_t delays_bounce[3] = {3000, 2000, 1000};
        for (int s = 0; s < 3; s++) {
            lv_anim_init(&tpl[s][0]);
            lv_anim_set_delay(&tpl[s][0], delays_cont[s]);
            lv_anim_init(&tpl[s][1]);
            lv_anim_set_delay(&tpl[s][1], delays_bounce[s]);
        }
        inited = 1;
    }

    int speed_level = config.visual.label_scroll_speed;
    if (speed_level < 1 || speed_level > 3) speed_level = 2;
    const int idx = speed_level - 1;

    static const uint32_t px_per_sec[3] = {25, 60, 100};
    lv_obj_set_style_anim(ui_lbl_item, &tpl[idx][is_bounce ? 1 : 0], 0);
    lv_obj_set_style_anim_speed(ui_lbl_item, px_per_sec[idx], 0);
}

void set_label_long_mode(const struct theme_config *theme, lv_obj_t *ui_lbl_item, const int scroll_mode) {
    if (theme->list_default.label_long_mode == LV_LABEL_LONG_WRAP) return;
    if (scroll_mode == 0 || config.visual.label_scroll_speed == 0) return;

    // Only do the scroll animation if the label is dotted
    lv_obj_update_layout(ui_lbl_item);
    lv_label_set_long_mode(ui_lbl_item, LV_LABEL_LONG_DOT);
    if (!lv_label_is_text_dotted(ui_lbl_item)) return;

    const int is_bounce = scroll_mode == 2;
    apply_label_scroll_speed(ui_lbl_item, is_bounce);
    lv_label_set_long_mode(ui_lbl_item, is_bounce ? LV_LABEL_LONG_SCROLL : LV_LABEL_LONG_SCROLL_CIRCULAR);
}

void apply_text_long_dot(const struct theme_config *theme, lv_obj_t *ui_lbl_item) {
    if (theme->list_default.label_long_mode == LV_LABEL_LONG_WRAP) return;

    lv_label_set_long_mode(ui_lbl_item, LV_LABEL_LONG_DOT);
}

void apply_size_to_content(
    const struct theme_config *c_theme, const lv_obj_t *ui_pnl_content, lv_obj_t *ui_lbl_item,
    lv_obj_t *ui_lbl_item_glyph, const char *item_text
) {
    if (c_theme->misc.content.size_to_content) {
        lv_obj_t *ui_pnl_item = lv_obj_get_parent(ui_lbl_item);
        lv_obj_set_width(ui_pnl_item, LV_SIZE_CONTENT);
        lv_obj_get_style_max_width(ui_pnl_item, c_theme->misc.content.width);

        const lv_font_t *font = lv_obj_get_style_text_font(ui_pnl_content, LV_PART_MAIN);
        const lv_coord_t letter_space = lv_obj_get_style_text_letter_space(ui_pnl_content, LV_PART_MAIN);

        const lv_coord_t act_line_length =
            lv_txt_get_width(item_text, strlen(item_text), font, letter_space, LV_TEXT_FLAG_EXPAND);
        const int item_width = LV_MIN(
            c_theme->font.list_pad_left + act_line_length + c_theme->font.list_pad_right,
            c_theme->misc.content.width - (c_theme->list_default.border_width * 2)
        );

        // When using size to content right padding needs to be zero to prevent text from wrapping.
        // The overall width of the control will include the right padding
        lv_obj_set_style_pad_right(ui_lbl_item, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_width(ui_lbl_item, item_width);

        lv_obj_set_x(
            ui_lbl_item_glyph,
            c_theme->list_default.glyph_padding_left - item_width / 2 - c_theme->list_default.border_width
        );
    }
}

void apply_theme_list_panel(lv_obj_t *ui_pnl_list) {
    lv_obj_set_scrollbar_mode(ui_pnl_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_style(ui_pnl_list, &style_list_panel_default, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_style(ui_pnl_list, &style_list_panel_focused, MU_OBJ_MAIN_FOCUS);
}

void apply_theme_list_item(const struct theme_config *theme, lv_obj_t *ui_lbl_item, const char *item_text) {
    lv_label_set_text(ui_lbl_item, item_text);
    lv_label_set_long_mode(ui_lbl_item, LV_LABEL_LONG_WRAP);

    lv_obj_add_style(ui_lbl_item, &style_list_item_default, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_style(ui_lbl_item, &style_list_item_focused, MU_OBJ_MAIN_FOCUS);

    if (theme->list_default.label_long_mode == LV_LABEL_LONG_WRAP) {
        lv_obj_set_height(ui_lbl_item, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_height(ui_lbl_item, lv_font_get_line_height(lv_obj_get_style_text_font(ui_lbl_item, LV_PART_MAIN)));
    }
}

void apply_theme_option_item_label(const struct theme_config *theme, lv_obj_t *ui_lbl_item, const char *item_text) {
    apply_theme_list_item(theme, ui_lbl_item, item_text);

    const int label_pct =
        config.settings.themeopt.label_width > 0 ? config.settings.themeopt.label_width : theme->misc.label_width;
    const lv_coord_t max_w = theme->misc.content.width * label_pct / 100;
    lv_obj_set_style_max_width(ui_lbl_item, max_w, MU_OBJ_MAIN_DEFAULT);

    lv_label_set_long_mode(ui_lbl_item, LV_LABEL_LONG_DOT);
    lv_obj_set_height(ui_lbl_item, lv_font_get_line_height(lv_obj_get_style_text_font(ui_lbl_item, LV_PART_MAIN)));
}

void apply_option_label_long_dot(lv_obj_t *ui_lbl_item) {
    lv_label_set_long_mode(ui_lbl_item, LV_LABEL_LONG_DOT);
}

void set_option_label_scroll_mode(lv_obj_t *ui_lbl_item) {
    const int scroll_mode = config.visual.name_scroll;
    if (scroll_mode == 0 || config.visual.label_scroll_speed == 0) return;

    lv_obj_update_layout(ui_lbl_item);
    lv_label_set_long_mode(ui_lbl_item, LV_LABEL_LONG_DOT);
    if (!lv_label_is_text_dotted(ui_lbl_item)) return;

    const int is_bounce = scroll_mode == 2;
    apply_label_scroll_speed(ui_lbl_item, is_bounce);
    lv_label_set_long_mode(ui_lbl_item, is_bounce ? LV_LABEL_LONG_SCROLL : LV_LABEL_LONG_SCROLL_CIRCULAR);
}

void apply_theme_list_value(const struct theme_config *lv_theme, lv_obj_t *ui_lbl_item_value, const char *item_text) {
    lv_label_set_text(ui_lbl_item_value, item_text);

    lv_obj_set_width(ui_lbl_item_value, lv_theme->misc.content.width);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_lbl_item_value, LV_PART_MAIN);
    const lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_lbl_item_value, font_height);
    lv_obj_set_align(ui_lbl_item_value, LV_ALIGN_RIGHT_MID);

    lv_obj_set_style_text_color(ui_lbl_item_value, lv_color_hex(lv_theme->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_item_value, lv_theme->list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_text_color(ui_lbl_item_value, lv_color_hex(lv_theme->list_focus.text), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lbl_item_value, lv_theme->list_focus.text_alpha, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_pad_left(ui_lbl_item_value, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_item_value, lv_theme->font.list_pad_right, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_item_value, lv_theme->font.list_pad_top, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_item_value, lv_theme->font.list_pad_bottom, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_text_line_space(ui_lbl_item_value, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_item_value, LV_TEXT_ALIGN_RIGHT, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_long_mode(ui_lbl_item_value, LV_LABEL_LONG_WRAP);

    lv_obj_set_style_radius(ui_lbl_item_value, lv_theme->list_default.radius, MU_OBJ_MAIN_DEFAULT);
}

void apply_theme_list_drop_down(const struct theme_config *d_theme, lv_obj_t *ui_lbl_item_value, const char *options) {
    lv_dropdown_set_dir(ui_lbl_item_value, LV_DIR_LEFT);
    if (options != NULL) lv_dropdown_set_options(ui_lbl_item_value, options);
    lv_dropdown_set_selected_highlight(ui_lbl_item_value, 0);
    lv_obj_set_width(ui_lbl_item_value, d_theme->misc.content.width);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_lbl_item_value, LV_PART_MAIN);
    const lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_lbl_item_value, font_height);
    lv_obj_set_align(ui_lbl_item_value, LV_ALIGN_RIGHT_MID);
    lv_obj_add_flag(ui_lbl_item_value, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_scrollbar_mode(ui_lbl_item_value, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lbl_item_value, LV_DIR_RIGHT);
    lv_obj_set_style_text_color(ui_lbl_item_value, lv_color_hex(d_theme->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_item_value, d_theme->list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_item_value, lv_color_hex(d_theme->list_focus.text), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lbl_item_value, d_theme->list_focus.text_alpha, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_bg_color(ui_lbl_item_value, lv_color_hex(0x403A03), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lbl_item_value, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lbl_item_value, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_item_value, d_theme->font.list_pad_right, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_item_value, d_theme->font.list_pad_top, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_item_value, d_theme->font.list_pad_bottom, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_item_value, lv_color_hex(0xF7E318), MU_OBJ_MAIN_SCROLL);
    lv_obj_set_style_text_opa(ui_lbl_item_value, LV_OPA_COVER, MU_OBJ_MAIN_SCROLL);
    lv_obj_set_style_text_color(ui_lbl_item_value, lv_color_hex(0x808080), MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_item_value, LV_OPA_TRANSP, MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_color(lv_dropdown_get_list(ui_lbl_item_value), lv_color_hex(0x02080D), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(lv_dropdown_get_list(ui_lbl_item_value), LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(lv_dropdown_get_list(ui_lbl_item_value), lv_color_hex(0xF8E008), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(lv_dropdown_get_list(ui_lbl_item_value), LV_OPA_COVER, MU_OBJ_SELECT_DEFAULT);
}

void apply_theme_list_glyph(
    const struct theme_config *theme, lv_obj_t *ui_lbl_item_glyph, const char *screen_name, const char *item_glyph
) {
    if (!config.visual.list_glyph) return;
    if (theme->list_default.glyph_alpha == 0 && theme->list_focus.glyph_alpha == 0) return;

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (get_glyph_path(screen_name, item_glyph, glyph_image_embed, MAX_BUFFER_SIZE)) {
        set_list_glyph_image(ui_lbl_item_glyph, glyph_image_embed);
    }

    lv_obj_add_style(ui_lbl_item_glyph, &style_list_glyph_default, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_style(ui_lbl_item_glyph, &style_list_glyph_focused, MU_OBJ_MAIN_FOCUS);
}

void apply_pass_theme(
    lv_obj_t *ui_rol_combo_one, lv_obj_t *ui_rol_combo_two, lv_obj_t *ui_rol_combo_three, lv_obj_t *ui_rol_combo_four,
    lv_obj_t *ui_rol_combo_five, lv_obj_t *ui_rol_combo_six
) {
    const struct pt_big roll_text_elements[] = {
        {ui_rol_combo_one, theme.roll.text},   {ui_rol_combo_two, theme.roll.text},
        {ui_rol_combo_three, theme.roll.text}, {ui_rol_combo_four, theme.roll.text},
        {ui_rol_combo_five, theme.roll.text},  {ui_rol_combo_six, theme.roll.text},
    };
    for (size_t i = 0; i < A_SIZE(roll_text_elements); ++i) {
        lv_obj_set_style_text_color(
            roll_text_elements[i].e, lv_color_hex(roll_text_elements[i].c), MU_OBJ_MAIN_DEFAULT
        );
    }

    const struct pt_big roll_selected_text_elements[] = {
        {ui_rol_combo_one, theme.roll.select_text},   {ui_rol_combo_two, theme.roll.select_text},
        {ui_rol_combo_three, theme.roll.select_text}, {ui_rol_combo_four, theme.roll.select_text},
        {ui_rol_combo_five, theme.roll.select_text},  {ui_rol_combo_six, theme.roll.select_text},
    };
    for (size_t i = 0; i < A_SIZE(roll_selected_text_elements); ++i) {
        lv_obj_set_style_text_color(
            roll_selected_text_elements[i].e, lv_color_hex(roll_selected_text_elements[i].c), MU_OBJ_SELECT_DEFAULT
        );
    }

    const struct pt_big roll_text_alpha_elements[] = {
        {ui_rol_combo_one, theme.roll.text_alpha},   {ui_rol_combo_two, theme.roll.text_alpha},
        {ui_rol_combo_three, theme.roll.text_alpha}, {ui_rol_combo_four, theme.roll.text_alpha},
        {ui_rol_combo_five, theme.roll.text_alpha},  {ui_rol_combo_six, theme.roll.text_alpha},
    };
    for (size_t i = 0; i < A_SIZE(roll_text_alpha_elements); ++i) {
        lv_obj_set_style_text_opa(roll_text_alpha_elements[i].e, roll_text_alpha_elements[i].c, MU_OBJ_MAIN_DEFAULT);
    }

    const struct pt_big roll_selected_text_alpha_elements[] = {
        {ui_rol_combo_one, theme.roll.select_text_alpha},   {ui_rol_combo_two, theme.roll.select_text_alpha},
        {ui_rol_combo_three, theme.roll.select_text_alpha}, {ui_rol_combo_four, theme.roll.select_text_alpha},
        {ui_rol_combo_five, theme.roll.select_text_alpha},  {ui_rol_combo_six, theme.roll.select_text_alpha},
    };
    for (size_t i = 0; i < A_SIZE(roll_selected_text_alpha_elements); ++i) {
        lv_obj_set_style_text_opa(
            roll_selected_text_alpha_elements[i].e, roll_selected_text_alpha_elements[i].c, MU_OBJ_SELECT_DEFAULT
        );
    }

    const struct pt_big roll_background_elements[] = {
        {ui_rol_combo_one, theme.roll.background},   {ui_rol_combo_two, theme.roll.background},
        {ui_rol_combo_three, theme.roll.background}, {ui_rol_combo_four, theme.roll.background},
        {ui_rol_combo_five, theme.roll.background},  {ui_rol_combo_six, theme.roll.background},
    };
    for (size_t i = 0; i < A_SIZE(roll_background_elements); ++i) {
        lv_obj_set_style_bg_color(
            roll_background_elements[i].e, lv_color_hex(roll_background_elements[i].c), MU_OBJ_SELECT_DEFAULT
        );
    }

    const struct pt_small roll_background_alpha_elements[] = {
        {ui_rol_combo_one, theme.roll.background_alpha},   {ui_rol_combo_two, theme.roll.background_alpha},
        {ui_rol_combo_three, theme.roll.background_alpha}, {ui_rol_combo_four, theme.roll.background_alpha},
        {ui_rol_combo_five, theme.roll.background_alpha},  {ui_rol_combo_six, theme.roll.background_alpha},
    };
    for (size_t i = 0; i < A_SIZE(roll_background_alpha_elements); ++i) {
        lv_obj_set_style_bg_opa(
            roll_background_alpha_elements[i].e, roll_background_alpha_elements[i].c, MU_OBJ_SELECT_DEFAULT
        );
    }

    const struct pt_big roll_selected_background_elements[] = {
        {ui_rol_combo_one, theme.roll.select_background},   {ui_rol_combo_two, theme.roll.select_background},
        {ui_rol_combo_three, theme.roll.select_background}, {ui_rol_combo_four, theme.roll.select_background},
        {ui_rol_combo_five, theme.roll.select_background},  {ui_rol_combo_six, theme.roll.select_background},
    };
    for (size_t i = 0; i < A_SIZE(roll_selected_background_elements); ++i) {
        lv_obj_set_style_bg_color(
            roll_selected_background_elements[i].e, lv_color_hex(roll_selected_background_elements[i].c),
            LV_PART_SELECTED | LV_STATE_FOCUSED
        );
    }

    const struct pt_small roll_selected_background_alpha_elements[] = {
        {ui_rol_combo_one, theme.roll.select_background_alpha},
        {ui_rol_combo_two, theme.roll.select_background_alpha},
        {ui_rol_combo_three, theme.roll.select_background_alpha},
        {ui_rol_combo_four, theme.roll.select_background_alpha},
        {ui_rol_combo_five, theme.roll.select_background_alpha},
        {ui_rol_combo_six, theme.roll.select_background_alpha},
    };
    for (size_t i = 0; i < A_SIZE(roll_selected_background_alpha_elements); ++i) {
        lv_obj_set_style_bg_opa(
            roll_selected_background_alpha_elements[i].e, roll_selected_background_alpha_elements[i].c,
            LV_PART_SELECTED | LV_STATE_FOCUSED
        );
    }

    const struct pt_small roll_radius_elements[] = {
        {ui_rol_combo_one, theme.roll.radius},   {ui_rol_combo_two, theme.roll.radius},
        {ui_rol_combo_three, theme.roll.radius}, {ui_rol_combo_four, theme.roll.radius},
        {ui_rol_combo_five, theme.roll.radius},  {ui_rol_combo_six, theme.roll.radius},
    };
    for (size_t i = 0; i < A_SIZE(roll_radius_elements); ++i) {
        lv_obj_set_style_radius(roll_radius_elements[i].e, roll_radius_elements[i].c, MU_OBJ_SELECT_DEFAULT);
    }
    const struct pt_small roll_selected_radius_elements[] = {
        {ui_rol_combo_one, theme.roll.select_radius},   {ui_rol_combo_two, theme.roll.select_radius},
        {ui_rol_combo_three, theme.roll.select_radius}, {ui_rol_combo_four, theme.roll.select_radius},
        {ui_rol_combo_five, theme.roll.select_radius},  {ui_rol_combo_six, theme.roll.select_radius},
    };
    for (size_t i = 0; i < A_SIZE(roll_selected_radius_elements); ++i) {
        lv_obj_set_style_radius(
            roll_selected_radius_elements[i].e, roll_selected_radius_elements[i].c, LV_PART_SELECTED | LV_STATE_FOCUSED
        );
    }

    const struct pt_small roll_border_radius_elements[] = {
        {ui_rol_combo_one, theme.roll.border_radius},   {ui_rol_combo_two, theme.roll.border_radius},
        {ui_rol_combo_three, theme.roll.border_radius}, {ui_rol_combo_four, theme.roll.border_radius},
        {ui_rol_combo_five, theme.roll.border_radius},  {ui_rol_combo_six, theme.roll.border_radius},
    };
    for (size_t i = 0; i < A_SIZE(roll_border_radius_elements); ++i) {
        lv_obj_set_style_radius(roll_border_radius_elements[i].e, roll_border_radius_elements[i].c, MU_OBJ_MAIN_FOCUS);
    }

    const struct pt_big roll_border_colour_elements[] = {
        {ui_rol_combo_one, theme.roll.border_colour},   {ui_rol_combo_two, theme.roll.border_colour},
        {ui_rol_combo_three, theme.roll.border_colour}, {ui_rol_combo_four, theme.roll.border_colour},
        {ui_rol_combo_five, theme.roll.border_colour},  {ui_rol_combo_six, theme.roll.border_colour},
    };
    for (size_t i = 0; i < A_SIZE(roll_border_colour_elements); ++i) {
        lv_obj_set_style_outline_color(
            roll_border_colour_elements[i].e, lv_color_hex(roll_border_colour_elements[i].c), MU_OBJ_MAIN_FOCUS
        );
    }

    const struct pt_small roll_border_alpha_elements[] = {
        {ui_rol_combo_one, theme.roll.border_alpha},   {ui_rol_combo_two, theme.roll.border_alpha},
        {ui_rol_combo_three, theme.roll.border_alpha}, {ui_rol_combo_four, theme.roll.border_alpha},
        {ui_rol_combo_five, theme.roll.border_alpha},  {ui_rol_combo_six, theme.roll.border_alpha},
    };
    for (size_t i = 0; i < A_SIZE(roll_border_alpha_elements); ++i) {
        lv_obj_set_style_outline_opa(
            roll_border_alpha_elements[i].e, roll_border_alpha_elements[i].c, MU_OBJ_MAIN_FOCUS
        );
    }
}

void init_panel_style(const struct theme_config *theme) {
    lv_style_init(&style_list_panel_default);
    lv_style_init(&style_list_panel_focused);

    // List panel default style
    lv_style_set_width(&style_list_panel_default, theme->misc.content.width);
    lv_style_set_height(&style_list_panel_default, theme->mux.item.height);

    lv_style_set_border_width(&style_list_panel_default, theme->list_default.border_width);
    lv_style_set_border_side(&style_list_panel_default, theme->list_default.border_side);
    lv_style_set_border_color(&style_list_panel_default, lv_color_hex(theme->list_default.indicator));
    lv_style_set_border_opa(&style_list_panel_default, theme->list_default.indicator_alpha);

    lv_style_set_bg_main_stop(&style_list_panel_default, theme->list_default.gradient_start);
    lv_style_set_bg_color(&style_list_panel_default, lv_color_hex(theme->list_default.background));
    lv_style_set_bg_opa(&style_list_panel_default, theme->list_default.background_alpha);

    lv_style_set_bg_grad_color(&style_list_panel_default, lv_color_hex(theme->list_default.background_gradient));
    lv_style_set_bg_grad_dir(
        &style_list_panel_default, theme->list_default.gradient_direction == 1 ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR
    );
    lv_style_set_bg_grad_stop(&style_list_panel_default, theme->list_default.gradient_stop);

    lv_style_set_pad_left(&style_list_panel_default, 0);
    lv_style_set_pad_right(&style_list_panel_default, 0);
    lv_style_set_pad_top(&style_list_panel_default, 0);
    lv_style_set_pad_bottom(&style_list_panel_default, 0);
    lv_style_set_pad_row(&style_list_panel_default, 0);
    lv_style_set_pad_column(&style_list_panel_default, 0);

    lv_style_set_radius(&style_list_panel_default, theme->list_default.radius);

    // List panel focused style
    lv_style_set_border_width(&style_list_panel_focused, theme->list_focus.border_width);
    lv_style_set_border_side(&style_list_panel_focused, theme->list_focus.border_side);
    lv_style_set_border_color(&style_list_panel_focused, lv_color_hex(theme->list_focus.indicator));
    lv_style_set_border_opa(&style_list_panel_focused, theme->list_focus.indicator_alpha);

    lv_style_set_bg_main_stop(&style_list_panel_focused, theme->list_focus.gradient_start);
    lv_style_set_bg_color(&style_list_panel_focused, lv_color_hex(theme->list_focus.background));
    lv_style_set_bg_opa(&style_list_panel_focused, theme->list_focus.background_alpha);

    lv_style_set_bg_grad_color(&style_list_panel_focused, lv_color_hex(theme->list_focus.background_gradient));
    lv_style_set_bg_grad_dir(
        &style_list_panel_focused, theme->list_focus.gradient_direction == 1 ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR
    );
    lv_style_set_bg_grad_stop(&style_list_panel_focused, theme->list_focus.gradient_stop);
}

void init_item_animation(void) {
    lv_anim_init(&style_list_item_animation);
    lv_anim_set_delay(&style_list_item_animation, 250);
    lv_style_set_anim(&style_list_item_default, &style_list_item_animation);
    lv_style_set_anim_speed(&style_list_item_default, 60);
}

void init_item_style(struct theme_config *theme) {
    if (!config.visual.list_glyph) theme->font.list_pad_left = 6;

    lv_style_init(&style_list_item_default);
    lv_style_init(&style_list_item_focused);

    lv_style_set_width(&style_list_item_default, theme->misc.content.width - theme->font.list_pad_right);
    lv_style_set_align(&style_list_item_default, LV_ALIGN_LEFT_MID);

    lv_style_set_text_color(&style_list_item_default, lv_color_hex(theme->list_default.text));
    lv_style_set_text_opa(&style_list_item_default, theme->list_default.text_alpha);

    lv_style_set_pad_left(&style_list_item_default, theme->font.list_pad_left);
    lv_style_set_pad_top(&style_list_item_default, theme->font.list_pad_top);

    lv_style_set_text_color(&style_list_item_focused, lv_color_hex(theme->list_focus.text));
    lv_style_set_text_opa(&style_list_item_focused, theme->list_focus.text_alpha);
}

void init_glyph_style(const struct theme_config *theme) {
    lv_style_init(&style_list_glyph_default);
    lv_style_init(&style_list_glyph_focused);

    lv_style_set_x(&style_list_glyph_default, theme->list_default.glyph_padding_left - theme->misc.content.width / 2);
    lv_style_set_align(&style_list_glyph_default, LV_ALIGN_CENTER);

    lv_style_set_img_opa(&style_list_glyph_default, theme->list_default.glyph_alpha);
    lv_style_set_img_recolor(&style_list_glyph_default, lv_color_hex(theme->list_default.glyph_recolour));
    lv_style_set_img_recolor_opa(&style_list_glyph_default, theme->list_default.glyph_recolour_alpha);

    lv_style_set_pad_top(&style_list_glyph_default, theme->font.list_icon_pad_top);
    lv_style_set_pad_bottom(&style_list_glyph_default, theme->font.list_icon_pad_bottom);

    lv_style_set_img_opa(&style_list_glyph_focused, theme->list_focus.glyph_alpha);
    lv_style_set_img_recolor(&style_list_glyph_focused, lv_color_hex(theme->list_focus.glyph_recolour));
    lv_style_set_img_recolor_opa(&style_list_glyph_focused, theme->list_focus.glyph_recolour_alpha);
}

int get_theme_preview_path(
    char *base_path, char *base_file_name, char *image_path, const size_t image_path_size, const int preview_index
) {
    char preview_suffix[MAX_BUFFER_SIZE];
    char preview_path[MAX_BUFFER_SIZE];
    char fallback_path[MAX_BUFFER_SIZE];

    const char *suffixes[2];
    size_t count = 0;

    if (preview_index >= 0) {
        snprintf(preview_suffix, sizeof(preview_suffix), ".%d", preview_index);
        suffixes[count++] = preview_suffix;
    }

    suffixes[count++] = "";

    for (int i = 0; i < count; i++) {
        snprintf(preview_path, sizeof(preview_path), "%s/%s%s%s.png", base_path, mux_dim, base_file_name, suffixes[i]);

        snprintf(fallback_path, sizeof(fallback_path), "%s/640x480/%s%s.png", base_path, base_file_name, suffixes[i]);

        if (!file_exist(preview_path) && !file_exist(fallback_path)) {
            snprintf(image_path, image_path_size, "%s", "");
        } else {
            snprintf(image_path, image_path_size, "%s", file_exist(preview_path) ? preview_path : fallback_path);
            return i;
        }
    }
    return -1;
}
