#include <stdlib.h>
#include <common/ui/font.h>
#include <common/ui/rich_text.h>

static size_t markdown_delimiter(const char *text) {
    size_t length = 0;
    while (length < 3 && text[length] == '*')
        length++;
    return length;
}

static int markdown_has_closing(const char *text, const size_t delimiter) {
    for (const char *cursor = text; *cursor; cursor++) {
        if (*cursor == '\\' && cursor[1]) {
            cursor++;
            continue;
        }
        if (markdown_delimiter(cursor) >= delimiter) return 1;
    }
    return 0;
}

static void rich_text_add_span(
    lv_obj_t *group, const char *text, const size_t length, const lv_font_t *base_font, const int bold, const int italic
) {
    if (!length) return;

    char *copy = malloc(length + 1);
    if (!copy) return;

    size_t output = 0;
    for (size_t input = 0; input < length; input++) {
        if (text[input] == '\\' && input + 1 < length && (text[input + 1] == '*' || text[input + 1] == '\\')) input++;
        copy[output++] = text[input];
    }
    copy[output] = '\0';

    lv_span_t *span = lv_spangroup_new_span(group);
    if (span) {
        lv_span_set_text(span, copy);
        const lv_font_t *font = font_style_variant(base_font, bold, italic);
        if (font) lv_style_set_text_font(&span->style, font);
    }
    free(copy);
}

void rich_text_set(lv_obj_t *group, const char *markup, const lv_font_t *base_font) {
    if (!group || !lv_obj_check_type(group, &lv_spangroup_class)) return;
    if (!markup) markup = "";
    if (!base_font) base_font = lv_obj_get_style_text_font(group, LV_PART_MAIN);

    lv_span_t *span;
    while ((span = lv_spangroup_get_child(group, 0)) != NULL)
        lv_spangroup_del_span(group, span);

    int bold = 0;
    int italic = 0;
    const char *run = markup;
    const char *cursor = markup;
    while (*cursor) {
        if (*cursor == '\\' && cursor[1]) {
            cursor += 2;
            continue;
        }

        const size_t delimiter = markdown_delimiter(cursor);
        if (!delimiter) {
            cursor++;
            continue;
        }

        const int closing = delimiter == 3 ? bold && italic : delimiter == 2 ? bold : italic;
        if (!closing && !markdown_has_closing(cursor + delimiter, delimiter)) {
            cursor += delimiter;
            continue;
        }

        rich_text_add_span(group, run, (size_t) (cursor - run), base_font, bold > 0, italic > 0);
        if (delimiter == 3) {
            bold = !bold;
            italic = !italic;
        } else if (delimiter == 2) {
            bold = !bold;
        } else {
            italic = !italic;
        }
        cursor += delimiter;
        run = cursor;
    }
    rich_text_add_span(group, run, (size_t) (cursor - run), base_font, bold > 0, italic > 0);

    lv_obj_invalidate(group);
}

lv_obj_t *rich_text_create(lv_obj_t *parent, const char *markup, const lv_font_t *base_font) {
    lv_obj_t *group = lv_spangroup_create(parent);
    if (!group) return NULL;
    lv_spangroup_set_mode(group, LV_SPAN_MODE_BREAK);
    rich_text_set(group, markup, base_font);
    return group;
}
