#include "ui_audio.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <module/muxshare.h>
#include <common/ui/glyph.h>
#include <common/ui/image.h>

static lv_obj_t *audio_panel;
static lv_obj_t *audio_progress;
static lv_obj_t *audio_progress_back;
static lv_obj_t *audio_progress_primary;
static lv_obj_t *audio_progress_secondary;
static lv_obj_t *audio_progress_marker;
#define AUDIO_PROGRESS_PART_CAPACITY 48
static lv_obj_t *audio_progress_parts[AUDIO_PROGRESS_PART_CAPACITY];
static int audio_progress_part_count;
static int audio_progress_width;
static lv_obj_t *audio_elapsed;
static lv_obj_t *audio_remaining;
static lv_obj_t *audio_modes;
static lv_obj_t *audio_shuffle;
static lv_obj_t *audio_repeat;
static lv_obj_t *audio_repeat_badge;
static int shown_second = -1;
static int shown_progress = -1;
static int shown_progress_step = -1;

typedef enum {
    progress_classic = 0,
    progress_centre_out,
    progress_waveform,
    progress_segmented,
    progress_reverse,
    progress_dot_trail,
    progress_curve,
    progress_position_marker,
    progress_comet,
    progress_pulse
} progress_style;

static lv_obj_t *progress_object(lv_obj_t *parent, const int active) {
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_color(
        object, lv_color_hex(active ? theme.bar.progress_active_background : theme.bar.progress_main_background),
        MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_opa(
        object, active ? theme.bar.progress_active_background_alpha : theme.bar.progress_main_background_alpha,
        MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return object;
}

static void progress_active(lv_obj_t *object, const int active) {
    if (!object) return;
    lv_obj_set_style_bg_color(
        object, lv_color_hex(active ? theme.bar.progress_active_background : theme.bar.progress_main_background),
        MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_opa(
        object, active ? theme.bar.progress_active_background_alpha : theme.bar.progress_main_background_alpha,
        MU_OBJ_MAIN_DEFAULT
    );
}

static void progress_opacity(lv_obj_t *object, const int opacity) {
    if (!object) return;
    lv_obj_set_style_bg_color(object, lv_color_hex(theme.bar.progress_active_background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(object, (lv_opa_t) opacity, MU_OBJ_MAIN_DEFAULT);
}

static void progress_rect(lv_obj_t *object, const int x, const int y, const int width, const int height) {
    if (!object) return;
    if (width < 1 || height < 1) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
}

static void
progress_part_add(lv_obj_t *parent, const int x, const int y, const int width, const int height, const int radius) {
    if (audio_progress_part_count >= AUDIO_PROGRESS_PART_CAPACITY) return;
    lv_obj_t *part = progress_object(parent, 0);
    lv_obj_set_pos(part, x, y);
    lv_obj_set_size(part, width, height);
    lv_obj_set_style_radius(part, radius, MU_OBJ_MAIN_DEFAULT);
    audio_progress_parts[audio_progress_part_count++] = part;
}

static void progress_build(lv_obj_t *parent, const int width) {
    static const signed char curve[] = {1, 0, -2, -4, -6, -8, -9, -9, -8, -6, -3, 0,  3,  6,  8, 9,
                                        9, 8, 6,  3,  0,  -3, -6, -8, -9, -9, -8, -6, -4, -2, 0, 1};
    const progress_style style = (progress_style) config.video.progress_bar;
    const int base_height = theme.bar.progress_height > 20  ? 20
                            : theme.bar.progress_height > 5 ? theme.bar.progress_height
                                                            : 7;
    audio_progress_width = width;
    audio_progress_part_count = 0;
    shown_progress_step = -1;

    if (style == progress_classic) {
        audio_progress = lv_bar_create(parent);
        lv_obj_set_pos(audio_progress, 0, (28 - base_height) / 2);
        lv_obj_set_size(audio_progress, width, base_height);
        lv_bar_set_range(audio_progress, 0, 1000);
        lv_bar_set_value(audio_progress, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(audio_progress, lv_color_hex(theme.bar.progress_main_background), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(audio_progress, theme.bar.progress_main_background_alpha, LV_PART_MAIN);
        lv_obj_set_style_radius(audio_progress, theme.bar.progress_radius, LV_PART_MAIN);
        lv_obj_set_style_bg_color(
            audio_progress, lv_color_hex(theme.bar.progress_active_background), LV_PART_INDICATOR
        );
        lv_obj_set_style_bg_opa(audio_progress, theme.bar.progress_active_background_alpha, LV_PART_INDICATOR);
        lv_obj_set_style_radius(audio_progress, theme.bar.progress_radius, LV_PART_INDICATOR);
        return;
    }

    if (style == progress_centre_out || style == progress_reverse) {
        audio_progress_back = progress_object(parent, 0);
        lv_obj_set_pos(audio_progress_back, 0, (28 - base_height) / 2);
        lv_obj_set_size(audio_progress_back, width, base_height);
        lv_obj_set_style_radius(audio_progress_back, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
        audio_progress_primary = progress_object(parent, 1);
        lv_obj_set_style_radius(audio_progress_primary, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
        if (style == progress_centre_out) {
            audio_progress_secondary = progress_object(parent, 1);
            lv_obj_set_style_radius(audio_progress_secondary, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
            audio_progress_marker = progress_object(parent, 1);
            lv_obj_set_size(audio_progress_marker, 2, base_height + 8);
            lv_obj_set_pos(audio_progress_marker, width / 2 - 1, (28 - base_height) / 2 - 4);
        }
        return;
    }

    if (style == progress_waveform) {
        const int count = width / 13 < 24 ? 24 : width / 13 > 48 ? 48 : width / 13;
        const int gap = 2;
        const int part_width = (width - gap * (count - 1)) / count;
        uint32_t random = (uint32_t) SDL_GetPerformanceCounter() ^ (uint32_t) (uintptr_t) parent ^ (uint32_t) width;
        if (!random) random = 0x9e3779b9U;
        for (int index = 0; index < count; index++) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            const int height = 7 + (int) (random % 18U);
            progress_part_add(parent, index * (part_width + gap), (28 - height) / 2, part_width, height, 1);
        }
        return;
    }

    if (style == progress_segmented) {
        const int count = 10;
        const int gap = width / 100 > 3 ? width / 100 : 3;
        const int part_width = (width - gap * (count - 1)) / count;
        for (int index = 0; index < count; index++) {
            progress_part_add(parent, index * (part_width + gap), 4, part_width, 20, 4);
            progress_opacity(audio_progress_parts[index], LV_OPA_TRANSP);
        }
        return;
    }

    if (style == progress_dot_trail || style == progress_comet) {
        const int count = width / 20 < 16 ? 16 : width / 20 > 36 ? 36 : width / 20;
        const int diameter = width / count / 2 < 6 ? 6 : width / count / 2;
        const int gap = count > 1 ? (width - diameter * count) / (count - 1) : 0;
        for (int index = 0; index < count; index++)
            progress_part_add(
                parent, index * (diameter + gap), (28 - diameter) / 2, diameter, diameter, LV_RADIUS_CIRCLE
            );
        if (style == progress_comet) {
            audio_progress_marker = progress_object(parent, 1);
            lv_obj_set_size(audio_progress_marker, diameter + 4, diameter + 4);
            lv_obj_set_style_radius(audio_progress_marker, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
        }
        return;
    }

    if (style == progress_curve) {
        const int count = 32;
        const int diameter = 10;
        for (int index = 0; index < count; index++) {
            const int x = index * (width - diameter) / (count - 1);
            const int y = 9 + curve[index];
            progress_part_add(parent, x, y, diameter, diameter, LV_RADIUS_CIRCLE);
        }
        return;
    }

    if (style == progress_position_marker) {
        audio_progress_back = progress_object(parent, 0);
        lv_obj_set_pos(audio_progress_back, 0, 11);
        lv_obj_set_size(audio_progress_back, width, 6);
        lv_obj_set_style_radius(audio_progress_back, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
        audio_progress_marker = progress_object(parent, 1);
        lv_obj_set_style_radius(audio_progress_marker, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
        return;
    }

    if (style == progress_pulse) {
        static const unsigned char pulse_height[] = {4, 4, 4, 7, 13, 24, 13, 7, 4, 4};
        const int count = 40;
        const int gap = 2;
        const int part_width = (width - gap * (count - 1)) / count;
        for (int index = 0; index < count; index++) {
            const int height = pulse_height[index % (int) (sizeof(pulse_height) / sizeof(pulse_height[0]))];
            progress_part_add(parent, index * (part_width + gap), (28 - height) / 2, part_width, height, 1);
        }
    }
}

static int progress_update(int value) {
    if (value < 0) value = 0;
    if (value > 1000) value = 1000;
    if (value == shown_progress) return 0;
    shown_progress = value;
    const progress_style style = (progress_style) config.video.progress_bar;
    const int base_height = theme.bar.progress_height > 20  ? 20
                            : theme.bar.progress_height > 5 ? theme.bar.progress_height
                                                            : 7;

    if (style == progress_classic) {
        if (audio_progress) lv_bar_set_value(audio_progress, value, LV_ANIM_OFF);
        return 1;
    }
    if (style == progress_centre_out) {
        const int half = audio_progress_width / 2;
        const int fill = half * value / 1000;
        progress_rect(audio_progress_primary, half - fill, (28 - base_height) / 2, fill, base_height);
        progress_rect(audio_progress_secondary, half, (28 - base_height) / 2, fill, base_height);
        return 1;
    }
    if (style == progress_segmented) {
        const int scaled = value * audio_progress_part_count;
        const int complete = scaled / 1000;
        const int fraction = scaled % 1000;
        const int active_opacity = theme.bar.progress_active_background_alpha;
        for (int index = 0; index < audio_progress_part_count; index++) {
            const int opacity = index < complete ? active_opacity
                                : index == complete && complete < audio_progress_part_count
                                    ? active_opacity * fraction / 1000
                                    : LV_OPA_TRANSP;
            progress_opacity(audio_progress_parts[index], opacity);
        }
        return 1;
    }
    if (style == progress_reverse) {
        const int fill = audio_progress_width * value / 1000;
        progress_rect(audio_progress_primary, audio_progress_width - fill, (28 - base_height) / 2, fill, base_height);
        return 1;
    }
    if (style == progress_position_marker) {
        progress_rect(audio_progress_marker, (audio_progress_width - 16) * value / 1000, 6, 16, 16);
        return 1;
    }
    const int step = value * audio_progress_part_count / 1000;
    if (style == progress_comet) {
        const int marker_size = 10;
        progress_rect(
            audio_progress_marker, (audio_progress_width - marker_size) * value / 1000, (28 - marker_size) / 2,
            marker_size, marker_size
        );
        if (step == shown_progress_step) return 1;
        shown_progress_step = step;
        const int active_opacity = theme.bar.progress_active_background_alpha;
        const int head = step >= audio_progress_part_count ? audio_progress_part_count - 1 : step;
        for (int index = 0; index < audio_progress_part_count; index++) {
            const int distance = head - index;
            const int opacity = distance < 0   ? LV_OPA_TRANSP
                                : distance > 6 ? active_opacity / 4
                                               : active_opacity * (7 - distance) / 7;
            progress_opacity(audio_progress_parts[index], opacity);
        }
        return 1;
    }
    if (step == shown_progress_step) return 0;
    shown_progress_step = step;
    for (int index = 0; index < audio_progress_part_count; index++) {
        const int active = index < step || (value >= 1000 && index == audio_progress_part_count - 1);
        progress_active(audio_progress_parts[index], active);
        lv_obj_set_style_border_width(
            audio_progress_parts[index],
            (style == progress_dot_trail || style == progress_curve) && index == step
                    && step < audio_progress_part_count
                ? style == progress_curve ? 3 : 2
                : 0,
            MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_border_color(
            audio_progress_parts[index], lv_color_hex(theme.bar.progress_active_background), MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_border_opa(
            audio_progress_parts[index], theme.bar.progress_active_background_alpha, MU_OBJ_MAIN_DEFAULT
        );
    }
    return 1;
}

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
        lv_obj_set_style_bg_opa(art_panel, theme.list_default.background_alpha, MU_OBJ_MAIN_DEFAULT);
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

    progress_build(progress_panel, device.mux.width - padding * 2);

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
    shown_progress = -1;
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
    int changed = progress_update(progress);
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
    audio_progress = NULL;
    audio_progress_back = NULL;
    audio_progress_primary = NULL;
    audio_progress_secondary = NULL;
    audio_progress_marker = NULL;
    memset(audio_progress_parts, 0, sizeof(audio_progress_parts));
    audio_progress_part_count = 0;
    audio_progress_width = 0;
    audio_elapsed = NULL;
    audio_remaining = NULL;
    audio_modes = NULL;
    audio_shuffle = NULL;
    audio_repeat = NULL;
    audio_repeat_badge = NULL;
    shown_second = -1;
    shown_progress = -1;
    shown_progress_step = -1;
}
