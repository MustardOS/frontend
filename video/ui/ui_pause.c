#include "ui_pause.h"
#include "state.h"
#include "settings.h"
#include "assets.h"
#include "catalogue.h"
#include "session.h"
#include "player.h"
#include "video.h"
#include "effects.h"
#include "ui_progress.h"
#include "../core/equaliser.h"
#include "ui_audio.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <module/muxshare.h>
#include <common/display/datetime.h>
#include <common/content/switcher.h>
#include <common/base/randname.h>
#include <common/platform/battery.h>
#include <common/ui/image.h>
#include <common/ui/dialogue.h>
#include <common/ui/list_frame.h>
#include <common/ui/osk.h>

enum { bookmark_view_limit = 256 };

static int menu_active;
static int settings_active;
static int bookmarks_active;
static int playlist_active;
static int information_active;
static int content_switch_active;
static wasabi_settings_page settings_page;
static int playback_paused;
static int live_content;
static int bookmark_row;
static int playlist_row;
static int content_switch_row;
static int information_row;
static int restart_row;
static int settings_row;
static int equaliser_row;
static int stop_row;
static int settings_parent_row;
static int *remembered_row = NULL;
static int shader_parameters_from_browser;
static int menu_peeking;
static int menu_combo_consumed;
static uint32_t menu_pressed_at;
static uint32_t playtime_started;
static uint32_t playtime_shown = UINT32_MAX;
static uint32_t header_deadline;
static uint32_t status_deadline;
static uint32_t mode_deadline;
static uint32_t timeline_deadline;
static double playback_position;
static double playback_duration;
static char playback_title[PATH_MAX];
static char playback_uri[PATH_MAX];
static char playback_content_uri[PATH_MAX];
static content_switch_list content_switch_items;
static video_state_entry *bookmark_entries;
static size_t bookmark_entry_count;
static size_t bookmark_indices[bookmark_view_limit];
static size_t bookmark_count;
static double selected_bookmark_position = -1.0;
static const video_library_entry *playback_playlist;
static size_t playback_playlist_count;
static size_t playback_playlist_index;
static size_t selected_playlist_index;
static size_t playlist_window_start;
static size_t playlist_focus_index;
static int playback_playlist_channels;
static int playback_playlist_audio;
static int naming_active;
static lv_obj_t *name_panel;
static lv_obj_t *name_entry;
static lv_obj_t *dim_overlay;
static lv_obj_t *playtime_panel;
static lv_obj_t *playtime_label;
static lv_obj_t *status_panel;
static lv_obj_t *status_glyph;
static lv_obj_t *status_label;
static lv_obj_t *mode_panel;
static lv_obj_t *shuffle_glyph;
static lv_obj_t *repeat_glyph;
static lv_obj_t *repeat_badge;
static lv_obj_t *timeline_panel;
static lv_obj_t *timeline_track;
static lv_obj_t *timeline_current;
static lv_obj_t *timeline_total;
static wasabi_progress timeline_progress;
static int timeline_width;
static int timeline_padding_x;
static int timeline_padding_y;
static lv_obj_t *setting_panels[wasabi_setting_count];
static lv_obj_t *setting_labels[wasabi_setting_count];
static lv_obj_t *setting_glyphs[wasabi_setting_count];
static lv_obj_t *setting_values[wasabi_setting_count];
static list_frame setting_frames[7];
enum { information_row_limit = 32, information_frame_limit = 4 };
static lv_obj_t *information_panels[information_row_limit];
static lv_obj_t *information_labels[information_row_limit];
static lv_obj_t *information_glyphs[information_row_limit];
static lv_obj_t *information_values[information_row_limit];
static char information_value_text[information_row_limit][128];
static list_frame information_frames[information_frame_limit];
static int information_count;
static unsigned catalogue_revision;
static uint32_t vignette_secret_since[2];
static int vignette_secret_holding[2];
static int vignette_secret_fired[2];
static mux_dialogue asset_actions_dialogue;
static mux_dialogue asset_delete_dialogue;
static mux_dialogue settings_save_dialogue;
static mux_dialogue settings_reset_dialogue;
static mux_dialogue bookmark_delete_dialogue;
static mux_dialogue equaliser_delete_dialogue;
static int asset_delete_skip_confirm;
static char asset_entry_key[MAX_BUFFER_SIZE];
static int asset_entry_overlay_mode;
static int suppress_asset_preview_once;
static int save_return_to_settings;
static int save_return_to_equaliser;
static int naming_equaliser;
static int equaliser_selected;
static lv_timer_t *equaliser_hold_timer;
static int equaliser_hold_direction;
static uint32_t equaliser_hold_seen;
static uint32_t equaliser_hold_started;
static uint32_t equaliser_hold_stepped;
static int equaliser_hold_count;

static void equaliser_hold_stop(void);
static lv_obj_t *equaliser_bar_label;
static lv_obj_t *equaliser_info;
static lv_obj_t *equaliser_fill[WASABI_EQ_GAINS];
static lv_obj_t *equaliser_knob[WASABI_EQ_GAINS];
static lv_obj_t *equaliser_name[WASABI_EQ_GAINS];
static int equaliser_track_top;
static int equaliser_track_height;

enum { asset_action_collect, asset_action_delete, asset_action_cancel };

enum { menu_peek_delay_ms = 250 };

static int asset_page(void) {
    return settings_page == wasabi_page_colour_filter || settings_page == wasabi_page_shader
           || settings_page == wasabi_page_overlay_image;
}

static wasabi_asset_kind current_asset_kind(void) {
    if (settings_page == wasabi_page_colour_filter) return wasabi_asset_filter;
    if (settings_page == wasabi_page_shader) return wasabi_asset_shader;
    return wasabi_asset_overlay;
}

static void set_glyph(lv_obj_t *image, const char *name);
static void populate_asset_page(int focus);

