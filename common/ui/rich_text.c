#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/ui/font.h>
#include <common/ui/rich_text.h>
#include <common/config/config.h>

static const uint32_t rich_text_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xFFAA00, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

typedef struct {
    int bold;
    int italic;
    int underline;
    int strike;
    int obfuscated;
    int colour;
} rich_text_state;

#define RICH_OBFUSCATE_FIRST 0x21
#define RICH_OBFUSCATE_LAST  0x7E
#define RICH_OBFUSCATE_COUNT (RICH_OBFUSCATE_LAST - RICH_OBFUSCATE_FIRST + 1)
#define RICH_OBFUSCATE_MS    66

typedef struct {
    lv_span_t *span;
    char *original;
    const lv_font_t *font;
} rich_obfuscated_span;

typedef struct {
    rich_obfuscated_span *spans;
    size_t count;
    size_t capacity;
    lv_timer_t *timer;
    const lv_font_t *width_font;
    uint16_t widths[RICH_OBFUSCATE_COUNT];
    lv_timer_t *follow;
    char *markup;
    int follow_mode;
    lv_coord_t follow_x;
} rich_obfuscated;

static void rich_obfuscated_clear(rich_obfuscated *obfuscated) {
    if (!obfuscated) return;

    if (obfuscated->timer) {
        lv_timer_del(obfuscated->timer);
        obfuscated->timer = NULL;
    }

    for (size_t i = 0; i < obfuscated->count; i++)
        free(obfuscated->spans[i].original);

    obfuscated->count = 0;
    obfuscated->width_font = NULL;
}

static void rich_obfuscated_deleted(lv_event_t *event) {
    rich_obfuscated *obfuscated = lv_event_get_user_data(event);
    rich_obfuscated_clear(obfuscated);
    if (obfuscated->follow) lv_timer_del(obfuscated->follow);
    free(obfuscated->markup);
    free(obfuscated->spans);
    free(obfuscated);
}

static rich_obfuscated *rich_obfuscated_get(lv_obj_t *group, const int create) {
    rich_obfuscated *obfuscated = lv_obj_get_user_data(group);
    if (obfuscated || !create) return obfuscated;

    obfuscated = calloc(1, sizeof(*obfuscated));
    if (!obfuscated) return NULL;

    lv_obj_set_user_data(group, obfuscated);
    lv_obj_add_event_cb(group, rich_obfuscated_deleted, LV_EVENT_DELETE, obfuscated);
    return obfuscated;
}

static void rich_obfuscated_widths(rich_obfuscated *obfuscated, const lv_font_t *font) {
    if (obfuscated->width_font == font) return;

    for (int i = 0; i < RICH_OBFUSCATE_COUNT; i++)
        obfuscated->widths[i] = lv_font_get_glyph_width(font, (uint32_t) (RICH_OBFUSCATE_FIRST + i), 0);

    obfuscated->width_font = font;
}

static char rich_obfuscated_pick(const rich_obfuscated *obfuscated, const char original) {
    const uint16_t width = obfuscated->widths[original - RICH_OBFUSCATE_FIRST];
    int matches = 0;

    for (int i = 0; i < RICH_OBFUSCATE_COUNT; i++)
        if (obfuscated->widths[i] == width) matches++;

    if (matches <= 1) return original;

    int choice = rand() % matches;
    for (int i = 0; i < RICH_OBFUSCATE_COUNT; i++) {
        if (obfuscated->widths[i] != width) continue;
        if (choice-- == 0) return (char) (RICH_OBFUSCATE_FIRST + i);
    }

    return original;
}

