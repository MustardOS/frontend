#include "ui_progress.h"

#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <module/muxshare.h>

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

static void progress_part_add(
    wasabi_progress *progress, lv_obj_t *parent, const int x, const int y, const int width, const int height,
    const int radius
) {
    if (progress->part_count >= WASABI_PROGRESS_PART_CAPACITY) return;
    lv_obj_t *part = progress_object(parent, 0);
    lv_obj_set_pos(part, x, y);
    lv_obj_set_size(part, width, height);
    lv_obj_set_style_radius(part, radius, MU_OBJ_MAIN_DEFAULT);
    progress->parts[progress->part_count++] = part;
}

void wasabi_progress_init(wasabi_progress *progress, lv_obj_t *parent, const int width) {
    static const signed char curve[] = {1, 0, -2, -4, -6, -8, -9, -9, -8, -6, -3, 0,  3,  6,  8, 9,
                                        9, 8, 6,  3,  0,  -3, -6, -8, -9, -9, -8, -6, -4, -2, 0, 1};
    if (!progress || !parent || width < 1) return;
    memset(progress, 0, sizeof(*progress));
    progress->width = width;
    progress->shown = -1;
    progress->shown_step = -1;
    progress->style = config.video.progress_bar;

    const progress_style style = (progress_style) progress->style;
    const int base_height = theme.bar.progress_height > 20  ? 20
                            : theme.bar.progress_height > 5 ? theme.bar.progress_height
                                                            : 7;

    if (style == progress_classic) {
        progress->bar = lv_bar_create(parent);
        lv_obj_set_pos(progress->bar, 0, (28 - base_height) / 2);
        lv_obj_set_size(progress->bar, width, base_height);
        lv_bar_set_range(progress->bar, 0, 1000);
        lv_bar_set_value(progress->bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(progress->bar, lv_color_hex(theme.bar.progress_main_background), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(progress->bar, theme.bar.progress_main_background_alpha, LV_PART_MAIN);
        lv_obj_set_style_radius(progress->bar, theme.bar.progress_radius, LV_PART_MAIN);
        lv_obj_set_style_bg_color(progress->bar, lv_color_hex(theme.bar.progress_active_background), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(progress->bar, theme.bar.progress_active_background_alpha, LV_PART_INDICATOR);
        lv_obj_set_style_radius(progress->bar, theme.bar.progress_radius, LV_PART_INDICATOR);
        return;
    }

    if (style == progress_centre_out || style == progress_reverse) {
        progress->back = progress_object(parent, 0);
        lv_obj_set_pos(progress->back, 0, (28 - base_height) / 2);
        lv_obj_set_size(progress->back, width, base_height);
        lv_obj_set_style_radius(progress->back, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
        progress->primary = progress_object(parent, 1);
        lv_obj_set_style_radius(progress->primary, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
        if (style == progress_centre_out) {
            progress->secondary = progress_object(parent, 1);
            lv_obj_set_style_radius(progress->secondary, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
            progress->marker = progress_object(parent, 1);
            lv_obj_set_size(progress->marker, 2, base_height + 8);
            lv_obj_set_pos(progress->marker, width / 2 - 1, (28 - base_height) / 2 - 4);
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
            progress_part_add(progress, parent, index * (part_width + gap), (28 - height) / 2, part_width, height, 1);
        }
        return;
    }

    if (style == progress_segmented) {
        const int count = 10;
        const int gap = width / 100 > 3 ? width / 100 : 3;
        const int part_width = (width - gap * (count - 1)) / count;
        for (int index = 0; index < count; index++) {
            progress_part_add(progress, parent, index * (part_width + gap), 4, part_width, 20, 4);
            progress_opacity(progress->parts[index], LV_OPA_TRANSP);
        }
        return;
    }

    if (style == progress_dot_trail || style == progress_comet) {
        const int count = width / 20 < 16 ? 16 : width / 20 > 36 ? 36 : width / 20;
        const int diameter = width / count / 2 < 6 ? 6 : width / count / 2;
        const int gap = count > 1 ? (width - diameter * count) / (count - 1) : 0;
        for (int index = 0; index < count; index++)
            progress_part_add(
                progress, parent, index * (diameter + gap), (28 - diameter) / 2, diameter, diameter, LV_RADIUS_CIRCLE
            );
        if (style == progress_comet) {
            progress->marker = progress_object(parent, 1);
            lv_obj_set_size(progress->marker, diameter + 4, diameter + 4);
            lv_obj_set_style_radius(progress->marker, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
        }
        return;
    }

    if (style == progress_curve) {
        const int count = 32;
        const int diameter = 10;
        for (int index = 0; index < count; index++) {
            const int x = index * (width - diameter) / (count - 1);
            const int y = 9 + curve[index];
            progress_part_add(progress, parent, x, y, diameter, diameter, LV_RADIUS_CIRCLE);
        }
        return;
    }

    if (style == progress_position_marker) {
        progress->back = progress_object(parent, 0);
        lv_obj_set_pos(progress->back, 0, 11);
        lv_obj_set_size(progress->back, width, 6);
        lv_obj_set_style_radius(progress->back, theme.bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
        progress->marker = progress_object(parent, 1);
        lv_obj_set_style_radius(progress->marker, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
        return;
    }

    if (style == progress_pulse) {
        static const unsigned char pulse_height[] = {4, 4, 4, 7, 13, 24, 13, 7, 4, 4};
        const int count = 40;
        const int gap = 2;
        const int part_width = (width - gap * (count - 1)) / count;
        for (int index = 0; index < count; index++) {
            const int height = pulse_height[index % (int) (sizeof(pulse_height) / sizeof(pulse_height[0]))];
            progress_part_add(progress, parent, index * (part_width + gap), (28 - height) / 2, part_width, height, 1);
        }
    }
}

int wasabi_progress_update(wasabi_progress *progress, int value) {
    if (!progress) return 0;
    if (value < 0) value = 0;
    if (value > 1000) value = 1000;
    if (value == progress->shown) return 0;
    progress->shown = value;
    const progress_style style = (progress_style) progress->style;
    const int base_height = theme.bar.progress_height > 20  ? 20
                            : theme.bar.progress_height > 5 ? theme.bar.progress_height
                                                            : 7;

    if (style == progress_classic) {
        if (progress->bar) lv_bar_set_value(progress->bar, value, LV_ANIM_OFF);
        return 1;
    }
    if (style == progress_centre_out) {
        const int half = progress->width / 2;
        const int fill = half * value / 1000;
        progress_rect(progress->primary, half - fill, (28 - base_height) / 2, fill, base_height);
        progress_rect(progress->secondary, half, (28 - base_height) / 2, fill, base_height);
        return 1;
    }
    if (style == progress_segmented) {
        const int scaled = value * progress->part_count;
        const int complete = scaled / 1000;
        const int fraction = scaled % 1000;
        const int active_opacity = theme.bar.progress_active_background_alpha;
        for (int index = 0; index < progress->part_count; index++) {
            const int opacity = index < complete ? active_opacity
                                : index == complete && complete < progress->part_count
                                    ? active_opacity * fraction / 1000
                                    : LV_OPA_TRANSP;
            progress_opacity(progress->parts[index], opacity);
        }
        return 1;
    }
    if (style == progress_reverse) {
        const int fill = progress->width * value / 1000;
        progress_rect(progress->primary, progress->width - fill, (28 - base_height) / 2, fill, base_height);
        return 1;
    }
    if (style == progress_position_marker) {
        progress_rect(progress->marker, (progress->width - 16) * value / 1000, 6, 16, 16);
        return 1;
    }
    const int step = value * progress->part_count / 1000;
    if (style == progress_comet) {
        const int marker_size = 10;
        progress_rect(
            progress->marker, (progress->width - marker_size) * value / 1000, (28 - marker_size) / 2, marker_size,
            marker_size
        );
        if (step == progress->shown_step) return 1;
        progress->shown_step = step;
        const int active_opacity = theme.bar.progress_active_background_alpha;
        const int head = step >= progress->part_count ? progress->part_count - 1 : step;
        for (int index = 0; index < progress->part_count; index++) {
            const int distance = head - index;
            const int opacity = distance < 0   ? LV_OPA_TRANSP
                                : distance > 6 ? active_opacity / 4
                                               : active_opacity * (7 - distance) / 7;
            progress_opacity(progress->parts[index], opacity);
        }
        return 1;
    }
    if (step == progress->shown_step) return 0;
    progress->shown_step = step;
    for (int index = 0; index < progress->part_count; index++) {
        const int active = index < step || (value >= 1000 && index == progress->part_count - 1);
        progress_active(progress->parts[index], active);
        lv_obj_set_style_border_width(
            progress->parts[index],
            (style == progress_dot_trail || style == progress_curve) && index == step && step < progress->part_count
                ? style == progress_curve ? 3 : 2
                : 0,
            MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_border_color(
            progress->parts[index], lv_color_hex(theme.bar.progress_active_background), MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_border_opa(
            progress->parts[index], theme.bar.progress_active_background_alpha, MU_OBJ_MAIN_DEFAULT
        );
    }
    return 1;
}

void wasabi_progress_reset(wasabi_progress *progress) {
    if (progress) memset(progress, 0, sizeof(*progress));
}