static void align_status_panel(void) {
    if (!status_panel) return;
    int offset = 4;
    if (mode_panel && !lv_obj_has_flag(mode_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_update_layout(mode_panel);
        offset += lv_obj_get_width(mode_panel) + 4;
    }
    lv_obj_align(status_panel, LV_ALIGN_BOTTOM_LEFT, offset, -4);
}

static void apply_session_preview(void) {
    video_player_image_settings_changed();
    video_player_effect_settings_changed();
    video_player_audio_settings_changed();
    video_player_audio_ui_changed();
    video_playback_ui_header_changed();
    video_player_modes_changed();
}

static int guard_settings_exit(void) {
    if (!wasabi_session_dirty()) return 0;
    save_return_to_settings = 0;
    save_return_to_equaliser = 0;
    dialogue_open(&settings_save_dialogue, &theme);
    display_composite_frame();
    return 1;
}

static void restore_asset_entry(void) {
    const wasabi_asset_kind kind = current_asset_kind();
    wasabi_asset_preview(kind, wasabi_asset_find(kind, asset_entry_key));
    if (kind == wasabi_asset_overlay) config.video.overlay_mode = asset_entry_overlay_mode;
    if (kind == wasabi_asset_filter || kind == wasabi_asset_shader)
        video_player_effect_settings_changed();
    else
        video_render_settings_changed();
}

static void apply_asset_preview(void) {
    if (!settings_active || !asset_page() || wasabi_catalogue_active()) return;
    const wasabi_asset_row_type type = wasabi_asset_browser_type(current_item_index);
    const wasabi_asset_kind kind = current_asset_kind();
    const int item = type == wasabi_asset_row_none   ? 0
                     : type == wasabi_asset_row_item ? wasabi_asset_browser_item(current_item_index)
                                                     : wasabi_asset_find(kind, asset_entry_key);
    if (item < 0 || !wasabi_asset_preview(kind, item)) return;
    if (kind == wasabi_asset_filter || kind == wasabi_asset_shader)
        video_player_effect_settings_changed();
    else {
        if (kind == wasabi_asset_overlay)
            config.video.overlay_mode = type == wasabi_asset_row_none   ? 0
                                        : type == wasabi_asset_row_item ? 3
                                                                        : asset_entry_overlay_mode;
        video_render_settings_changed();
    }
}

static void nav_show_x(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_x, text);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_y(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_y, text);
        lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void hide_bookmark_preview(void) {
    clear_image(ui_img_box);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
}

static void create_name_entry(void) {
    if (name_panel && lv_obj_is_valid(name_panel)) return;

    name_panel = lv_obj_create(ui_screen);
    lv_obj_set_size(name_panel, device.mux.width, device.mux.height);
    lv_obj_center(name_panel);
    lv_obj_set_flex_flow(name_panel, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(name_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(name_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(name_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(name_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(name_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(name_panel, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(name_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(name_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(name_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(name_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(name_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(name_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(name_panel, 5, MU_OBJ_MAIN_DEFAULT);

    name_entry = lv_textarea_create(name_panel);
    lv_obj_set_width(name_entry, device.mux.width * 5 / 6);
    lv_obj_set_height(name_entry, LV_SIZE_CONTENT);
    lv_obj_center(name_entry);
    lv_textarea_set_one_line(name_entry, 1);
    lv_textarea_set_placeholder_text(name_entry, lang.muxmedia.bookmark_name);
    lv_obj_set_style_radius(name_entry, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(name_entry, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(name_entry, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(name_entry, 2, MU_OBJ_MAIN_DEFAULT);
}

static void close_name_entry(const int cancel) {
    if (!naming_active) return;
    naming_active = 0;
    if (cancel)
        close_osk(key_entry, ui_group, name_entry, name_panel);
    else {
        key_show = 0;
        key_map = 0;
        reset_osk(key_entry);
        lv_textarea_set_text(name_entry, "");
        lv_group_set_focus_cb(ui_group, NULL);
        osk_hide(name_panel);
    }
    display_composite_frame();
}

static void update_bookmark_preview(void) {
    if (!bookmarks_active || current_item_index < 0 || (size_t) current_item_index >= bookmark_count) {
        hide_bookmark_preview();
        return;
    }

    const video_state_entry *entry = &bookmark_entries[bookmark_indices[current_item_index]];
    if (!entry->thumbnail || !entry->thumbnail[0] || !file_exist(entry->thumbnail)) {
        hide_bookmark_preview();
        return;
    }

    lv_img_cache_invalidate_src(NULL);
    lv_obj_clear_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    static const int divisors[] = {3, 2, 3};
    const int divisor = divisors[config.video.thumbnail_size];
    const int multiplier = config.video.thumbnail_size == 2 ? 2 : 1;
    const struct image_settings settings = {
        .image_path = entry->thumbnail,
        .align = LV_ALIGN_BOTTOM_RIGHT,
        .max_width = (int16_t) (device.mux.width * multiplier / divisor),
        .max_height =
            (int16_t) ((device.mux.height - theme.header.height - theme.footer.height - 8) * multiplier / divisor),
    };
    update_image(ui_img_box, settings);
}

static void show_paused_status(void) {
    set_glyph(status_glyph, "pause");
    lv_obj_set_width(status_label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(status_label, lang.muxretro.hotkeys_screen.paused);
    align_status_panel();
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
    status_deadline = SDL_GetTicks() + 2000;
}

static void set_glyph(lv_obj_t *image, const char *name) {
    char path[MAX_BUFFER_SIZE];
    if (!get_glyph_path(mux_module, name, path, sizeof(path))) {
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    set_footer_glyph_image(image, path);
    lv_obj_set_style_img_opa(image, (lv_opa_t) theme.list_default.glyph_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(image, lv_color_hex(theme.list_default.glyph_recolour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(image, (lv_opa_t) theme.list_default.glyph_recolour_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *create_indicator(const lv_align_t align, lv_obj_t **glyph) {
    lv_obj_t *panel = lv_obj_create(ui_screen);
    lv_obj_set_size(panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 140, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(panel, align, align == LV_ALIGN_BOTTOM_LEFT ? 4 : -4, -4);
    load_font_section(FONT_FOOTER_DIR, panel);

    *glyph = lv_img_create(panel);
    lv_obj_move_foreground(panel);
    return panel;
}

static void format_time(const uint32_t seconds, char *buffer, const size_t size) {
    const uint32_t days = seconds / 86400;
    const uint32_t hours = seconds / 3600 % 24;
    const uint32_t minutes = seconds / 60 % 60;
    if (days)
        snprintf(buffer, size, "%ud %02u:%02u:%02u", days, hours, minutes, seconds % 60);
    else if (seconds >= 3600)
        snprintf(buffer, size, "%02u:%02u:%02u", hours, minutes, seconds % 60);
    else
        snprintf(buffer, size, "%02u:%02u", minutes, seconds % 60);
}

static void format_playback_time(const uint32_t seconds, char *buffer, const size_t size) {
    if (config.video.time_display && !wasabi_settings_audio_active())
        snprintf(buffer, size, "%04u", seconds % 10000);
    else
        format_time(seconds, buffer, size);
}

static void update_timeline(void) {
    if (!timeline_panel || playback_duration <= 0.0) return;
    char current[24];
    char total[24];
    format_playback_time(playback_position > 0.0 ? (uint32_t) playback_position : 0, current, sizeof(current));
    format_playback_time((uint32_t) playback_duration, total, sizeof(total));
    lv_label_set_text(timeline_current, current);
    lv_label_set_text(timeline_total, total);
    const int value = (int) lround(playback_position * 1000.0 / playback_duration);
    wasabi_progress_update(&timeline_progress, value);
}

static void rebuild_timeline_progress(void) {
    if (!timeline_panel || timeline_width < 1) return;
    if (timeline_track && lv_obj_is_valid(timeline_track)) lv_obj_del(timeline_track);
    wasabi_progress_reset(&timeline_progress);
    timeline_track = lv_obj_create(timeline_panel);
    lv_obj_remove_style_all(timeline_track);
    lv_obj_set_pos(timeline_track, timeline_padding_x, timeline_padding_y);
    lv_obj_set_size(timeline_track, timeline_width, 28);
    lv_obj_clear_flag(timeline_track, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    wasabi_progress_init(&timeline_progress, timeline_track, timeline_width);
    lv_obj_move_background(timeline_track);
    update_timeline();
}

static void show_timeline(void) {
    if (menu_active || wasabi_settings_audio_active() || playback_duration <= 0.0 || !timeline_panel) return;
    update_timeline();
    lv_obj_clear_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN);
    timeline_deadline = SDL_GetTicks() + 2000;
}

static int header_shows_clock(void) {
    return config.video.header_visibility == 1 || config.video.header_visibility == 3
           || config.video.header_visibility == 5;
}

static int header_shows_battery(void) {
    return config.video.header_visibility == 2 || config.video.header_visibility == 3
           || config.video.header_visibility == 5;
}

static int header_shows_duration(void) {
    return config.video.header_visibility == 4 || config.video.header_visibility == 5;
}

static void update_header_playback_time(void) {
    if (!header_shows_duration()) return;
    char current[24];
    char total[24];
    char value[56];
    format_playback_time(playback_position > 0.0 ? (uint32_t) playback_position : 0, current, sizeof(current));
    format_playback_time(playback_duration > 0.0 ? (uint32_t) playback_duration : 0, total, sizeof(total));
    snprintf(value, sizeof(value), "%s / %s", current, total);
    lv_label_set_text(ui_lbl_title, value);
}

static void restore_header(void) {
    lv_obj_set_style_opa(ui_pnl_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_header, theme.header.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_sta_bluetooth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_sta_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
    process_visual_element(vis_headertitle, ui_lbl_title);
    process_visual_element(vis_bluetooth, ui_sta_bluetooth);
    process_visual_element(vis_network, ui_sta_network);
    process_visual_element(vis_clock, ui_lbl_datetime);
    process_visual_element(vis_battery, ui_sta_capacity);
    if (config.visual.battery == 1 || config.visual.battery == 2)
        lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    update_battery_capacity(ui_sta_capacity, &theme);
    update_battery_percent_label(ui_lbl_battery_percent, &theme);
}

static void apply_gameplay_header(void) {
    if (!config.video.header_visibility) {
        lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(ui_pnl_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_header, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_sta_bluetooth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_sta_network, LV_OBJ_FLAG_HIDDEN);

    if (header_shows_clock())
        lv_obj_clear_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);

    if (header_shows_duration())
        lv_obj_clear_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);

    if (header_shows_battery()) {
        if (config.visual.battery != 1)
            lv_obj_clear_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
        if (config.visual.battery != 0)
            lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
    }

    update_header_playback_time();
    if (header_shows_clock()) datetime_task(NULL);
    if (header_shows_battery()) battery_capacity_task(NULL);
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
}

static int transient_visible(void) {
    if (mode_panel && !lv_obj_has_flag(mode_panel, LV_OBJ_FLAG_HIDDEN)) return 1;
    if (status_panel && !lv_obj_has_flag(status_panel, LV_OBJ_FLAG_HIDDEN)) return 1;
    if (timeline_panel && !lv_obj_has_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN)) return 1;
    if (ui_pnl_progress_volume && !lv_obj_has_flag(ui_pnl_progress_volume, LV_OBJ_FLAG_HIDDEN)) return 1;
    if (ui_pnl_progress_brightness && !lv_obj_has_flag(ui_pnl_progress_brightness, LV_OBJ_FLAG_HIDDEN)) return 1;
    if (ui_pnl_message && !lv_obj_has_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN)) return 1;
    return 0;
}

static void apply_visibility(void) {
    const int playtime =
        config.video.show_playtime && playtime_panel && !lv_obj_has_flag(playtime_panel, LV_OBJ_FLAG_HIDDEN);
    display_set_ui_hidden(
        menu_peeking
        || (!wasabi_settings_audio_active()
            && !(menu_active || playtime || config.video.header_visibility || transient_visible()))
    );
}

static void apply_playback_chrome(void) {
    hide_bookmark_preview();
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);
    if (config.video.show_playtime)
        lv_obj_clear_flag(playtime_panel, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(playtime_panel, LV_OBJ_FLAG_HIDDEN);
    apply_gameplay_header();
    video_playback_ui_modes_changed();
    apply_visibility();
}

static void apply_menu_chrome(void) {
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(playtime_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mode_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN);
    restore_header();
    display_set_ui_hidden(0);
}

static void set_menu_peeking(const int peeking) {
    if (menu_peeking == peeking) return;
    menu_peeking = peeking;
    display_set_ui_hidden(peeking);
    lv_obj_invalidate(ui_screen);
    display_composite_frame();
}

static void add_row(const char *glyph, const char *label) {
    gen_label(mux_module, glyph, label);
    ui_count_static++;
}

static void focus_pause_row(const int row) {
    if (row < 0 || row >= ui_count_static) return;
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, row);
    if (!panel) return;

    current_item_index = row;
    nav_suppress_next_shake();
    lv_obj_t *label = lv_obj_get_child(panel, 0);
    lv_obj_t *glyph = lv_obj_get_child(panel, 1);
    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    lv_group_focus_obj(panel);
    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
}

static void add_setting_row(const char *glyph_name, const char *label_text, const char *value_text) {
    const int has_value = value_text && value_text[0];
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(panel);
    lv_obj_t *label = lv_label_create(panel);
    apply_theme_option_item_label(&theme, label, label_text, has_value);
    lv_obj_t *glyph = lv_img_create(panel);
    apply_theme_list_glyph(&theme, glyph, mux_module, glyph_name);
    lv_obj_t *value = lv_label_create(panel);
    apply_theme_list_value(&theme, value, value_text);
    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_value, value);
    lv_group_add_obj(ui_group_glyph, glyph);
    lv_group_add_obj(ui_group_panel, panel);
    if (has_value) adjust_label_value_width(panel, label, value);
    apply_option_label_long_dot(label);
    apply_option_value_long_dot(value);
    ui_count_static++;
}

static void apply_overlay_row_visibility(void) {
    if (!list_frame_active()) return;
    const int enabled = config.video.overlay_mode != 0;
    list_frame_set_suppressed(wasabi_setting_overlay_pattern, config.video.overlay_mode != 1);
    list_frame_set_suppressed(wasabi_setting_overlay_image, config.video.overlay_mode != 3);
    list_frame_set_suppressed(wasabi_setting_overlay_opacity, !enabled);
    list_frame_set_suppressed(wasabi_setting_overlay_adjustment, !enabled);
    list_frame_set_suppressed(wasabi_setting_overlay_cropping, !enabled);
    list_frame_set_suppressed(wasabi_setting_overlay_reset, !enabled);
    list_frame_set_suppressed(wasabi_setting_live_quality, !live_content || !video_player_live_quality_available());
    list_frame_set_suppressed(wasabi_setting_live_buffer, !live_content);
    list_frame_set_suppressed(wasabi_setting_crt_television, !live_content);
    list_frame_set_suppressed(
        wasabi_setting_seek_effect, wasabi_settings_audio_active() || !video_player_seek_available()
    );
    list_frame_set_suppressed(wasabi_setting_time_display, wasabi_settings_audio_active() || live_content);
    list_frame_set_suppressed(wasabi_setting_visualiser, !wasabi_settings_audio_active());
    list_frame_set_suppressed(wasabi_setting_progress_bar, 0);
    list_frame_set_suppressed(wasabi_setting_artwork_position, !wasabi_settings_audio_active());
    list_frame_set_suppressed(wasabi_setting_thumbnail, wasabi_settings_audio_active());
    for (int row = wasabi_setting_gapless; row <= wasabi_setting_tracker_loop; row++)
        list_frame_set_suppressed(row, !wasabi_settings_audio_active());
    for (int row = wasabi_setting_fast_forward_mode; row <= wasabi_setting_slow_motion_speed; row++)
        list_frame_set_suppressed(row, !video_player_speed_available());
    list_frame_set_suppressed(wasabi_setting_hotkey_fast_forward, !video_player_speed_available());
    list_frame_set_suppressed(wasabi_setting_hotkey_slow_motion, !video_player_speed_available());
    for (int row = wasabi_setting_hotkey_seek_back; row <= wasabi_setting_hotkey_seek_forward_long; row++)
        list_frame_set_suppressed(row, !video_player_seek_available());
    list_frame_set_suppressed(
        wasabi_setting_tracker_loop, !wasabi_settings_audio_active() || !video_player_tracker_loop_available()
    );
}

static void show_equaliser_nav(void) {
    nav_hide_all();
    setup_nav((struct nav_bar[]) {
        {ui_lbl_nav_b_glyph, "", 0},
        {ui_lbl_nav_b, lang.generic.back, 0},
        {NULL, NULL, 0},
    });
    nav_show_a(!live_content, video_player_paused() ? lang.muxmedia.play : lang.muxmedia.pause);
    nav_show_x(wasabi_eq_profile_deletable(), lang.generic.remove);
    nav_show_y(1, lang.generic.save);
}

static void show_nav(void) {
    if (settings_active && settings_page == wasabi_page_equaliser) {
        show_equaliser_nav();
        return;
    }
    nav_hide_all();
    setup_nav((struct nav_bar[]) {
        {ui_lbl_nav_b_glyph, "", 0},
        {ui_lbl_nav_b,
         settings_active || bookmarks_active || playlist_active || information_active || content_switch_active
             ? lang.generic.back
             : lang.muxmedia.continue_playback,
         0},
        {NULL, NULL, 0},
    });
    int show_a = !information_active && (!bookmarks_active || bookmark_count > 0)
                 && (!content_switch_active || content_switch_items.count > 0);
    if (settings_active) {
        const int row = settings_page == wasabi_page_root ? list_frame_current_row() : current_item_index;
        show_a = (wasabi_catalogue_active() && wasabi_catalogue_count() > 0)
                 || (asset_page() && wasabi_asset_browser_type(current_item_index) != wasabi_asset_row_empty)
                 || (settings_page == wasabi_page_root && row >= 0 && wasabi_setting_is_action((wasabi_setting) row));
    }
    nav_show_a(
        show_a, bookmarks_active            ? lang.generic.load
                : wasabi_catalogue_active() ? wasabi_catalogue_action(current_item_index)
                                            : lang.generic.select
    );
    if (settings_active) {
        const int row = settings_page == wasabi_page_root ? list_frame_current_row() : current_item_index;
        const int changeable =
            (asset_page() || wasabi_catalogue_active()) ? 0
            : settings_page == wasabi_page_root
                ? list_frame_focused() || (row >= 0 && wasabi_setting_can_change((wasabi_setting) row))
            : settings_page == wasabi_page_shader_parameters ? row >= 0 && row < video_effects_parameter_count()
                                                             : row >= 0;
        if (changeable) lv_label_set_text(ui_lbl_nav_lr, lang.generic.change);
        nav_show_lr(changeable);
    }
    const int root_row = settings_active && settings_page == wasabi_page_root ? list_frame_current_row() : -1;
    const int hotkey_reset = root_row >= wasabi_setting_hotkey_pause && root_row <= wasabi_setting_hotkey_slow_motion;
    const int shader_browser_adjust = settings_active && settings_page == wasabi_page_shader
                                      && !wasabi_catalogue_active()
                                      && wasabi_asset_browser_type(current_item_index) == wasabi_asset_row_item;
    const int shader_adjust = settings_active
                              && ((settings_page == wasabi_page_root && root_row == wasabi_setting_shader
                                   && video_effects_parameter_count() > 0)
                                  || shader_browser_adjust);
    const int bookmark_remove = bookmarks_active && bookmark_count > 0;
    const int asset_delete = settings_active && asset_page() && settings_page != wasabi_page_shader
                             && !wasabi_catalogue_active() && wasabi_asset_browser_removable(current_item_index);
    nav_show_x(
        hotkey_reset || shader_adjust || bookmark_remove || asset_delete,
        hotkey_reset    ? lang.generic.reset
        : shader_adjust ? lang.muxretro.shader_screen.adjust
        : asset_delete  ? lang.muxretro.catalogue_screen.delete_label
                        : lang.generic.remove
    );
    const int asset_collect = settings_active && asset_page() && !wasabi_catalogue_active()
                              && wasabi_asset_browser_type(current_item_index) == wasabi_asset_row_item;
    const int collected = asset_collect && wasabi_asset_browser_collected(current_item_index);
    const int removable_shader =
        asset_collect && settings_page == wasabi_page_shader && wasabi_asset_browser_removable(current_item_index);
    const char *asset_action =
        collected          ? (removable_shader ? lang.muxretro.catalogue_screen.delete_label : lang.generic.remove)
        : removable_shader ? lang.generic.actions
                           : lang.generic.collect;
    nav_show_y(bookmarks_active || asset_collect, bookmarks_active ? lang.generic.save : asset_action);
}

static void build_pause_at(const int focus_row) {
    shader_parameters_from_browser = 0;
    equaliser_hold_stop();
    wasabi_audio_ui_set_hidden(0);
    equaliser_bar_label = NULL;
    equaliser_info = NULL;
    settings_active = 0;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 0;
    content_switch_active = 0;
    content_switch_free(&content_switch_items);
    content_switch_load(&content_switch_items, playback_content_uri);
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    add_row("resume", lang.muxmedia.continue_playback);
    bookmark_row = -1;
    if (!live_content) {
        bookmark_row = ui_count_static;
        add_row("state", lang.muxmedia.bookmarks);
    }
    playlist_row = -1;
    if (playback_playlist_count > 1) {
        playlist_row = ui_count_static;
        add_row(
            playback_playlist_channels ? "network"
            : playback_playlist_audio  ? "audio"
                                       : "video",
            playback_playlist_channels ? lang.muxmedia.channels
            : playback_playlist_audio  ? lang.muxmedia.tracks
                                       : lang.muxmedia.episodes
        );
    }
    content_switch_row = -1;
    if (content_switch_items.count > 1) {
        content_switch_row = ui_count_static;
        add_row("switch", lang.content_switch.title);
    }
    settings_row = ui_count_static;
    add_row("settings", lang.muxretro.settings);
    equaliser_row = ui_count_static;
    add_row("audiofilter", lang.muxmedia.equaliser);
    information_row = ui_count_static;
    add_row("info", lang.muxretro.information);
    restart_row = ui_count_static;
    add_row("restart", lang.muxretro.restart);
    stop_row = ui_count_static;
    add_row("exit", lang.muxretro.quit);
    lv_label_set_text(ui_lbl_title, playback_title);
    lv_label_set_text(ui_lbl_screen_message, "");
    show_nav();
    focus_pause_row(focus_row >= 0 && focus_row < ui_count_static ? focus_row : 0);
}

static void build_content_switch(void) {
    settings_active = 0;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 0;
    content_switch_active = 1;
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    for (size_t index = 0; index < content_switch_items.count; index++) {
        const content_switch_entry *entry = &content_switch_items.entries[index];
        add_row(entry->native == content_switch_native_wasabi ? "wasabi" : "pickles", entry->title);
    }
    lv_label_set_text(ui_lbl_title, lang.content_switch.title);
    lv_label_set_text(ui_lbl_screen_message, content_switch_items.count ? "" : lang.content_switch.empty);
    show_nav();
    if (content_switch_items.count) gen_step_movement((int) content_switch_items.selected, 1, 1, 0, 0);
}

static void build_pause(void) {
    build_pause_at(0);
    if (remembered_row && *remembered_row > 0) focus_pause_row(*remembered_row);
}

static void focus_root_setting(const int row) {
    if (row < 0 || row >= wasabi_setting_count) return;
    size_t section_count = 0;
    const wasabi_setting_section *sections = wasabi_setting_sections(&section_count);
    for (size_t section = 0; section < section_count; section++) {
        if (row < sections[section].first || row >= sections[section].first + sections[section].count) continue;
        list_frame_go((int) section);
        const int steps = list_frame_steps_to_row(row);
        if (steps > 0) gen_step_movement(steps, 1, 1, 0, 0);
        break;
    }
}

static void build_settings_at(const int focus_row) {
    shader_parameters_from_browser = 0;
    settings_active = 1;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 0;
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;
    settings_page = wasabi_page_root;

    memset(setting_panels, 0, sizeof(setting_panels));
    memset(setting_labels, 0, sizeof(setting_labels));
    memset(setting_glyphs, 0, sizeof(setting_glyphs));
    memset(setting_values, 0, sizeof(setting_values));

    for (int row = 0; row < wasabi_setting_count; row++) {
        char value[128];
        wasabi_setting_value((wasabi_setting) row, value, sizeof(value));
        add_setting_row(wasabi_setting_glyph((wasabi_setting) row), wasabi_setting_label((wasabi_setting) row), value);
        setting_panels[row] = lv_obj_get_child(ui_pnl_content, row);
        setting_labels[row] = lv_obj_get_child(setting_panels[row], 0);
        setting_glyphs[row] = lv_obj_get_child(setting_panels[row], 1);
        setting_values[row] = lv_obj_get_child(setting_panels[row], 2);
    }

    size_t section_count = 0;
    const wasabi_setting_section *sections = wasabi_setting_sections(&section_count);
    for (size_t i = 0; i < section_count; i++)
        setting_frames[i] = (list_frame) {sections[i].label, sections[i].first, sections[i].count};
    if (list_frame_init(
            &theme, ui_pnl_content, setting_frames, (int) section_count, setting_panels, setting_labels, setting_glyphs,
            setting_values, wasabi_setting_count
        )) {
        apply_overlay_row_visibility();
        if (focus_row >= 0) {
            list_frame_apply();
            focus_root_setting(focus_row);
        } else {
            const int steps = list_frame_restore_key("muxmedia_settings");
            if (steps > 0) gen_step_movement(steps, 1, 2, 0, 0);
        }
    }

    lv_label_set_text(ui_lbl_title, lang.muxretro.settings);
    show_nav();
}

static void build_settings(void) {
    build_settings_at(-1);
}

static void build_settings_page(const wasabi_settings_page page) {
    settings_active = 1;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 0;
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    settings_page = page;
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    memset(setting_panels, 0, sizeof(setting_panels));
    memset(setting_labels, 0, sizeof(setting_labels));
    memset(setting_glyphs, 0, sizeof(setting_glyphs));
    memset(setting_values, 0, sizeof(setting_values));

    const int rows = wasabi_page_row_count(page);
    for (int row = 0; row < rows; row++) {
        char value[128];
        wasabi_page_value(page, row, value, sizeof(value));
        add_setting_row(wasabi_page_glyph(page, row), wasabi_page_label(page, row), value);
        setting_panels[row] = lv_obj_get_child(ui_pnl_content, row);
        setting_labels[row] = lv_obj_get_child(setting_panels[row], 0);
        setting_glyphs[row] = lv_obj_get_child(setting_panels[row], 1);
        setting_values[row] = lv_obj_get_child(setting_panels[row], 2);
    }
    lv_label_set_text(ui_lbl_title, wasabi_page_title(page));
    show_nav();
    if (rows) gen_step_movement(0, 1, 1, 0, 0);
}

static lv_obj_t *equaliser_rect(
    lv_obj_t *parent, const int x, const int y, const int width, const int height, const uint32_t colour,
    const lv_opa_t opacity
) {
    lv_obj_t *rect = lv_obj_create(parent);
    lv_obj_remove_style_all(rect);
    lv_obj_clear_flag(rect, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_size(rect, width > 0 ? width : 1, height > 0 ? height : 1);
    lv_obj_set_style_bg_color(rect, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(rect, opacity, MU_OBJ_MAIN_DEFAULT);
    return rect;
}

static lv_obj_t *equaliser_text(lv_obj_t *parent, const char *text, const uint32_t colour) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    return label;
}

static int equaliser_gain_y(const int gain) {
    const int limit = WASABI_EQ_LIMIT * WASABI_EQ_STEPS;
    return equaliser_track_top + (limit - gain) * equaliser_track_height / (limit * 2);
}

static void update_equaliser_view(void) {
    if (!equaliser_bar_label || !lv_obj_is_valid(equaliser_bar_label)) return;
    char text[128];
    wasabi_eq_profile_label(text, sizeof(text));
    lv_label_set_text(equaliser_bar_label, text);

    const int zero = equaliser_gain_y(0);
    const int knob_height = equaliser_track_height / 24 > 6 ? equaliser_track_height / 24 : 6;
    for (int index = 0; index < WASABI_EQ_GAINS; index++) {
        if (!equaliser_fill[index]) continue;
        const int selected = index == equaliser_selected;
        const uint32_t colour = selected ? theme.list_focus.background : theme.list_default.text;
        const int y = equaliser_gain_y(wasabi_eq_gain(index));
        const int top = y < zero ? y : zero;
        const int height = y < zero ? zero - y : y - zero;
        lv_obj_set_y(equaliser_fill[index], top);
        lv_obj_set_height(equaliser_fill[index], height > 0 ? height : 1);
        lv_obj_set_style_bg_color(equaliser_fill[index], lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(equaliser_fill[index], selected ? LV_OPA_COVER : LV_OPA_60, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_y(equaliser_knob[index], y - knob_height / 2);
        lv_obj_set_style_bg_color(equaliser_knob[index], lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
        const lv_color_t label_colour = lv_color_hex(selected ? theme.list_focus.background : theme.list_default.text);
        if (index) {
            lv_obj_set_style_text_color(equaliser_name[index], label_colour, MU_OBJ_MAIN_DEFAULT);
        } else {
            lv_obj_set_style_img_recolor(equaliser_name[index], label_colour, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_img_recolor_opa(equaliser_name[index], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        }
    }

    char gain[16];
    wasabi_eq_format(wasabi_eq_gain(equaliser_selected), gain, sizeof(gain));
    snprintf(
        text, sizeof(text), "%s  %s dB",
        equaliser_selected ? wasabi_eq_band_label(equaliser_selected - 1) : lang.muxmedia.equaliser_preamp, gain
    );
    lv_label_set_text(equaliser_info, text);
}

static void build_equaliser(void) {
    wasabi_eq_profiles_refresh();
    wasabi_audio_ui_set_hidden(1);
    settings_active = 1;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 0;
    content_switch_active = 0;
    settings_page = wasabi_page_equaliser;
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;
    if (equaliser_selected < 0 || equaliser_selected >= WASABI_EQ_GAINS) equaliser_selected = 0;

    lv_obj_update_layout(ui_pnl_content);
    const int width = lv_obj_get_content_width(ui_pnl_content);
    const int height = lv_obj_get_content_height(ui_pnl_content);
    const int bar_height = theme.mux.item.height > 0 ? theme.mux.item.height : height / 8;

    lv_obj_t *root = lv_obj_create(ui_pnl_content);
    lv_obj_remove_style_all(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, width, height);

    lv_obj_t *bar = lv_obj_create(root);
    apply_theme_list_panel(bar);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bar, width, bar_height);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_t *left = lv_img_create(bar);
    apply_theme_list_glyph(&theme, left, "section", "left");
    equaliser_bar_label = lv_label_create(bar);
    apply_theme_list_item(&theme, equaliser_bar_label, "");
    lv_obj_set_width(equaliser_bar_label, lv_pct(100));
    lv_obj_set_align(equaliser_bar_label, LV_ALIGN_CENTER);
    lv_label_set_long_mode(equaliser_bar_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(equaliser_bar_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    const lv_coord_t inset = theme.list_default.glyph_padding_left + bar_height;
    lv_obj_set_style_pad_left(equaliser_bar_label, inset, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(equaliser_bar_label, inset, MU_OBJ_MAIN_DEFAULT);
    lv_obj_t *right = lv_img_create(bar);
    apply_theme_list_glyph(&theme, right, "section", "right");
    lv_obj_set_style_align(right, LV_ALIGN_RIGHT_MID, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_x(right, -theme.list_default.glyph_padding_left, MU_OBJ_MAIN_DEFAULT);

    const int line = lv_font_get_line_height(lv_obj_get_style_text_font(equaliser_bar_label, LV_PART_MAIN));
    const int scale_width = line * 2;
    const int top = bar_height + line;
    const int bottom = height - line * 3;
    equaliser_track_top = top;
    equaliser_track_height = bottom - top > 24 ? bottom - top : 24;

    const uint32_t ink = theme.list_default.text;
    static const int marks[] = {12, 6, 0, -6, -12};
    for (size_t mark = 0; mark < sizeof(marks) / sizeof(marks[0]); mark++) {
        char value[8];
        snprintf(value, sizeof(value), marks[mark] > 0 ? "+%d" : "%d", marks[mark]);
        lv_obj_t *label = equaliser_text(root, value, ink);
        lv_obj_set_width(label, scale_width);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, MU_OBJ_MAIN_DEFAULT);
        const int mark_y = equaliser_gain_y(marks[mark] * WASABI_EQ_STEPS);
        lv_obj_set_pos(label, 0, mark_y - line / 2);
        equaliser_rect(
            root, scale_width + line / 2, mark_y, width - scale_width - line / 2, 1, ink,
            marks[mark] ? LV_OPA_20 : LV_OPA_40
        );
    }

    const int area = width - scale_width - line;
    const int column = area / WASABI_EQ_GAINS;
    const int knob_width = column * 3 / 5;
    const int knob_height = equaliser_track_height / 24 > 6 ? equaliser_track_height / 24 : 6;
    const int fill_width = column / 6 > 3 ? column / 6 : 3;
    for (int index = 0; index < WASABI_EQ_GAINS; index++) {
        const int centre = scale_width + line + column * index + column / 2;
        equaliser_rect(root, centre - 1, equaliser_track_top, 2, equaliser_track_height, ink, LV_OPA_40);
        equaliser_fill[index] = equaliser_rect(root, centre - fill_width / 2, 0, fill_width, 1, ink, LV_OPA_60);
        equaliser_knob[index] =
            equaliser_rect(root, centre - knob_width / 2, 0, knob_width, knob_height, ink, LV_OPA_COVER);
        lv_obj_set_style_radius(equaliser_knob[index], knob_height / 2, MU_OBJ_MAIN_DEFAULT);
        const int label_y = equaliser_track_top + equaliser_track_height + line / 2;
        if (!index) {
            equaliser_name[index] = lv_img_create(root);
            apply_theme_list_glyph(&theme, equaliser_name[index], mux_module, "volume");
            lv_obj_set_style_align(equaliser_name[index], LV_ALIGN_TOP_LEFT, MU_OBJ_MAIN_DEFAULT);
            lv_obj_update_layout(equaliser_name[index]);
            const int glyph_width = lv_obj_get_width(equaliser_name[index]);
            const int glyph_height = lv_obj_get_height(equaliser_name[index]);
            lv_obj_set_pos(equaliser_name[index], centre - glyph_width / 2, label_y + (line - glyph_height) / 2);
            continue;
        }
        char short_name[16];
        snprintf(short_name, sizeof(short_name), "%s", wasabi_eq_band_label(index - 1));
        char *space = strchr(short_name, ' ');
        if (space) *space = '\0';
        if (space && space[1] == 'k') strcat(short_name, "K");
        equaliser_name[index] = equaliser_text(root, short_name, ink);
        lv_obj_set_size(equaliser_name[index], column, line);
        lv_label_set_long_mode(equaliser_name[index], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(equaliser_name[index], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_pos(equaliser_name[index], centre - column / 2, label_y);
    }

    equaliser_info = equaliser_text(root, "", theme.list_focus.background);
    lv_obj_set_width(equaliser_info, width);
    lv_obj_set_style_text_align(equaliser_info, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_pos(equaliser_info, 0, height - line - line / 4);

    lv_label_set_text(ui_lbl_title, lang.muxmedia.equaliser);
    lv_label_set_text(ui_lbl_screen_message, "");
    update_equaliser_view();
    show_nav();
}

static void clear_equaliser_view(void) {
    equaliser_bar_label = NULL;
    equaliser_info = NULL;
    memset(equaliser_fill, 0, sizeof(equaliser_fill));
    memset(equaliser_knob, 0, sizeof(equaliser_knob));
    memset(equaliser_name, 0, sizeof(equaliser_name));
}

static void leave_equaliser(void) {
    if (wasabi_session_dirty()) {
        save_return_to_settings = 0;
        save_return_to_equaliser = 1;
        dialogue_open(&settings_save_dialogue, &theme);
        return;
    }
    clear_equaliser_view();
    build_pause_at(equaliser_row);
}

static int equaliser_active(void) {
    return menu_active && settings_active && settings_page == wasabi_page_equaliser;
}

static int equaliser_step(const int direction) {
    if (!wasabi_eq_set_gain(equaliser_selected, wasabi_eq_gain(equaliser_selected) + direction)) return 0;
    update_equaliser_view();
    display_composite_frame();
    return 1;
}

static void equaliser_adjust(const int direction) {
    if (!equaliser_step(direction)) return;
    play_sound(snd_option);
    show_nav();
}

static void equaliser_hold_stop(void) {
    if (equaliser_hold_timer) lv_timer_del(equaliser_hold_timer);
    equaliser_hold_timer = NULL;
    equaliser_hold_count = 0;
}

static void equaliser_hold_tick(lv_timer_t *timer __attribute__((unused))) {
    const uint32_t now = SDL_GetTicks();
    if (!equaliser_active() || now - equaliser_hold_seen > 180) {
        equaliser_hold_stop();
        show_nav();
        return;
    }
    const uint32_t interval = now - equaliser_hold_started < 400 ? 60 : now - equaliser_hold_started < 1000 ? 35 : 20;
    if (now - equaliser_hold_stepped < interval) return;
    equaliser_hold_stepped = now;
    if (!equaliser_step(equaliser_hold_direction)) {
        equaliser_hold_stop();
        show_nav();
        return;
    }
    if (++equaliser_hold_count % WASABI_EQ_STEPS == 0) play_sound(snd_option);
}

static void equaliser_hold(const int direction) {
    const uint32_t now = SDL_GetTicks();
    equaliser_hold_seen = now;
    if (equaliser_hold_timer && equaliser_hold_direction == direction) return;
    equaliser_hold_stop();
    equaliser_hold_direction = direction;
    equaliser_hold_started = now;
    equaliser_hold_stepped = 0;
    equaliser_hold_timer = lv_timer_create(equaliser_hold_tick, 10, NULL);
}

static void equaliser_select(const int direction) {
    const int next = equaliser_selected + direction;
    if (next < 0 || next >= WASABI_EQ_GAINS) return;
    equaliser_selected = next;
    play_sound(snd_navigate);
    update_equaliser_view();
    display_composite_frame();
}

static void equaliser_profile(const int direction) {
    if (!wasabi_eq_profile_cycle(direction)) {
        play_sound(snd_error);
        return;
    }
    play_sound(snd_navigate);
    update_equaliser_view();
    show_nav();
    display_composite_frame();
}

static void begin_equaliser_name(void) {
    create_name_entry();
    lv_textarea_set_placeholder_text(name_entry, lang.muxmedia.equaliser_name);
    lv_textarea_set_text(name_entry, "");
    init_osk(name_panel, name_entry, 0, 0, 63);
    key_show = 1;
    osk_show(name_panel);
    char default_name[RANDNAME_MAX_LEN];
    if (wasabi_eq_profile_deletable()) {
        wasabi_eq_profile_label(default_name, sizeof(default_name));
        lv_textarea_set_text(name_entry, default_name);
    } else if (randname_generate_with_separator(default_name, sizeof(default_name), " ") == 0) {
        lv_textarea_set_text(name_entry, default_name);
    }
    naming_equaliser = 1;
    naming_active = 1;
    display_composite_frame();
}

static void populate_asset_page(const int focus) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    const int count = wasabi_asset_browser_count();
    for (int row = 0; row < count; row++)
        add_setting_row(wasabi_asset_browser_glyph(row), wasabi_asset_browser_label(row), "");
    if (count) gen_step_movement(focus >= 0 && focus < count ? focus : 0, 1, 1, 0, 0);
    if (suppress_asset_preview_once)
        suppress_asset_preview_once = 0;
    else
        apply_asset_preview();
    show_nav();
}

static void populate_catalogue_page(const int focus) {
    catalogue_revision = wasabi_catalogue_revision();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    const int count = wasabi_catalogue_count();
    for (int row = 0; row < count; row++)
        add_setting_row(wasabi_catalogue_glyph(row), wasabi_catalogue_label(row), wasabi_catalogue_value(row));
    if (count) gen_step_movement(focus >= 0 && focus < count ? focus : 0, 1, 1, 0, 0);
    show_nav();
}

static void build_asset_page(const wasabi_settings_page page) {
    settings_active = 1;
    bookmarks_active = 0;
    information_active = 0;
    settings_page = page;
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    const wasabi_asset_kind kind = current_asset_kind();
    wasabi_asset_browser_open(kind);
    snprintf(asset_entry_key, sizeof(asset_entry_key), "%s", wasabi_asset_key(kind, wasabi_asset_selected(kind)));
    asset_entry_overlay_mode = config.video.overlay_mode;
    lv_label_set_text(
        ui_lbl_title, kind == wasabi_asset_filter   ? lang.muxretro.display_screen.filter
                      : kind == wasabi_asset_shader ? lang.muxretro.display_screen.shaders
                                                    : lang.muxretro.overlay_screen.image
    );
    populate_asset_page(wasabi_asset_browser_focus());
}

static void add_information_row(const char *glyph, const char *label, const char *value) {
    if (information_count >= information_row_limit) return;
    const int row = information_count++;
    snprintf(information_value_text[row], sizeof(information_value_text[row]), "%s", value ? value : "");
    add_setting_row(glyph, label, information_value_text[row]);
    information_panels[row] = lv_obj_get_child(ui_pnl_content, row);
    information_labels[row] = lv_obj_get_child(information_panels[row], 0);
    information_glyphs[row] = lv_obj_get_child(information_panels[row], 1);
    information_values[row] = lv_obj_get_child(information_panels[row], 2);
}

static void build_information(void) {
    video_player_info information;
    video_player_get_information(&information);

    settings_active = 0;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 1;
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;
    information_count = 0;
    memset(information_panels, 0, sizeof(information_panels));
    memset(information_labels, 0, sizeof(information_labels));
    memset(information_glyphs, 0, sizeof(information_glyphs));
    memset(information_values, 0, sizeof(information_values));

    const int media_first = information_count;
    add_information_row("content", lang.generic.content, get_file_name(playback_uri));
    char location[PATH_MAX];
    snprintf(location, sizeof(location), "%s", playback_uri);
    char *separator = strrchr(location, '/');
    if (separator) {
        if (separator == location)
            separator[1] = '\0';
        else
            *separator = '\0';
    }
    add_information_row("folder", lang.muxretro.information_screen.content_location, location);
    struct stat status;
    char value[128];
    if (stat(playback_uri, &status) == 0 && S_ISREG(status.st_mode)) {
        const double bytes = (double) status.st_size;
        if (bytes >= 1024.0 * 1024.0 * 1024.0)
            snprintf(value, sizeof(value), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
        else if (bytes >= 1024.0 * 1024.0)
            snprintf(value, sizeof(value), "%.1f MB", bytes / (1024.0 * 1024.0));
        else
            snprintf(value, sizeof(value), "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(value, sizeof(value), "%s", information.live ? lang.muxmedia.live_tv : lang.generic.unknown);
    }
    add_information_row("storage", lang.muxretro.information_screen.content_size, value);
    if (information.live)
        snprintf(value, sizeof(value), "%s", lang.muxmedia.live_tv);
    else
        format_time(information.duration > 0.0 ? (uint32_t) information.duration : 0, value, sizeof(value));
    add_information_row("playtime", lang.generic.duration, value);
    information_frames[0] =
        (list_frame) {lang.muxretro.information_screen.section_content, media_first, information_count - media_first};

    int frame = 1;
    if (!information.audio_only) {
        const int video_first = information_count;
        add_information_row("video", lang.muxretro.information_screen.section_video, information.video_codec);
        snprintf(value, sizeof(value), "%d\xC3\x97%d", information.width, information.height);
        add_information_row("resolution", lang.muxmedia.resolution, value);
        add_information_row("videosettings", lang.muxretro.information_screen.pixel_format, information.pixel_format);
        if (information.frame_rate > 0.0)
            snprintf(value, sizeof(value), "%.3f FPS", information.frame_rate);
        else
            snprintf(value, sizeof(value), "%s", lang.generic.unknown);
        add_information_row("fps", lang.muxretro.information_screen.target_fps, value);
        add_information_row(
            "deinterlace", lang.muxmedia.deinterlace,
            information.deinterlace ? lang.generic.enabled : lang.generic.disabled
        );
        snprintf(value, sizeof(value), "%d", information.queued_video);
        add_information_row("video", lang.muxmedia.queued_frames, value);
        information_frames[frame++] =
            (list_frame) {lang.muxretro.information_screen.section_video, video_first, information_count - video_first};
    } else {
        const int audio_first = information_count;
#define ADD_AUDIO_INFO(GLYPH, LABEL, FIELD)                                                                            \
    do {                                                                                                               \
        if (information.FIELD[0]) add_information_row(GLYPH, LABEL, information.FIELD);                                \
    } while (0)
        ADD_AUDIO_INFO("content", lang.muxmedia.audio_title, title);
        ADD_AUDIO_INFO("user", lang.muxmedia.artist, artist);
        ADD_AUDIO_INFO("collection", lang.muxmedia.album, album);
        ADD_AUDIO_INFO("user", lang.muxmedia.album_artist, album_artist);
        ADD_AUDIO_INFO("user", lang.muxmedia.composer, composer);
        ADD_AUDIO_INFO("audio", lang.muxmedia.genre, genre);
        ADD_AUDIO_INFO("calendar", lang.muxmedia.year, year);
        ADD_AUDIO_INFO("audio", lang.muxmedia.track, track);
        ADD_AUDIO_INFO("audio", lang.muxmedia.disc, disc);
        ADD_AUDIO_INFO("information", lang.muxmedia.format, format);
        ADD_AUDIO_INFO("information", lang.muxmedia.encoder, encoder);
        ADD_AUDIO_INFO("information", lang.muxmedia.comment, comment);
        ADD_AUDIO_INFO("information", lang.muxmedia.copyright, copyright);
#undef ADD_AUDIO_INFO
        if (information.bitrate > 0) {
            snprintf(value, sizeof(value), "%d kbps", information.bitrate / 1000);
            add_information_row("information", lang.muxmedia.bitrate, value);
        }
        information_frames[frame++] = (list_frame) {lang.muxmedia.audio, audio_first, information_count - audio_first};
    }

    const int audio_first = information_count;
    add_information_row("audio", lang.muxretro.information_screen.section_audio, information.audio_codec);
    snprintf(value, sizeof(value), "%d Hz", information.audio_rate);
    add_information_row("audio", lang.muxretro.settings_screen.sample_rate, value);
    snprintf(value, sizeof(value), "%d", information.audio_channels);
    add_information_row("audio", lang.muxmedia.channels, value);
    snprintf(value, sizeof(value), "%d", information.queued_audio);
    add_information_row("audio", lang.muxmedia.queued_samples, value);
    information_frames[frame++] =
        (list_frame) {lang.muxretro.information_screen.section_audio, audio_first, information_count - audio_first};

    if (list_frame_init(
            &theme, ui_pnl_content, information_frames, frame, information_panels, information_labels,
            information_glyphs, information_values, information_count
        )) {
        const int steps = list_frame_restore_key("muxmedia_information");
        if (steps > 0) gen_step_movement(steps, 1, 2, 0, 0);
    }
    lv_label_set_text(ui_lbl_title, lang.muxretro.information);
    show_nav();
}

static void refresh_setting_value(const int row) {
    if (row < 0 || row >= wasabi_setting_count || !setting_values[row]) return;
    char value[128];
    wasabi_setting_value((wasabi_setting) row, value, sizeof(value));
    lv_label_set_text(setting_values[row], value);
    adjust_label_value_width(setting_panels[row], setting_labels[row], setting_values[row]);
}

static void change_setting(const int direction) {
    if (settings_page != wasabi_page_root) {
        if (!wasabi_page_cycle(settings_page, current_item_index, direction)) {
            play_sound(snd_error);
            return;
        }
        char value[128];
        wasabi_page_value(settings_page, current_item_index, value, sizeof(value));
        lv_label_set_text(setting_values[current_item_index], value);
        adjust_label_value_width(
            setting_panels[current_item_index], setting_labels[current_item_index], setting_values[current_item_index]
        );
        if (settings_page == wasabi_page_vignette && current_item_index == 0 && setting_values[7]) {
            wasabi_page_value(settings_page, 7, value, sizeof(value));
            lv_label_set_text(setting_values[7], value);
            adjust_label_value_width(setting_panels[7], setting_labels[7], setting_values[7]);
        }
        if (settings_page != wasabi_page_hotkeys) video_render_settings_changed();
        return;
    }
    if (list_frame_focused()) {
        if (list_frame_move(direction)) show_nav();
        return;
    }

    const int row = list_frame_current_row();
    if (row < 0 || !wasabi_setting_can_change((wasabi_setting) row)) return;
    if (!wasabi_setting_cycle((wasabi_setting) row, direction)) {
        play_sound(snd_error);
        return;
    }
    refresh_setting_value(row);
    if (row >= wasabi_setting_hotkey_pause && row <= wasabi_setting_hotkey_slow_motion)
        for (int item = wasabi_setting_hotkey_pause; item <= wasabi_setting_hotkey_slow_motion; item++)
            refresh_setting_value(item);
    if (row >= wasabi_setting_brightness && row <= wasabi_setting_gamma)
        video_player_image_settings_changed();
    else if (row == wasabi_setting_overlay_source) {
        build_settings_at(row);
        video_render_settings_changed();
    } else if ((row >= wasabi_setting_scaling && row <= wasabi_setting_border)
               || (row >= wasabi_setting_overlay_pattern && row <= wasabi_setting_overlay_opacity))
        video_render_settings_changed();
    if (row >= wasabi_setting_volume && row <= wasabi_setting_slow_motion_speed) video_player_audio_settings_changed();
    if (row == wasabi_setting_artwork_position)
        video_player_audio_ui_changed();
    else if (row == wasabi_setting_progress_bar) {
        if (wasabi_settings_audio_active())
            video_player_audio_ui_changed();
        else
            rebuild_timeline_progress();
    }
    if (row == wasabi_setting_repeat || row == wasabi_setting_shuffle) video_player_modes_changed();
    if (row == wasabi_setting_gapless || row == wasabi_setting_crossfade || row == wasabi_setting_auto_play)
        video_player_transition_settings_changed();
}

static void build_bookmarks(void) {
    settings_active = 0;
    bookmarks_active = 1;
    playlist_active = 0;
    information_active = 0;
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    video_state_free(bookmark_entries, bookmark_entry_count);
    bookmark_entries = NULL;
    bookmark_entry_count = 0;
    bookmark_count = 0;
    selected_bookmark_position = -1.0;
    list_frame_reset();
    if (video_bookmark_load(&bookmark_entries, &bookmark_entry_count) < 0) {
        bookmark_entries = NULL;
        bookmark_entry_count = 0;
    }

    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    for (size_t i = 0; i < bookmark_entry_count && bookmark_count < bookmark_view_limit; ++i) {
        if (strcmp(bookmark_entries[i].uri, playback_uri) != 0) continue;
        char position[24];
        format_time(
            bookmark_entries[i].position > 0.0 ? (uint32_t) bookmark_entries[i].position : 0, position, sizeof(position)
        );
        bookmark_indices[bookmark_count++] = i;
        if (video_bookmark_is_quick(bookmark_entries[i].name)) {
            char label[96];
            snprintf(label, sizeof(label), "%s - %s", position, lang.muxretro.gamestate.source_quick);
            add_row("state", label);
        } else if (bookmark_entries[i].name && bookmark_entries[i].name[0]) {
            char label[96];
            snprintf(label, sizeof(label), "%s - %s", position, bookmark_entries[i].name);
            add_row("state", label);
        } else {
            add_row("state", position);
        }
    }

    lv_label_set_text(ui_lbl_title, lang.muxmedia.bookmarks);
    lv_label_set_text(ui_lbl_screen_message, bookmark_count ? "" : lang.muxmedia.empty_bookmarks);
    show_nav();
    if (bookmark_count) gen_step_movement(0, 1, 1, 0, 0);
    update_bookmark_preview();
}

static void build_playlist(void) {
    settings_active = 0;
    bookmarks_active = 0;
    playlist_active = 1;
    information_active = 0;
    hide_bookmark_preview();
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 1;

    size_t capacity = theme.mux.item.count > 0 ? (size_t) theme.mux.item.count * LIST_WINDOW_SCALE : LIST_WINDOW_MIN;
    if (capacity < LIST_WINDOW_MIN) capacity = LIST_WINDOW_MIN;
    if (capacity > LIST_WINDOW_MAX) capacity = LIST_WINDOW_MAX;
    if (capacity > playback_playlist_count) capacity = playback_playlist_count;
    if (playlist_focus_index >= playback_playlist_count) playlist_focus_index = playback_playlist_index;
    playlist_window_start = playlist_focus_index > capacity / 2U ? playlist_focus_index - capacity / 2U : 0;
    if (playlist_window_start + capacity > playback_playlist_count)
        playlist_window_start = playback_playlist_count - capacity;

    const size_t end = playlist_window_start + capacity;
    for (size_t index = playlist_window_start; index < end; index++)
        add_row(
            playback_playlist_channels ? "network"
            : playback_playlist_audio  ? "audio"
                                       : "video",
            playback_playlist[index].title
        );

    lv_label_set_text(
        ui_lbl_title, playback_playlist_channels ? lang.muxmedia.channels
                      : playback_playlist_audio  ? lang.muxmedia.tracks
                                                 : lang.muxmedia.episodes
    );
    lv_label_set_text(ui_lbl_screen_message, "");
    show_nav();
    if (capacity) gen_step_movement((int) (playlist_focus_index - playlist_window_start), 1, 1, 0, 0);
    update_item_counter(
        ui_lbl_counter_explore, playlist_focus_index, playback_playlist_count, config.visual.menu_counter_file
    );
}

static void playlist_focus(const size_t target) {
    if (target >= playback_playlist_count || target == playlist_focus_index) return;

    const size_t previous = playlist_focus_index;
    playlist_focus_index = target;
    if (target >= playlist_window_start && target < playlist_window_start + (size_t) ui_count_static) {
        const int direction = target < previous ? -1 : 1;
        const size_t distance = target < previous ? previous - target : target - previous;
        gen_step_movement((int) distance, direction, 1, 0, 1);
        update_item_counter(
            ui_lbl_counter_explore, playlist_focus_index, playback_playlist_count, config.visual.menu_counter_file
        );
        return;
    }

    gen_step_movement(0, target < previous ? -1 : 1, 1, 0, 1);
    toast_message(lang.muxmedia.loading_more, tst_wait_s);
    build_playlist();
}

static void playlist_move(const int steps, const int direction, const int wrap) {
    playlist_focus(video_playlist_step(
        playback_playlist, playback_playlist_count, playlist_focus_index, direction, steps > 0 ? (size_t) steps : 1U,
        wrap
    ));
}

static void playlist_skip(const int direction) {
    playlist_focus(video_playlist_skip(
        playback_playlist, playback_playlist_count, playlist_focus_index, direction,
        theme.mux.item.count > 0 ? (size_t) theme.mux.item.count : 1U, config.visual.page_skip != 0
    ));
}

int video_playback_ui_init(
    const char *title, const char *uri, const char *content_uri, const int live, const video_library_entry *playlist,
    const size_t playlist_count, const size_t playlist_index, const int playlist_channels
) {
    snprintf(playback_title, sizeof(playback_title), "%s", title && title[0] ? title : lang.muxmedia.title);
    snprintf(playback_uri, sizeof(playback_uri), "%s", uri ? uri : "");
    snprintf(playback_content_uri, sizeof(playback_content_uri), "%s", content_uri ? content_uri : playback_uri);
    live_content = live;
    menu_active = 0;
    settings_active = 0;
    bookmarks_active = 0;
    playlist_active = 0;
    information_active = 0;
    content_switch_active = 0;
    playback_playlist = playlist;
    playback_playlist_count = playlist_count;
    playback_playlist_index = playlist_index < playlist_count ? playlist_index : 0;
    selected_playlist_index = playback_playlist_index;
    playlist_window_start = 0;
    playlist_focus_index = playback_playlist_index;
    playback_playlist_channels = playlist_channels;
    playback_playlist_audio = !playlist_channels && playlist_count > 0 && video_path_is_audio(playlist[0].uri);
    bookmark_entries = NULL;
    bookmark_entry_count = 0;
    bookmark_count = 0;
    naming_active = 0;
    name_panel = NULL;
    name_entry = NULL;
    playtime_started = SDL_GetTicks();
    playtime_shown = UINT32_MAX;
    header_deadline = 0;
    status_deadline = 0;
    mode_deadline = 0;
    timeline_deadline = 0;
    playback_position = 0.0;
    playback_duration = 0.0;
    playback_paused = 0;
    settings_parent_row = -1;
    menu_peeking = 0;
    menu_combo_consumed = 0;
    menu_pressed_at = 0;

    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    set_gradient_visible(0);
    lv_obj_add_flag(ui_pnl_wall, LV_OBJ_FLAG_HIDDEN);

    dim_overlay = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(dim_overlay);
    lv_obj_set_size(dim_overlay, device.mux.width, device.mux.height);
    lv_obj_center(dim_overlay);
    lv_obj_set_style_bg_color(dim_overlay, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dim_overlay, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(dim_overlay);

    lv_obj_t *playtime_glyph = NULL;
    playtime_panel = create_indicator(LV_ALIGN_BOTTOM_RIGHT, &playtime_glyph);
    set_glyph(playtime_glyph, "playtime");
    playtime_label = lv_label_create(playtime_panel);
    lv_obj_set_style_text_color(playtime_label, lv_color_hex(theme.footer.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(playtime_label, theme.footer.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(playtime_label, "00:00");

    status_panel = create_indicator(LV_ALIGN_BOTTOM_LEFT, &status_glyph);
    status_label = lv_label_create(status_panel);
    lv_obj_set_style_text_color(status_label, lv_color_hex(theme.footer.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(status_label, theme.footer.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(status_panel, LV_OBJ_FLAG_HIDDEN);

    mode_panel = create_indicator(LV_ALIGN_BOTTOM_LEFT, &shuffle_glyph);
    repeat_glyph = lv_img_create(mode_panel);
    repeat_badge = lv_label_create(mode_panel);
    lv_obj_set_style_text_color(repeat_badge, lv_color_hex(theme.footer.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(repeat_badge, theme.footer.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(mode_panel, LV_OBJ_FLAG_HIDDEN);
    video_playback_ui_modes_changed();

    const int timeline_edge = 4;
    timeline_padding_x = device.mux.width / 80;
    if (timeline_padding_x < 8) timeline_padding_x = 8;
    if (timeline_padding_x > 16) timeline_padding_x = 16;
    timeline_padding_y = device.mux.height / 96;
    if (timeline_padding_y < 5) timeline_padding_y = 5;
    if (timeline_padding_y > 10) timeline_padding_y = 10;
    const int timeline_height = 58 + timeline_padding_y * 2;
    const int timeline_panel_width = device.mux.width - timeline_edge * 2;
    timeline_width = timeline_panel_width - timeline_padding_x * 2;
    timeline_panel = lv_obj_create(ui_screen);
    lv_obj_set_size(timeline_panel, timeline_panel_width, timeline_height);
    lv_obj_set_style_bg_color(timeline_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(timeline_panel, 140, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(timeline_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(timeline_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(timeline_panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(timeline_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    load_font_section(FONT_FOOTER_DIR, timeline_panel);
    lv_obj_update_layout(playtime_panel);
    lv_obj_align_to(timeline_panel, playtime_panel, LV_ALIGN_OUT_TOP_RIGHT, 0, -timeline_edge);

    rebuild_timeline_progress();

    timeline_current = lv_label_create(timeline_panel);
    timeline_total = lv_label_create(timeline_panel);
    lv_obj_set_style_text_color(timeline_current, lv_color_hex(theme.footer.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(timeline_current, theme.footer.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(timeline_total, lv_color_hex(theme.footer.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(timeline_total, theme.footer.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(timeline_current, "00:00");
    lv_label_set_text(timeline_total, "00:00");
    lv_obj_align(timeline_current, LV_ALIGN_TOP_LEFT, timeline_padding_x, timeline_padding_y + 29);
    lv_obj_align(timeline_total, LV_ALIGN_TOP_RIGHT, -timeline_padding_x, timeline_padding_y + 29);
    lv_obj_add_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(timeline_panel);

    static const char *asset_actions[] = {NULL, NULL, NULL};
    asset_actions[asset_action_collect] = lang.generic.collect;
    asset_actions[asset_action_delete] = lang.muxretro.catalogue_screen.delete_label;
    asset_actions[asset_action_cancel] = lang.generic.cancel;
    dialogue_init(
        &asset_actions_dialogue, &theme, ui_screen, lang.generic.actions, NULL, asset_actions, 3, lang.generic.select,
        lang.generic.cancel
    );
    dialogue_init_remove(
        &asset_delete_dialogue, &theme, ui_screen, lang.muxretro.catalogue_screen.delete_confirm, lang.generic.select,
        lang.generic.cancel
    );
    static const char *save_options[] = {NULL, NULL, NULL, NULL};
    save_options[0] = lang.muxretro.save.content_save;
    save_options[1] = lang.muxretro.save.directory_save;
    save_options[2] = lang.muxretro.save.session_save;
    save_options[3] = lang.generic.discard;
    dialogue_init(
        &settings_save_dialogue, &theme, ui_screen, lang.generic.save, NULL, save_options, 4, lang.generic.select,
        lang.generic.cancel
    );
    dialogue_init_confirm(
        &settings_reset_dialogue, &theme, ui_screen, lang.generic.reset, NULL, lang.generic.reset, lang.generic.cancel,
        lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &bookmark_delete_dialogue, &theme, ui_screen, lang.muxmedia.bookmark_remove, lang.muxmedia.bookmark_remove_desc,
        lang.generic.remove, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &equaliser_delete_dialogue, &theme, ui_screen, lang.muxmedia.equaliser_delete, NULL,
        lang.muxretro.catalogue_screen.delete_label, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    equaliser_delete_dialogue.safe_default = mux_confirm_nah;
    settings_reset_dialogue.safe_default = mux_confirm_nah;
    asset_delete_skip_confirm = 0;

    build_pause();
    apply_playback_chrome();
    return 1;
}

void video_playback_ui_shutdown(void) {
    equaliser_hold_stop();
    content_switch_free(&content_switch_items);
    video_state_free(bookmark_entries, bookmark_entry_count);
    bookmark_entries = NULL;
    bookmark_entry_count = 0;
    bookmark_count = 0;
    naming_active = 0;
    key_show = 0;
    if (name_panel && lv_obj_is_valid(name_panel)) lv_obj_del(name_panel);
    name_panel = NULL;
    name_entry = NULL;
    osk_screen_reset();
    list_frame_reset();
    hide_bookmark_preview();
    if (dim_overlay && lv_obj_is_valid(dim_overlay)) lv_obj_del(dim_overlay);
    if (playtime_panel && lv_obj_is_valid(playtime_panel)) lv_obj_del(playtime_panel);
    if (status_panel && lv_obj_is_valid(status_panel)) lv_obj_del(status_panel);
    if (mode_panel && lv_obj_is_valid(mode_panel)) lv_obj_del(mode_panel);
    if (timeline_panel && lv_obj_is_valid(timeline_panel)) lv_obj_del(timeline_panel);
    if (asset_actions_dialogue.panel && lv_obj_is_valid(asset_actions_dialogue.panel))
        lv_obj_del(asset_actions_dialogue.panel);
    if (asset_actions_dialogue.dim && lv_obj_is_valid(asset_actions_dialogue.dim))
        lv_obj_del(asset_actions_dialogue.dim);
    if (asset_delete_dialogue.panel && lv_obj_is_valid(asset_delete_dialogue.panel))
        lv_obj_del(asset_delete_dialogue.panel);
    if (asset_delete_dialogue.dim && lv_obj_is_valid(asset_delete_dialogue.dim)) lv_obj_del(asset_delete_dialogue.dim);
    if (settings_save_dialogue.panel && lv_obj_is_valid(settings_save_dialogue.panel))
        lv_obj_del(settings_save_dialogue.panel);
    if (settings_save_dialogue.dim && lv_obj_is_valid(settings_save_dialogue.dim))
        lv_obj_del(settings_save_dialogue.dim);
    if (bookmark_delete_dialogue.panel && lv_obj_is_valid(bookmark_delete_dialogue.panel))
        lv_obj_del(bookmark_delete_dialogue.panel);
    if (bookmark_delete_dialogue.dim && lv_obj_is_valid(bookmark_delete_dialogue.dim))
        lv_obj_del(bookmark_delete_dialogue.dim);
    memset(&asset_actions_dialogue, 0, sizeof(asset_actions_dialogue));
    memset(&asset_delete_dialogue, 0, sizeof(asset_delete_dialogue));
    memset(&settings_save_dialogue, 0, sizeof(settings_save_dialogue));
    if (equaliser_delete_dialogue.panel && lv_obj_is_valid(equaliser_delete_dialogue.panel))
        lv_obj_del(equaliser_delete_dialogue.panel);
    if (equaliser_delete_dialogue.dim && lv_obj_is_valid(equaliser_delete_dialogue.dim))
        lv_obj_del(equaliser_delete_dialogue.dim);
    memset(&bookmark_delete_dialogue, 0, sizeof(bookmark_delete_dialogue));
    memset(&equaliser_delete_dialogue, 0, sizeof(equaliser_delete_dialogue));
    dim_overlay = NULL;
    playtime_panel = NULL;
    playtime_label = NULL;
    status_panel = NULL;
    status_glyph = NULL;
    status_label = NULL;
    mode_panel = NULL;
    shuffle_glyph = NULL;
    repeat_glyph = NULL;
    repeat_badge = NULL;
    timeline_panel = NULL;
    timeline_track = NULL;
    timeline_current = NULL;
    timeline_total = NULL;
    timeline_width = 0;
    timeline_padding_x = 0;
    timeline_padding_y = 0;
    wasabi_progress_reset(&timeline_progress);

    lv_obj_clear_flag(ui_pnl_wall, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(
        ui_screen, theme.system.background_gradient_direction == 0 ? theme.system.background_alpha : LV_OPA_TRANSP,
        MU_OBJ_MAIN_DEFAULT
    );
    set_gradient_visible(1);
    restore_header();
    display_set_ui_hidden(0);
}

int video_playback_ui_menu_active(void) {
    return menu_active;
}

int video_playback_ui_confirmable(void) {
    if (!menu_active || information_active) return 0;
    if (equaliser_active()) return !live_content;
    if (bookmarks_active) return bookmark_count > 0;
    if (content_switch_active) return content_switch_items.count > 0;
    if (!settings_active) return 1;
    if (settings_page == wasabi_page_root) {
        const int row = list_frame_current_row();
        return !list_frame_focused() && row >= 0
               && (wasabi_setting_is_action((wasabi_setting) row) || wasabi_setting_can_change((wasabi_setting) row));
    }
    if (wasabi_catalogue_active()) return wasabi_catalogue_count() > 0;
    if (asset_page()) return wasabi_asset_browser_type(current_item_index) != wasabi_asset_row_empty;
    return current_item_index >= 0;
}

int video_playback_ui_y_actionable(void) {
    if (!menu_active) return 0;
    if (equaliser_active()) return 1;
    if (bookmarks_active) return !live_content;
    return settings_active && asset_page() && !wasabi_catalogue_active()
           && wasabi_asset_browser_type(current_item_index) == wasabi_asset_row_item;
}

void video_playback_ui_menu_press(void) {
    menu_pressed_at = SDL_GetTicks();
}

void video_playback_ui_menu_consume(void) {
    menu_combo_consumed = 1;
    menu_pressed_at = 0;
    set_menu_peeking(0);
}

int video_playback_ui_menu_release(void) {
    const uint32_t elapsed = menu_pressed_at ? SDL_GetTicks() - menu_pressed_at : 0;
    menu_pressed_at = 0;
    const int held = menu_combo_consumed || (menu_active && (menu_peeking || elapsed >= menu_peek_delay_ms));
    menu_combo_consumed = 0;
    set_menu_peeking(0);
    return held;
}

video_ui_action video_playback_ui_toggle_menu(void) {
    if (!menu_active) {
        build_pause();
        menu_active = 1;
        apply_menu_chrome();
        display_composite_frame();
        return video_ui_action_opened;
    }
    if (settings_active && settings_page == wasabi_page_equaliser) {
        leave_equaliser();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (settings_active || bookmarks_active || playlist_active || information_active || content_switch_active) {
        if (settings_active && settings_page == wasabi_page_root && guard_settings_exit()) return video_ui_action_none;
        if (settings_active && settings_page == wasabi_page_root)
            list_frame_remember_section_key("muxmedia_settings");
        else if (information_active)
            list_frame_remember_section_key("muxmedia_information");
        const int focus = settings_active         ? settings_row
                          : bookmarks_active      ? bookmark_row
                          : playlist_active       ? playlist_row
                          : content_switch_active ? content_switch_row
                                                  : information_row;
        build_pause_at(focus);
        display_composite_frame();
        return video_ui_action_none;
    }
    menu_active = 0;
    apply_playback_chrome();
    display_composite_frame();
    return video_ui_action_closed;
}

video_ui_action video_playback_ui_confirm(void) {
    if (!menu_active) return video_ui_action_none;
    if (equaliser_active()) {
        if (live_content) return video_ui_action_none;
        video_player_toggle_menu_playback();
        show_nav();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (content_switch_active) {
        if (current_item_index < 0 || (size_t) current_item_index >= content_switch_items.count)
            return video_ui_action_none;
        const content_switch_entry *entry = &content_switch_items.entries[current_item_index];
        if (!content_switch_write_request(entry->path, entry->native)) {
            play_sound(snd_error);
            return video_ui_action_none;
        }
        return video_ui_action_switch_content;
    }
    if (settings_active) {
        if (settings_page != wasabi_page_root) {
            if (asset_page()) {
                const wasabi_asset_kind kind = current_asset_kind();
                if (wasabi_catalogue_active()) {
                    const int focus = current_item_index;
                    if (wasabi_catalogue_confirm(focus)) populate_catalogue_page(0);
                    display_composite_frame();
                    return video_ui_action_none;
                }
                const wasabi_asset_row_type type = wasabi_asset_browser_type(current_item_index);
                if (wasabi_asset_browser_enter(current_item_index)) {
                    populate_asset_page(wasabi_asset_browser_focus());
                    display_composite_frame();
                    return video_ui_action_none;
                }
                if (type == wasabi_asset_row_download) {
                    wasabi_catalogue_open(kind);
                    return video_ui_action_none;
                }
                if (type == wasabi_asset_row_empty) return video_ui_action_none;
                const int item = wasabi_asset_browser_item(current_item_index);
                if (item < 0 || !wasabi_asset_select(kind, item)) play_sound(snd_error);
                if (kind == wasabi_asset_overlay) {
                    config.video.overlay_mode = item > 0 ? 3 : 0;
                    video_render_settings_changed();
                } else if (kind == wasabi_asset_shader) {
                    video_player_effect_settings_changed();
                } else {
                    video_player_effect_settings_changed();
                }
                if (wasabi_session_dirty()) {
                    save_return_to_settings = 1;
                    save_return_to_equaliser = 0;
                    dialogue_open(&settings_save_dialogue, &theme);
                } else {
                    build_settings_at(settings_parent_row);
                }
                display_composite_frame();
                return video_ui_action_none;
            }
            if (settings_page == wasabi_page_shader_parameters
                && current_item_index == video_effects_parameter_count()) {
                video_effects_parameters_reset();
                video_render_settings_changed();
                for (int item = 0; item < video_effects_parameter_count(); item++) {
                    char value[128];
                    wasabi_page_value(settings_page, item, value, sizeof(value));
                    lv_label_set_text(setting_values[item], value);
                }
                display_composite_frame();
                return video_ui_action_none;
            }
            change_setting(1);
            display_composite_frame();
            return video_ui_action_none;
        }
        const int row = list_frame_current_row();
        settings_parent_row = row;
        if (row == wasabi_setting_viewport_reset) {
            if (!wasabi_setting_reset_viewport())
                play_sound(snd_error);
            else {
                video_render_settings_changed();
            }
        } else if (row == wasabi_setting_overlay_reset) {
            if (!wasabi_setting_reset_overlay()) play_sound(snd_error);
            video_render_settings_changed();
        } else if (row == wasabi_setting_reset) {
            dialogue_open(&settings_reset_dialogue, &theme);
        } else if (row == wasabi_setting_vignette) {
            build_settings_page(wasabi_page_vignette);
        } else if (row == wasabi_setting_colour_filter) {
            build_asset_page(wasabi_page_colour_filter);
        } else if (row == wasabi_setting_shader) {
            build_asset_page(wasabi_page_shader);
        } else if (row == wasabi_setting_overlay_image) {
            build_asset_page(wasabi_page_overlay_image);
        } else if (row == wasabi_setting_overlay_adjustment) {
            build_settings_page(wasabi_page_overlay_adjustment);
        } else if (row == wasabi_setting_overlay_cropping) {
            build_settings_page(wasabi_page_overlay_cropping);
        } else if (row == wasabi_setting_viewport_adjustment) {
            build_settings_page(wasabi_page_viewport_adjustment);
        } else if (row == wasabi_setting_viewport_cropping) {
            build_settings_page(wasabi_page_viewport_cropping);
        } else if (row >= 0 && wasabi_setting_can_change((wasabi_setting) row)) {
            change_setting(1);
        }
        display_composite_frame();
        return video_ui_action_none;
    }
    if (bookmarks_active) {
        if (!bookmark_count) return video_ui_action_none;
        selected_bookmark_position = bookmark_entries[bookmark_indices[current_item_index]].position;
        build_pause();
        menu_active = 0;
        apply_playback_chrome();
        display_composite_frame();
        return video_ui_action_load_bookmark;
    }
    if (playlist_active) {
        if (!playback_playlist_count || current_item_index < 0
            || (size_t) current_item_index >= (size_t) ui_count_static)
            return video_ui_action_none;
        selected_playlist_index = playlist_window_start + (size_t) current_item_index;
        return video_ui_action_load_playlist;
    }
    if (current_item_index == 0) {
        menu_active = 0;
        apply_playback_chrome();
        display_composite_frame();
        return video_ui_action_closed;
    }
    if (current_item_index == bookmark_row) {
        remembered_row = &bookmark_row;
        build_bookmarks();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (current_item_index == playlist_row) {
        remembered_row = &playlist_row;
        playlist_focus_index = playback_playlist_index;
        build_playlist();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (current_item_index == content_switch_row) {
        remembered_row = &content_switch_row;
        build_content_switch();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (current_item_index == information_row) {
        remembered_row = &information_row;
        build_information();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (current_item_index == restart_row) {
        build_pause();
        menu_active = 0;
        apply_playback_chrome();
        display_composite_frame();
        return video_ui_action_restart;
    }
    if (current_item_index == equaliser_row) {
        remembered_row = &equaliser_row;
        build_equaliser();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (current_item_index == settings_row) {
        remembered_row = &settings_row;
        settings_parent_row = -1;
        build_settings();
        display_composite_frame();
        return video_ui_action_none;
    }
    if (current_item_index == stop_row) return video_ui_action_stop;
    return video_ui_action_none;
}

video_ui_action video_playback_ui_back(void) {
    if (!menu_active) return video_ui_action_none;
    if (settings_active || bookmarks_active || playlist_active || information_active || content_switch_active) {
        if (settings_active && settings_page == wasabi_page_equaliser) {
            leave_equaliser();
            display_composite_frame();
            return video_ui_action_none;
        }
        if (settings_active && settings_page != wasabi_page_root) {
            if (wasabi_catalogue_active()) {
                if (wasabi_catalogue_back())
                    populate_catalogue_page(0);
                else {
                    wasabi_asset_browser_open(current_asset_kind());
                    populate_asset_page(wasabi_asset_browser_focus());
                }
                display_composite_frame();
                return video_ui_action_none;
            }
            if (settings_page == wasabi_page_shader_parameters && shader_parameters_from_browser) {
                shader_parameters_from_browser = 0;
                build_asset_page(wasabi_page_shader);
                display_composite_frame();
                return video_ui_action_none;
            }
            if (asset_page() && wasabi_asset_browser_back()) {
                suppress_asset_preview_once = 1;
                populate_asset_page(wasabi_asset_browser_focus());
                display_composite_frame();
                return video_ui_action_none;
            }
            if (asset_page()) restore_asset_entry();
            build_settings_at(settings_parent_row);
            display_composite_frame();
            return video_ui_action_none;
        }
        if (settings_active && settings_page == wasabi_page_root && guard_settings_exit()) return video_ui_action_none;
        if (settings_active && settings_page == wasabi_page_root)
            list_frame_remember_section_key("muxmedia_settings");
        else if (information_active)
            list_frame_remember_section_key("muxmedia_information");
        const int focus = settings_active         ? settings_row
                          : bookmarks_active      ? bookmark_row
                          : playlist_active       ? playlist_row
                          : content_switch_active ? content_switch_row
                                                  : information_row;
        build_pause_at(focus);
        display_composite_frame();
        return video_ui_action_none;
    }
    menu_active = 0;
    apply_playback_chrome();
    display_composite_frame();
    return video_ui_action_closed;
}

void video_playback_ui_move(const int steps, const int direction) {
    if (equaliser_active()) {
        equaliser_adjust(direction < 0 ? steps : -steps);
        return;
    }
    if (!menu_active || ui_count_static < 2) return;
    if (playlist_active) {
        playlist_move(steps, direction, 1);
        return;
    }
    gen_step_movement(steps, direction, settings_active || information_active ? 2 : 1, 0, 1);
    if (bookmarks_active) update_bookmark_preview();
    if (settings_active) {
        apply_asset_preview();
        show_nav();
    }
}

void video_playback_ui_move_held(const int steps, const int direction) {
    if (equaliser_active()) {
        equaliser_hold(direction < 0 ? 1 : -1);
        return;
    }
    if (!menu_active || ui_count_static < 2) return;
    if (playlist_active) {
        playlist_move(steps, direction, 0);
        return;
    }
    int bounded = steps;
    if (direction < 0 && bounded > current_item_index) bounded = current_item_index;
    if (direction > 0 && bounded > ui_count_static - current_item_index - 1)
        bounded = ui_count_static - current_item_index - 1;
    if (bounded <= 0) return;
    gen_step_movement(bounded, direction, settings_active || information_active ? 2 : 1, 0, 1);
    if (bookmarks_active) update_bookmark_preview();
    if (settings_active) {
        apply_asset_preview();
        show_nav();
    }
}

void video_playback_ui_section(const int direction) {
    if (!menu_active) return;
    if (equaliser_active()) {
        equaliser_profile(direction);
        return;
    }
    if (playlist_active) {
        playlist_skip(direction);
        return;
    }
    if (!settings_active && !information_active) {
        const int page = theme.mux.item.count > 0 ? theme.mux.item.count : 1;
        const int index = current_item_index;
        video_playback_ui_move_held(page, direction);
        if (current_item_index != index) play_sound(snd_navigate);
        return;
    }
    if (!list_frame_active()) return;
    if (!list_frame_move(direction)) return;
    play_sound(snd_navigate);
    gen_step_movement(0, 1, 2, 0, 0);
    show_nav();
    display_composite_frame();
}

void video_playback_ui_shuffle(void) {
    if (!menu_active || !playlist_active || !config.visual.shuffle || playback_playlist_count < 2) return;

    const size_t pick = (size_t) random() % (playback_playlist_count - 1);
    const size_t target = pick >= playlist_focus_index ? pick + 1 : pick;

    play_sound(snd_navigate);
    playlist_focus(target);
}

void video_playback_ui_change(const int direction) {
    if (!menu_active) return;
    if (equaliser_active()) {
        equaliser_select(direction);
        return;
    }
    if (information_active) {
        if (!list_frame_focused()) return;
        if (list_frame_move(direction)) {
            play_sound(snd_option);
            gen_step_movement(0, 1, 2, 0, 0);
            show_nav();
        }
        display_composite_frame();
        return;
    }
    if (!settings_active || asset_page()) return;
    if (settings_page == wasabi_page_root) {
        const int row = list_frame_current_row();
        if (!list_frame_focused() && (row < 0 || !wasabi_setting_can_change((wasabi_setting) row))) return;
    } else if (settings_page == wasabi_page_shader_parameters
               && current_item_index >= video_effects_parameter_count()) {
        return;
    }
    play_sound(snd_option);
    change_setting(direction);
    display_composite_frame();
}

void video_playback_ui_extra(void) {
    if (!menu_active || !settings_active) return;
    if (settings_page == wasabi_page_equaliser) {
        if (!wasabi_eq_profile_deletable()) return;
        play_sound(snd_confirm);
        dialogue_open(&equaliser_delete_dialogue, &theme);
        display_composite_frame();
        return;
    }
    if (settings_page == wasabi_page_root) {
        const int row = list_frame_current_row();
        if (row == wasabi_setting_shader && video_effects_parameter_count() > 0) {
            play_sound(snd_confirm);
            settings_parent_row = row;
            build_settings_page(wasabi_page_shader_parameters);
            display_composite_frame();
            return;
        }
        if (row < wasabi_setting_hotkey_pause || row > wasabi_setting_hotkey_slow_motion) return;
        wasabi_hotkey_reset();
        for (int item = wasabi_setting_hotkey_pause; item <= wasabi_setting_hotkey_slow_motion; item++)
            refresh_setting_value(item);
        display_composite_frame();
        return;
    }
    if (settings_page == wasabi_page_shader && !wasabi_catalogue_active()
        && wasabi_asset_browser_type(current_item_index) == wasabi_asset_row_item) {
        const int item = wasabi_asset_browser_item(current_item_index);
        if (item < 0 || !wasabi_asset_select(wasabi_asset_shader, item)) {
            play_sound(snd_error);
            return;
        }
        video_player_effect_settings_changed();
        if (video_effects_parameter_count() > 0) {
            play_sound(snd_confirm);
            shader_parameters_from_browser = 1;
            build_settings_page(wasabi_page_shader_parameters);
            display_composite_frame();
        } else {
            play_sound(snd_error);
            toast_message(lang.muxretro.shader_screen.no_adjust, tst_wait_m);
        }
        return;
    }
    if (asset_page() && settings_page != wasabi_page_shader && !wasabi_catalogue_active()
        && wasabi_asset_browser_removable(current_item_index)) {
        play_sound(snd_confirm);
        if (asset_delete_skip_confirm)
            video_playback_ui_modal_confirm();
        else
            dialogue_open(&asset_delete_dialogue, &theme);
        return;
    }
    if (settings_page != wasabi_page_hotkeys) return;
    wasabi_hotkey_reset();
    for (int row = 0; row < wasabi_page_row_count(settings_page); row++) {
        char value[128];
        wasabi_page_value(settings_page, row, value, sizeof(value));
        lv_label_set_text(setting_values[row], value);
        adjust_label_value_width(setting_panels[row], setting_labels[row], setting_values[row]);
    }
    display_composite_frame();
}

int video_playback_ui_collect(void) {
    if (!video_playback_ui_y_actionable()) return 0;
    if (equaliser_active()) {
        begin_equaliser_name();
        return 1;
    }
    if (settings_page == wasabi_page_shader && wasabi_asset_browser_removable(current_item_index)) {
        if (wasabi_asset_browser_collected(current_item_index)) {
            if (asset_delete_skip_confirm)
                video_playback_ui_modal_confirm();
            else
                dialogue_open(&asset_delete_dialogue, &theme);
        } else {
            dialogue_open_at(&asset_actions_dialogue, &theme, asset_action_collect);
        }
        display_composite_frame();
        return 1;
    }
    const int focus = current_item_index;
    if (wasabi_asset_browser_toggle_collection(focus) < 0) {
        play_sound(snd_error);
        return 0;
    }
    populate_asset_page(focus);
    display_composite_frame();
    return 1;
}

static void delete_selected_asset(void) {
    const wasabi_asset_kind kind = current_asset_kind();
    const int focus = current_item_index;
    if (!wasabi_asset_browser_delete(focus)) {
        play_sound(snd_error);
        toast_message(lang.muxretro.catalogue_screen.delete_failed, tst_wait_m);
        return;
    }
    if (kind == wasabi_asset_filter || kind == wasabi_asset_shader)
        video_player_effect_settings_changed();
    else
        video_render_settings_changed();
    populate_asset_page(focus < wasabi_asset_browser_count() ? focus : wasabi_asset_browser_count() - 1);
    toast_message(lang.muxretro.catalogue_screen.delete_done, tst_wait_m);
}

int video_playback_ui_modal_active(void) {
    return dialogue_active(&asset_actions_dialogue) || dialogue_active(&asset_delete_dialogue)
           || dialogue_active(&settings_save_dialogue) || dialogue_active(&settings_reset_dialogue)
           || dialogue_active(&bookmark_delete_dialogue) || dialogue_active(&equaliser_delete_dialogue);
}

void video_playback_ui_modal_move(const int direction) {
    mux_dialogue *dialogue = dialogue_active(&asset_actions_dialogue)      ? &asset_actions_dialogue
                             : dialogue_active(&asset_delete_dialogue)     ? &asset_delete_dialogue
                             : dialogue_active(&settings_save_dialogue)    ? &settings_save_dialogue
                             : dialogue_active(&settings_reset_dialogue)   ? &settings_reset_dialogue
                             : dialogue_active(&bookmark_delete_dialogue)  ? &bookmark_delete_dialogue
                             : dialogue_active(&equaliser_delete_dialogue) ? &equaliser_delete_dialogue
                                                                           : NULL;
    if (dialogue) dialogue_handle_dpad(dialogue, &theme, direction, 1);
}

void video_playback_ui_modal_confirm(void) {
    if (dialogue_active(&equaliser_delete_dialogue)) {
        const mux_confirm_opt action = (mux_confirm_opt) equaliser_delete_dialogue.selected;
        dialogue_dismiss(&equaliser_delete_dialogue);
        if (action == mux_confirm_yep) {
            if (wasabi_eq_profile_delete()) {
                toast_message(lang.muxmedia.equaliser_deleted, tst_wait_m);
                update_equaliser_view();
                show_nav();
            } else {
                play_sound(snd_error);
            }
        }
        display_composite_frame();
        return;
    }
    if (dialogue_active(&bookmark_delete_dialogue)) {
        const mux_confirm_opt action = (mux_confirm_opt) bookmark_delete_dialogue.selected;
        dialogue_dismiss(&bookmark_delete_dialogue);
        if (action == mux_confirm_yep) video_playback_ui_delete_bookmark();
        display_composite_frame();
        return;
    }
    if (dialogue_active(&settings_reset_dialogue)) {
        const mux_confirm_opt action = (mux_confirm_opt) settings_reset_dialogue.selected;
        dialogue_dismiss(&settings_reset_dialogue);
        if (action == mux_confirm_yep) {
            wasabi_session_reset();
            apply_session_preview();
            build_settings_at(wasabi_setting_reset);
        }
        display_composite_frame();
        return;
    }
    if (dialogue_active(&settings_save_dialogue)) {
        const int choice = settings_save_dialogue.selected;
        if (!wasabi_session_save(choice)) {
            play_sound(snd_error);
            return;
        }
        dialogue_dismiss(&settings_save_dialogue);
        if (choice == 3) apply_session_preview();
        if (save_return_to_equaliser) {
            clear_equaliser_view();
            build_pause_at(equaliser_row);
        } else {
            list_frame_remember_section_key("muxmedia_settings");
            if (save_return_to_settings)
                build_settings_at(settings_parent_row);
            else
                build_pause_at(settings_row);
        }
        save_return_to_settings = 0;
        save_return_to_equaliser = 0;
        display_composite_frame();
        return;
    }
    if (dialogue_active(&asset_actions_dialogue)) {
        const int action = asset_actions_dialogue.selected;
        dialogue_dismiss(&asset_actions_dialogue);
        if (action == asset_action_collect) {
            const int focus = current_item_index;
            if (wasabi_asset_browser_toggle_collection(focus) < 0) {
                play_sound(snd_error);
                toast_message(lang.muxretro.catalogue_screen.collection_failed, tst_wait_m);
            } else {
                populate_asset_page(focus);
                toast_message(lang.muxretro.catalogue_screen.collection_added, tst_wait_m);
            }
        } else if (action == asset_action_delete) {
            if (asset_delete_skip_confirm)
                delete_selected_asset();
            else
                dialogue_open(&asset_delete_dialogue, &theme);
        }
        display_composite_frame();
        return;
    }
    if (dialogue_active(&asset_delete_dialogue)) {
        const mux_remove_opt action = (mux_remove_opt) asset_delete_dialogue.selected;
        dialogue_dismiss(&asset_delete_dialogue);
        if (action == mux_remove_skip) asset_delete_skip_confirm = 1;
        if (action != mux_remove_nah) delete_selected_asset();
        display_composite_frame();
        return;
    }
    if (asset_delete_skip_confirm && settings_active && asset_page()) {
        play_sound(snd_confirm);
        delete_selected_asset();
        display_composite_frame();
    }
}

void video_playback_ui_modal_cancel(void) {
    if (dialogue_active(&asset_actions_dialogue)) {
        dialogue_mark_cancelled(&asset_actions_dialogue);
        dialogue_dismiss(&asset_actions_dialogue);
    } else if (dialogue_active(&asset_delete_dialogue)) {
        dialogue_mark_cancelled(&asset_delete_dialogue);
        dialogue_dismiss(&asset_delete_dialogue);
    } else if (dialogue_active(&settings_save_dialogue)) {
        dialogue_mark_cancelled(&settings_save_dialogue);
        dialogue_dismiss(&settings_save_dialogue);
    } else if (dialogue_active(&settings_reset_dialogue)) {
        dialogue_mark_cancelled(&settings_reset_dialogue);
        dialogue_dismiss(&settings_reset_dialogue);
    } else if (dialogue_active(&bookmark_delete_dialogue)) {
        dialogue_mark_cancelled(&bookmark_delete_dialogue);
        dialogue_dismiss(&bookmark_delete_dialogue);
    } else if (dialogue_active(&equaliser_delete_dialogue)) {
        dialogue_mark_cancelled(&equaliser_delete_dialogue);
        dialogue_dismiss(&equaliser_delete_dialogue);
    }
    display_composite_frame();
}

void video_playback_ui_tick(void) {
    const uint32_t now = SDL_GetTicks();
    int redraw = 0;
    wasabi_catalogue_tick();
    const unsigned current_catalogue_revision = wasabi_catalogue_revision();
    if (current_catalogue_revision != catalogue_revision) {
        const int focus = current_item_index;
        catalogue_revision = current_catalogue_revision;
        if (menu_active && settings_active && asset_page()) {
            if (wasabi_catalogue_active())
                populate_catalogue_page(focus);
            else
                populate_asset_page(focus);
            redraw = 1;
        }
    }
    if (menu_active && !naming_active && !video_playback_ui_modal_active() && !menu_combo_consumed && menu_pressed_at
        && mux_input_pressed(mux_input_menu) && SDL_TICKS_PASSED(now, menu_pressed_at + menu_peek_delay_ms))
        set_menu_peeking(1);
    if (menu_active && settings_active && settings_page == wasabi_page_vignette && current_item_index == 0) {
        const int held[2] = {mux_input_pressed(mux_input_x), mux_input_pressed(mux_input_y)};
        const int shapes[2] = {4, 5};
        for (int index = 0; index < 2; index++) {
            if (!held[index]) {
                vignette_secret_holding[index] = 0;
                vignette_secret_fired[index] = 0;
                continue;
            }
            if (!vignette_secret_holding[index]) {
                vignette_secret_holding[index] = 1;
                vignette_secret_since[index] = now;
                continue;
            }
            if (!vignette_secret_fired[index] && SDL_TICKS_PASSED(now, vignette_secret_since[index] + 1500)) {
                vignette_secret_fired[index] = 1;
                if (wasabi_vignette_secret(shapes[index])) {
                    char value[128];
                    wasabi_page_value(settings_page, 0, value, sizeof(value));
                    lv_label_set_text(setting_values[0], value);
                    video_render_settings_changed();
                    play_sound(snd_confirm);
                    redraw = 1;
                }
            }
        }
    } else {
        memset(vignette_secret_holding, 0, sizeof(vignette_secret_holding));
        memset(vignette_secret_fired, 0, sizeof(vignette_secret_fired));
    }
    if (!menu_active && config.video.show_playtime) {
        const uint32_t elapsed = (now - playtime_started) / 1000;
        if (elapsed != playtime_shown) {
            char value[24];
            playtime_shown = elapsed;
            format_time(elapsed, value, sizeof(value));
            lv_label_set_text(playtime_label, value);
            redraw = 1;
        }
    }
    if (!menu_active && config.video.header_visibility && SDL_TICKS_PASSED(now, header_deadline)) {
        update_header_playback_time();
        if (header_shows_clock()) datetime_task(NULL);
        if (header_shows_battery()) battery_capacity_task(NULL);
        header_deadline = now + 1000;
        redraw = 1;
    }
    if (mode_deadline && SDL_TICKS_PASSED(now, mode_deadline)) {
        mode_deadline = 0;
        lv_obj_add_flag(mode_panel, LV_OBJ_FLAG_HIDDEN);
        align_status_panel();
        redraw = 1;
    }
    if (timeline_deadline && SDL_TICKS_PASSED(now, timeline_deadline)) {
        timeline_deadline = 0;
        lv_obj_add_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN);
        redraw = 1;
    }
    if (status_deadline && SDL_TICKS_PASSED(now, status_deadline)) {
        status_deadline = 0;
        lv_obj_add_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
        redraw = 1;
    }
    apply_visibility();
    if (redraw) display_composite_frame();
}

void video_playback_ui_set_paused(const int paused) {
    playback_paused = paused;
    if (menu_active) return;
    if (paused) {
        show_paused_status();
        show_timeline();
    } else {
        set_glyph(status_glyph, "resume");
        lv_obj_set_width(status_label, LV_SIZE_CONTENT);
        lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(status_label, lang.muxmedia.play);
        align_status_panel();
        lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
        status_deadline = SDL_GetTicks() + 1500;
        if (timeline_panel && !lv_obj_has_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN))
            timeline_deadline = SDL_GetTicks() + 2000;
    }
    apply_visibility();
    display_composite_frame();
}

void video_playback_ui_update_position(const double position, const double duration) {
    playback_position = position;
    playback_duration = duration;
    if (timeline_panel && !lv_obj_has_flag(timeline_panel, LV_OBJ_FLAG_HIDDEN)) update_timeline();
}

void video_playback_ui_header_changed(void) {
    if (menu_active) return;
    apply_gameplay_header();
    apply_visibility();
    display_composite_frame();
}

void video_playback_ui_modes_changed(void) {
    if (!mode_panel) return;
    const int show_shuffle = !live_content && !wasabi_settings_audio_active() && config.video.shuffle;
    const int show_repeat = !live_content && !wasabi_settings_audio_active() && config.video.repeat_mode;
    if (menu_active || (!show_shuffle && !show_repeat)) {
        lv_obj_add_flag(mode_panel, LV_OBJ_FLAG_HIDDEN);
        mode_deadline = 0;
        align_status_panel();
        apply_visibility();
        return;
    }

    if (show_shuffle) {
        set_glyph(shuffle_glyph, "shuffle");
        lv_obj_clear_flag(shuffle_glyph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(shuffle_glyph, LV_OBJ_FLAG_HIDDEN);
    }
    if (show_repeat) {
        set_glyph(repeat_glyph, "repeat");
        lv_obj_clear_flag(repeat_glyph, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(repeat_badge, config.video.repeat_mode == 1 ? "1" : "A");
        lv_obj_clear_flag(repeat_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(repeat_glyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(repeat_badge, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(mode_panel, LV_OBJ_FLAG_HIDDEN);
    mode_deadline = SDL_GetTicks() + 1500;
    lv_obj_align(mode_panel, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    align_status_panel();
    apply_visibility();
}

void video_playback_ui_show_position(const double position, const double duration) {
    if (menu_active) return;
    video_playback_ui_update_position(position, duration);
    show_timeline();
    apply_visibility();
    display_composite_frame();
}

void video_playback_ui_show_speed(const char *value, const char *glyph) {
    if (menu_active || !status_panel) return;
    if (!value || !value[0] || !glyph || !glyph[0]) {
        status_deadline = 0;
        if (playback_paused)
            show_paused_status();
        else
            lv_obj_add_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        set_glyph(status_glyph, glyph);
        lv_obj_set_width(status_label, LV_SIZE_CONTENT);
        lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(status_label, value);
        align_status_panel();
        lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
        status_deadline = 0;
    }
    apply_visibility();
    display_composite_frame();
}

void video_playback_ui_show_channel(const size_t index) {
    if (menu_active || !playback_playlist_channels || !playback_playlist || index >= playback_playlist_count) return;

    char value[PATH_MAX + 32];
    snprintf(
        value, sizeof(value), "%zu / %zu  %s", index + 1, playback_playlist_count,
        playback_playlist[index].title && playback_playlist[index].title[0] ? playback_playlist[index].title
                                                                            : lang.muxmedia.channels
    );
    set_glyph(status_glyph, "network");
    lv_label_set_text(status_label, value);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(status_label, device.mux.width * 3 / 4);
    align_status_panel();
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
    status_deadline = 0;
    apply_visibility();
    display_composite_frame();
}

void video_playback_ui_hide_channel(void) {
    status_deadline = 0;
    if (playback_paused && !menu_active)
        show_paused_status();
    else
        lv_obj_add_flag(status_panel, LV_OBJ_FLAG_HIDDEN);
    apply_visibility();
    display_composite_frame();
}

void video_playback_ui_prepare_channel_transition(void) {
    menu_active = 0;
    menu_peeking = 0;
    apply_playback_chrome();
}

int video_playback_ui_bookmarks_active(void) {
    return menu_active && bookmarks_active;
}

video_ui_action video_playback_ui_begin_bookmark_name(void) {
    if (live_content || naming_active) return video_ui_action_none;

    const int opened = !menu_active;
    if (opened) {
        menu_active = 1;
        apply_menu_chrome();
    }
    if (!bookmarks_active) build_bookmarks();

    create_name_entry();
    lv_textarea_set_placeholder_text(name_entry, lang.muxmedia.bookmark_name);
    lv_textarea_set_text(name_entry, "");
    init_osk(name_panel, name_entry, 0, 0, 127);
    naming_equaliser = 0;
    key_show = 1;
    osk_show(name_panel);
    char default_name[RANDNAME_MAX_LEN];
    if (randname_generate_with_separator(default_name, sizeof(default_name), " ") == 0)
        lv_textarea_set_text(name_entry, default_name);
    naming_active = 1;
    display_composite_frame();
    return opened ? video_ui_action_opened : video_ui_action_none;
}

int video_playback_ui_naming_active(void) {
    return naming_active;
}

int video_playback_ui_name_finish(char *name, const size_t name_size) {
    if (!naming_active || !name || !name_size) return 0;
    const char *text = lv_textarea_get_text(name_entry);
    while (text && (*text == ' ' || *text == '\t'))
        text++;
    if (!text || !text[0]) {
        play_sound(snd_error);
        return 0;
    }

    snprintf(name, name_size, "%s", text);
    size_t length = strlen(name);
    while (length && (name[length - 1] == ' ' || name[length - 1] == '\t'))
        name[--length] = '\0';
    if (!length) {
        play_sound(snd_error);
        return 0;
    }

    close_name_entry(0);
    if (naming_equaliser) {
        naming_equaliser = 0;
        if (wasabi_eq_profile_save(name)) {
            toast_message(lang.muxmedia.equaliser_saved, tst_wait_m);
            update_equaliser_view();
            show_nav();
        } else {
            play_sound(snd_error);
            toast_message(lang.muxmedia.equaliser_save_failed, tst_wait_m);
        }
        display_composite_frame();
        return 0;
    }
    return 1;
}

int video_playback_ui_name_press(char *name, const size_t name_size) {
    if (!naming_active || !key_entry || !lv_obj_is_valid(key_entry)) return 0;
    play_sound(snd_keypress);
    const char *key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (key && strcasecmp(key, OSK_DONE) == 0) return video_playback_ui_name_finish(name, name_size);
    lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    return 0;
}

void video_playback_ui_name_cancel(void) {
    naming_equaliser = 0;
    close_name_entry(1);
}

void video_playback_ui_name_backspace(void) {
    if (naming_active) key_backspace(name_entry);
}

void video_playback_ui_name_clear(void) {
    if (naming_active) key_clear(name_entry);
}

void video_playback_ui_name_space(void) {
    if (naming_active) key_space(name_entry);
}

void video_playback_ui_name_move(const int vertical, const int direction) {
    if (!naming_active) return;
    if (vertical) {
        if (direction < 0)
            key_up();
        else
            key_down();
    } else if (direction < 0) {
        key_left();
    } else {
        key_right();
    }
}

void video_playback_ui_name_layer(const int direction) {
    if (!naming_active) return;
    if (direction < 0)
        key_swap_back();
    else
        key_swap();
}

void video_playback_ui_bookmarks_refresh(void) {
    if (!video_playback_ui_bookmarks_active()) return;
    build_bookmarks();
    display_composite_frame();
}

void video_playback_ui_delete_bookmark(void) {
    if (!video_playback_ui_bookmarks_active() || !bookmark_count) return;
    const video_state_entry *entry = &bookmark_entries[bookmark_indices[current_item_index]];
    if (video_bookmark_remove(entry->uri, entry->position) < 0) {
        play_sound(snd_error);
        return;
    }
    build_bookmarks();
    display_composite_frame();
}

void video_playback_ui_request_bookmark_delete(void) {
    if (!video_playback_ui_bookmarks_active() || !bookmark_count) return;
    dialogue_open(&bookmark_delete_dialogue, &theme);
    display_composite_frame();
}

double video_playback_ui_selected_position(void) {
    return selected_bookmark_position;
}

size_t video_playback_ui_selected_playlist(void) {
    return selected_playlist_index;
}