static void rich_obfuscated_scramble(lv_obj_t *group, rich_obfuscated *obfuscated) {
    for (size_t i = 0; i < obfuscated->count; i++) {
        rich_obfuscated_span *entry = &obfuscated->spans[i];
        char *scrambled = strdup(entry->original);
        if (!scrambled) continue;

        rich_obfuscated_widths(obfuscated, entry->font);
        for (char *cursor = scrambled; *cursor; cursor++) {
            if (*cursor >= RICH_OBFUSCATE_FIRST && *cursor <= RICH_OBFUSCATE_LAST)
                *cursor = rich_obfuscated_pick(obfuscated, *cursor);
        }

        lv_span_set_text(entry->span, scrambled);
        free(scrambled);
    }

    lv_spangroup_refr_mode(group);
    lv_obj_invalidate(group);
}

static void rich_obfuscated_tick(lv_timer_t *timer) {
    lv_obj_t *group = timer->user_data;
    if (!lv_obj_is_valid(group) || lv_obj_has_flag(group, LV_OBJ_FLAG_HIDDEN)) return;

    rich_obfuscated *obfuscated = lv_obj_get_user_data(group);
    if (obfuscated && obfuscated->count) rich_obfuscated_scramble(group, obfuscated);
}

static void rich_obfuscated_track(lv_obj_t *group, lv_span_t *span, const char *text, const lv_font_t *font) {
    rich_obfuscated *obfuscated = rich_obfuscated_get(group, 1);
    if (!obfuscated) return;

    if (obfuscated->count == obfuscated->capacity) {
        const size_t grown = obfuscated->capacity ? obfuscated->capacity * 2 : 4;
        rich_obfuscated_span *spans = realloc(obfuscated->spans, grown * sizeof(*spans));
        if (!spans) return;
        obfuscated->spans = spans;
        obfuscated->capacity = grown;
    }

    char *original = strdup(text);
    if (!original) return;

    obfuscated->spans[obfuscated->count++] = (rich_obfuscated_span) {span, original, font};
}

static int rich_text_sign(const char *text) {
    return (unsigned char) text[0] == 0xC2 && (unsigned char) text[1] == 0xA7;
}

static int rich_text_colour_index(const char code) {
    if (code >= '0' && code <= '9') return code - '0';
    if (code >= 'a' && code <= 'f') return code - 'a' + 10;
    if (code >= 'A' && code <= 'F') return code - 'A' + 10;
    return -1;
}

static int rich_text_known_code(const char code) {
    return code && (rich_text_colour_index(code) >= 0 || strchr("lLoOnNmMrRkK", code) != NULL);
}

int rich_text_has_codes(const char *text) {
    if (!text) return 0;

    for (const char *cursor = text; *cursor; cursor++)
        if (rich_text_sign(cursor)) return 1;

    return 0;
}

void rich_text_strip(const char *text, char *out, const size_t out_size) {
    if (!out || !out_size) return;

    size_t used = 0;
    for (const char *cursor = text ? text : ""; *cursor && used + 1 < out_size;) {
        if (rich_text_sign(cursor)) {
            if (rich_text_sign(cursor + 2)) {
                if (used + 2 >= out_size) break;
                out[used++] = cursor[0];
                out[used++] = cursor[1];
                cursor += 4;
                continue;
            }
            if (cursor[2] && rich_text_known_code(cursor[2])) {
                cursor += 3;
                continue;
            }
        }
        out[used++] = *cursor++;
    }
    out[used] = '\0';
}

