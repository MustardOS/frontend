#include "ui_audio.h"
#include "ui_progress.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <module/muxshare.h>
#include <common/ui/glyph.h>
#include <common/ui/image.h>

static lv_obj_t *audio_panel;
static wasabi_progress audio_progress;
static lv_obj_t *audio_elapsed;
static lv_obj_t *audio_remaining;
static lv_obj_t *audio_modes;
static lv_obj_t *audio_shuffle;
static lv_obj_t *audio_repeat;
static lv_obj_t *audio_repeat_badge;
static int shown_second = -1;
static void mode_glyph(lv_obj_t *image, const char *name) {
    char path[MAX_BUFFER_SIZE];
    if (!get_glyph_path(mux_module, name, path, sizeof(path))) {
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    set_footer_glyph_image(image, path);
    lv_obj_set_style_img_opa(image, (lv_opa_t) theme.list_default.glyph_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(image, lv_color_hex(theme.list_default.glyph_recolour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(image, (lv_opa_t) theme.list_default.glyph_recolour_alpha, MU_OBJ_MAIN_DEFAULT);
}

static lv_obj_t *audio_label(lv_obj_t *parent, const char *text, const int primary, const int centred) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(label, text && text[0] ? text : "");
    lv_obj_set_style_text_align(label, centred ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(
        label, lv_color_hex(primary ? theme.list_focus.background : theme.list_default.text), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_text_opa(
        label, primary ? theme.list_focus.background_alpha : theme.list_default.text_alpha, MU_OBJ_MAIN_DEFAULT
    );
    return label;
}

static void compact_message(const char *source, char *output, const size_t size) {
    size_t written = 0;
    int spacing = 0;
    for (const unsigned char *cursor = (const unsigned char *) source; cursor && *cursor && written + 1 < size;
         cursor++) {
        if (*cursor == '\r' || *cursor == '\n' || *cursor == '\t' || *cursor == ' ') {
            spacing = written > 0;
            continue;
        }
        if (spacing && written + 1 < size) output[written++] = ' ';
        output[written++] = (char) *cursor;
        spacing = 0;
    }
    output[written] = '\0';
}

static void audio_time(const double seconds, char *buffer, const size_t size) {
    const int value = seconds > 0.0 ? (int) seconds : 0;
    if (value >= 3600)
        snprintf(buffer, size, "%d:%02d:%02d", value / 3600, value / 60 % 60, value % 60);
    else
        snprintf(buffer, size, "%02d:%02d", value / 60, value % 60);
}

int wasabi_audio_ui_init(const wasabi_audio_info *information) {
    if (!information || !ui_screen) return 0;
    wasabi_audio_ui_shutdown();

    audio_panel = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(audio_panel);
    lv_obj_set_size(audio_panel, device.mux.width, device.mux.height);
    lv_obj_center(audio_panel);
    lv_obj_clear_flag(audio_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(audio_panel);
    load_font_section(FONT_PANEL_DIR, audio_panel);

    const int padding = device.mux.width / 32 > 8 ? device.mux.width / 32 : 8;
    const int progress_height = 58;
    const int progress_y = device.mux.height - theme.footer.height - progress_height - 14;
    const int media_top = theme.header.height + 12;
    const int media_height = progress_y - media_top - 14;
    const int art_size = media_height < device.mux.width * 2 / 5 ? media_height : device.mux.width * 2 / 5;
    const int artwork_enabled = config.video.artwork_position != 2 && art_size > 0;
    const int artwork_left = config.video.artwork_position == 1;
    const int details_x = !artwork_enabled ? padding * 2 : artwork_left ? art_size + padding * 2 : padding;
    const int details_width =
        !artwork_enabled ? device.mux.width - padding * 4 : device.mux.width - art_size - padding * 3;
    const int content_y = media_top + (media_height - art_size) / 2;

    lv_obj_t *details = lv_obj_create(audio_panel);
    lv_obj_remove_style_all(details);
    lv_obj_set_pos(details, details_x, content_y);
    lv_obj_set_size(details, details_width, art_size);
    lv_obj_set_flex_flow(details, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        details, LV_FLEX_ALIGN_CENTER, artwork_enabled ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_row(details, 6, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(details, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    audio_label(details, information->title, 1, !artwork_enabled);
    audio_label(
        details, information->artist[0] ? information->artist : lang.muxmedia.unknown_artist, 0, !artwork_enabled
    );
    if (information->album[0]) audio_label(details, information->album, 0, !artwork_enabled);
    if (information->year[0]) audio_label(details, information->year, 0, !artwork_enabled);
    if (information->genre[0]) audio_label(details, information->genre, 0, !artwork_enabled);
    if (information->comment[0]) {
        char message[sizeof(information->comment)];
        compact_message(information->comment, message, sizeof(message));
        if (message[0]) audio_label(details, message, 0, !artwork_enabled);
    }

    if (artwork_enabled) {
        lv_obj_t *art_panel = lv_obj_create(audio_panel);
        lv_obj_set_pos(art_panel, artwork_left ? padding : device.mux.width - art_size - padding, content_y);
        lv_obj_set_size(art_panel, art_size, art_size);
        lv_obj_set_style_radius(art_panel, theme.list_default.radius, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(art_panel, lv_color_hex(theme.list_default.background), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(art_panel, 160, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(art_panel, 2, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(art_panel, lv_color_hex(theme.list_focus.background), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(
            art_panel, theme.list_focus.background_alpha > LV_OPA_50 ? theme.list_focus.background_alpha : LV_OPA_50,
            MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_clear_flag(art_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        if (information->artwork[0]) {
            lv_obj_t *art = lv_img_create(art_panel);
            const struct image_settings settings = {
                .image_path = information->artwork,
                .align = LV_ALIGN_CENTER,
                .max_width = (int16_t) (art_size - 8),
                .max_height = (int16_t) (art_size - 8),
            };
            update_image(art, settings);
        } else {
            char glyph[MAX_BUFFER_SIZE];
            if (get_glyph_path(mux_module, "audio", glyph, sizeof(glyph))) {
                lv_obj_t *placeholder = lv_img_create(art_panel);
                append_glyph_size_hint(glyph, sizeof(glyph), art_size / 3);
                lv_img_set_src(placeholder, glyph);
                apply_glyph_scale(placeholder, glyph, art_size / 3, art_size / 3);
                lv_obj_set_style_img_opa(placeholder, (lv_opa_t) theme.list_default.glyph_alpha, MU_OBJ_MAIN_DEFAULT);
                lv_obj_set_style_img_recolor(
                    placeholder, lv_color_hex(theme.list_default.glyph_recolour), MU_OBJ_MAIN_DEFAULT
                );
                lv_obj_set_style_img_recolor_opa(
                    placeholder, (lv_opa_t) theme.list_default.glyph_recolour_alpha, MU_OBJ_MAIN_DEFAULT
                );
                lv_obj_center(placeholder);
            }
        }
    }

    lv_obj_t *progress_panel = lv_obj_create(audio_panel);
    lv_obj_remove_style_all(progress_panel);
    lv_obj_set_pos(progress_panel, padding, progress_y);
    lv_obj_set_size(progress_panel, device.mux.width - padding * 2, progress_height);
    lv_obj_clear_flag(progress_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    wasabi_progress_init(&audio_progress, progress_panel, device.mux.width - padding * 2);

    audio_elapsed = lv_label_create(progress_panel);
    audio_remaining = lv_label_create(progress_panel);
    const int time_y = 34;
    lv_obj_align(audio_elapsed, LV_ALIGN_TOP_LEFT, 0, time_y);
    lv_obj_align(audio_remaining, LV_ALIGN_TOP_RIGHT, 0, time_y);
    lv_obj_set_style_text_color(audio_elapsed, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(audio_remaining, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(audio_elapsed, "00:00");
    lv_label_set_text(audio_remaining, "00:00");

    audio_modes = lv_obj_create(progress_panel);
    lv_obj_remove_style_all(audio_modes);
    lv_obj_set_size(audio_modes, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(audio_modes, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(audio_modes, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(audio_modes, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(audio_modes, LV_ALIGN_TOP_MID, 0, time_y - 4);
    lv_obj_clear_flag(audio_modes, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    audio_shuffle = lv_img_create(audio_modes);
    audio_repeat = lv_img_create(audio_modes);
    audio_repeat_badge = lv_label_create(audio_modes);
    lv_obj_set_style_text_color(audio_repeat_badge, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(audio_repeat_badge, theme.list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    wasabi_audio_ui_modes_changed();
    shown_second = -1;
    return 1;
}

void wasabi_audio_ui_modes_changed(void) {
    if (!audio_modes || !lv_obj_is_valid(audio_modes)) return;
    if (config.video.shuffle) {
        mode_glyph(audio_shuffle, "shuffle");
        lv_obj_clear_flag(audio_shuffle, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(audio_shuffle, LV_OBJ_FLAG_HIDDEN);
    }
    if (config.video.repeat_mode) {
        mode_glyph(audio_repeat, "repeat");
        lv_obj_clear_flag(audio_repeat, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(audio_repeat_badge, config.video.repeat_mode == 1 ? "1" : "A");
        lv_obj_clear_flag(audio_repeat_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(audio_repeat, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(audio_repeat_badge, LV_OBJ_FLAG_HIDDEN);
    }
    if (config.video.shuffle || config.video.repeat_mode)
        lv_obj_clear_flag(audio_modes, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(audio_modes, LV_OBJ_FLAG_HIDDEN);
}

int wasabi_audio_ui_update(const double position, const double duration, const int paused __attribute__((unused))) {
    if (!audio_panel || !lv_obj_is_valid(audio_panel)) return 0;
    const int progress = duration > 0.0 ? (int) (position * 1000.0 / duration) : 0;
    int changed = wasabi_progress_update(&audio_progress, progress);
    const int second = position > 0.0 ? (int) position : 0;
    if (second != shown_second) {
        shown_second = second;
        char elapsed[24];
        char total[24];
        audio_time(position, elapsed, sizeof(elapsed));
        audio_time(duration, total, sizeof(total));
        lv_label_set_text(audio_elapsed, elapsed);
        lv_label_set_text(audio_remaining, total);
        changed = 1;
    }
    return changed;
}

void wasabi_audio_ui_shutdown(void) {
    if (audio_panel && lv_obj_is_valid(audio_panel)) lv_obj_del(audio_panel);
    audio_panel = NULL;
    wasabi_progress_reset(&audio_progress);
    audio_elapsed = NULL;
    audio_remaining = NULL;
    audio_modes = NULL;
    audio_shuffle = NULL;
    audio_repeat = NULL;
    audio_repeat_badge = NULL;
    shown_second = -1;
}