static void rich_text_add_span(
    lv_obj_t *group, const char *text, const size_t length, size_t *carried_newlines, const lv_font_t *base_font,
    const rich_text_state *state
) {
    size_t body = length;
    size_t trailing = 0;
    while (body > 0 && text[body - 1] == '\n') {
        body--;
        trailing++;
    }

    const size_t leading = *carried_newlines;
    if (body == 0) {
        *carried_newlines = leading + trailing;
        return;
    }

    char *copy = malloc(leading + body + 1);
    if (!copy) return;

    size_t output = leading;
    memset(copy, '\n', leading);
    for (size_t input = 0; input < body; input++) {
        copy[output++] = text[input];
        if (rich_text_sign(text + input) && input + 3 < body && rich_text_sign(text + input + 2)) {
            copy[output++] = text[input + 1];
            input += 3;
        }
    }
    copy[output] = '\0';
    *carried_newlines = trailing;

    lv_span_t *span = lv_spangroup_new_span(group);
    if (span) {
        lv_span_set_text(span, copy);

        const lv_font_t *font = font_style_variant(base_font, state->bold, state->italic);
        if (font) lv_style_set_text_font(&span->style, font);
        if (state->obfuscated) rich_obfuscated_track(group, span, copy, font ? font : base_font);

        if (state->colour >= 0) lv_style_set_text_color(&span->style, lv_color_hex(rich_text_palette[state->colour]));

        lv_text_decor_t decor = LV_TEXT_DECOR_NONE;
        if (state->underline) decor |= LV_TEXT_DECOR_UNDERLINE;
        if (state->strike) decor |= LV_TEXT_DECOR_STRIKETHROUGH;
        if (decor != LV_TEXT_DECOR_NONE) lv_style_set_text_decor(&span->style, decor);
    }
    free(copy);
}

void rich_text_set(lv_obj_t *group, const char *markup, const lv_font_t *base_font) {
    if (!group || !lv_obj_check_type(group, &lv_spangroup_class)) return;
    if (!markup) markup = "";
    if (!base_font) base_font = lv_obj_get_style_text_font(group, LV_PART_MAIN);

    rich_obfuscated_clear(rich_obfuscated_get(group, 0));

    lv_span_t *span;
    while ((span = lv_spangroup_get_child(group, 0)) != NULL)
        lv_spangroup_del_span(group, span);

    rich_text_state state = {0, 0, 0, 0, 0, -1};
    size_t carried_newlines = 0;
    const char *run = markup;
    const char *cursor = markup;

    while (*cursor) {
        if (!rich_text_sign(cursor)) {
            cursor++;
            continue;
        }
        if (rich_text_sign(cursor + 2)) {
            cursor += 4;
            continue;
        }

        const char code = cursor[2];
        if (!code || !rich_text_known_code(code)) {
            cursor += 2;
            continue;
        }

        rich_text_add_span(group, run, (size_t) (cursor - run), &carried_newlines, base_font, &state);

        const int colour = rich_text_colour_index(code);
        if (colour >= 0) {
            state.colour = colour;
        } else {
            switch (code) {
                case 'l':
                case 'L':
                    state.bold = 1;
                    break;
                case 'o':
                case 'O':
                    state.italic = 1;
                    break;
                case 'n':
                case 'N':
                    state.underline = 1;
                    break;
                case 'm':
                case 'M':
                    state.strike = 1;
                    break;
                case 'k':
                case 'K':
                    state.obfuscated = 1;
                    break;
                case 'r':
                case 'R':
                    state = (rich_text_state) {0, 0, 0, 0, 0, -1};
                    break;
                default:
                    break;
            }
        }

        cursor += 3;
        run = cursor;
    }
    rich_text_add_span(group, run, (size_t) (cursor - run), &carried_newlines, base_font, &state);

    rich_obfuscated *obfuscated = rich_obfuscated_get(group, 0);
    if (obfuscated && obfuscated->count) {
        rich_obfuscated_scramble(group, obfuscated);
        if (!config.visual.reduce_motion)
            obfuscated->timer = lv_timer_create(rich_obfuscated_tick, RICH_OBFUSCATE_MS, group);
    }

    lv_obj_invalidate(group);
}

void rich_text_bold_labels(const char *plain, char *out, const size_t out_size) {
    if (!out || !out_size) return;
    out[0] = '\0';
    if (!plain) return;

    size_t used = 0;
    const char *line = plain;

    while (*line && used + 1 < out_size) {
        const char *end = strchr(line, '\n');
        const size_t length = end ? (size_t) (end - line) : strlen(line);
        const char *colon = memchr(line, ':', length);
        const size_t label = colon ? (size_t) (colon - line) : 0;
        int written;

        if (colon && label > 0 && label <= 32 && (label + 1 == length || colon[1] == ' ')) {
            const char *value = colon + 1;
            while (value < line + length && *value == ' ')
                value++;

            written = snprintf(
                out + used, out_size - used, RICH_TEXT_BOLD "%.*s:" RICH_TEXT_RESET "%s%.*s%s", (int) label, line,
                value < line + length ? " " : "", (int) (line + length - value), value, end ? "\n" : ""
            );
        } else {
            written = snprintf(out + used, out_size - used, "%.*s%s", (int) length, line, end ? "\n" : "");
        }

        if (written < 0 || (size_t) written >= out_size - used) break;
        used += (size_t) written;
        if (!end) break;
        line = end + 1;
    }
}

static lv_obj_t *rich_text_label_overlay(lv_obj_t *label) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(label); i++) {
        lv_obj_t *frame = lv_obj_get_child(label, (int32_t) i);
        if (lv_obj_get_child_cnt(frame) != 1) continue;

        lv_obj_t *child = lv_obj_get_child(frame, 0);
        if (lv_obj_check_type(child, &lv_spangroup_class)) return child;
    }

    return NULL;
}

static lv_obj_t *rich_text_overlay_label(lv_obj_t *overlay) {
    lv_obj_t *frame = lv_obj_get_parent(overlay);
    return frame ? lv_obj_get_parent(frame) : NULL;
}

static void rich_text_label_reveal(lv_obj_t *label) {
    lv_obj_remove_local_style_prop(label, LV_STYLE_TEXT_OPA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_local_style_prop(label, LV_STYLE_TEXT_OPA, LV_PART_MAIN | LV_STATE_FOCUSED);
}

static int rich_text_label_scroll_mode(const lv_obj_t *label) {
    const lv_label_long_mode_t mode = lv_label_get_long_mode(label);
    if (mode == LV_LABEL_LONG_SCROLL_CIRCULAR) return 2;
    if (mode == LV_LABEL_LONG_SCROLL) return 1;
    return 0;
}

static void rich_text_overlay_fit(lv_obj_t *overlay, const int mode) {
    lv_obj_t *frame = lv_obj_get_parent(overlay);
    lv_obj_t *label = rich_text_overlay_label(overlay);
    if (!frame || !label) return;

    lv_obj_update_layout(label);
    const lv_coord_t width = lv_obj_get_content_width(label);
    const lv_coord_t height = lv_obj_get_content_height(label);

    if (lv_obj_get_width(frame) != width || lv_obj_get_height(frame) != height) lv_obj_set_size(frame, width, height);

    if (mode == 0) {
        if (lv_obj_get_width(overlay) != width || lv_obj_get_height(overlay) != height)
            lv_obj_set_size(overlay, width, height);
    } else if (lv_obj_get_height(overlay) != height) {
        lv_obj_set_height(overlay, height);
    }
}

static void rich_text_overlay_layout(lv_obj_t *overlay, rich_obfuscated *state, const int mode) {
    lv_obj_t *label = rich_text_overlay_label(overlay);
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const char *markup = state->markup ? state->markup : "";

    if (mode == 0) {
        lv_spangroup_set_mode(overlay, LV_SPAN_MODE_FIXED);
        lv_spangroup_set_overflow(overlay, LV_SPAN_OVERFLOW_ELLIPSIS);
        lv_obj_set_x(overlay, 0);
        rich_text_set(overlay, markup, font);
    } else {
        lv_spangroup_set_mode(overlay, LV_SPAN_MODE_EXPAND);
        lv_spangroup_set_overflow(overlay, LV_SPAN_OVERFLOW_CLIP);

        if (mode == 2) {
            const size_t length = strlen(markup);
            char *twice = malloc(length * 2 + 16);
            if (twice) {
                snprintf(twice, length * 2 + 16, "%s" RICH_TEXT_RESET "   %s", markup, markup);
                rich_text_set(overlay, twice, font);
                free(twice);
            } else {
                rich_text_set(overlay, markup, font);
            }
        } else {
            rich_text_set(overlay, markup, font);
        }
    }

    state->follow_mode = mode;
    state->follow_x = 0;
    rich_text_overlay_fit(overlay, mode);
}

static void rich_text_overlay_follow(lv_timer_t *timer) {
    lv_obj_t *overlay = timer->user_data;
    if (!lv_obj_is_valid(overlay)) return;

    rich_obfuscated *state = lv_obj_get_user_data(overlay);
    lv_obj_t *label = rich_text_overlay_label(overlay);
    if (!state || !label) return;

    const int mode = rich_text_label_scroll_mode(label);
    if (mode != state->follow_mode) rich_text_overlay_layout(overlay, state, mode);
    rich_text_overlay_fit(overlay, mode);
    if (!mode) return;

    const lv_coord_t x = ((const lv_label_t *) label)->offset.x;
    if (x != state->follow_x) {
        lv_obj_set_x(overlay, x);
        state->follow_x = x;
    }
}

void rich_text_label_set(lv_obj_t *label, const char *text) {
    if (!label) return;
    if (!text) text = "";

    lv_obj_t *overlay = rich_text_label_overlay(label);

    if (!rich_text_has_codes(text)) {
        if (overlay) {
            lv_obj_del(lv_obj_get_parent(overlay));
            rich_text_label_reveal(label);
        }
        lv_label_set_text(label, text);
        return;
    }

    const size_t plain_size = strlen(text) + 1;
    char *plain = malloc(plain_size);
    if (!plain) {
        lv_label_set_text(label, text);
        return;
    }

    rich_text_strip(text, plain, plain_size);
    lv_label_set_text(label, plain);
    free(plain);

    lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);

    const int wraps = lv_label_get_long_mode(label) == LV_LABEL_LONG_WRAP;

    if (!overlay) {
        lv_obj_t *frame = lv_obj_create(label);
        if (!frame) {
            rich_text_label_reveal(label);
            return;
        }

        lv_obj_remove_style_all(frame);
        lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(frame, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_align(frame, LV_ALIGN_TOP_LEFT);
        lv_obj_set_style_text_opa(frame, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

        overlay = lv_spangroup_create(frame);
        if (!overlay) {
            lv_obj_del(frame);
            rich_text_label_reveal(label);
            return;
        }

        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_style_text_opa(overlay, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_align(overlay, LV_ALIGN_TOP_LEFT);
    }

    if (wraps) {
        lv_obj_update_layout(label);
        const lv_coord_t width = lv_obj_get_content_width(label);
        lv_obj_set_width(lv_obj_get_parent(overlay), width);
        lv_obj_set_height(lv_obj_get_parent(overlay), LV_SIZE_CONTENT);
        lv_obj_set_width(overlay, width);
        lv_spangroup_set_mode(overlay, LV_SPAN_MODE_BREAK);
        lv_obj_set_height(overlay, LV_SIZE_CONTENT);
        rich_text_set(overlay, text, lv_obj_get_style_text_font(label, LV_PART_MAIN));
        return;
    }

    rich_obfuscated *state = rich_obfuscated_get(overlay, 1);
    if (!state) {
        rich_text_set(overlay, text, lv_obj_get_style_text_font(label, LV_PART_MAIN));
        return;
    }

    free(state->markup);
    state->markup = strdup(text);
    rich_text_overlay_layout(overlay, state, rich_text_label_scroll_mode(label));

    if (!state->follow) state->follow = lv_timer_create(rich_text_overlay_follow, 33, overlay);
}

lv_obj_t *rich_text_create(lv_obj_t *parent, const char *markup, const lv_font_t *base_font) {
    lv_obj_t *group = lv_spangroup_create(parent);
    if (!group) return NULL;
    lv_spangroup_set_mode(group, LV_SPAN_MODE_BREAK);
    rich_text_set(group, markup, base_font);
    return group;
}
