#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <common/ui/font_info.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ot.h>
#include <ft2build.h>
#include FT_FREETYPE_H

static hb_face_t *open_face(const char *path, const unsigned int face_index, hb_blob_t **blob_out) {
    hb_blob_t *blob = hb_blob_create_from_file_or_fail(path);
    if (!blob || hb_blob_get_length(blob) == 0) {
        if (blob) hb_blob_destroy(blob);
        return NULL;
    }

    const unsigned int face_count = hb_face_count(blob);
    if (face_index >= face_count) {
        hb_blob_destroy(blob);
        return NULL;
    }

    hb_face_t *face = hb_face_create(blob, face_index);
    if (!face || hb_face_get_glyph_count(face) == 0) {
        if (face) hb_face_destroy(face);
        hb_blob_destroy(blob);
        return NULL;
    }

    *blob_out = blob;
    return face;
}

static int read_name(hb_face_t *face, const hb_ot_name_id_t id, char *out, const size_t out_size) {
    if (!face || !out || out_size == 0) return 0;

    unsigned int entry_count = 0;
    const hb_ot_name_entry_t *entries = hb_ot_name_list_names(face, &entry_count);
    hb_language_t chosen = HB_LANGUAGE_INVALID;

    for (unsigned int i = 0; i < entry_count; i++) {
        if (entries[i].name_id != id) continue;
        if (chosen == HB_LANGUAGE_INVALID) chosen = entries[i].language;

        const char *language = hb_language_to_string(entries[i].language);
        if (language && strncasecmp(language, "en", 2) == 0) {
            chosen = entries[i].language;
            break;
        }
    }

    if (chosen == HB_LANGUAGE_INVALID) return 0;

    unsigned int length = out_size > 0 && out_size - 1 < UINT_MAX ? (unsigned int) out_size - 1 : UINT_MAX;
    if (!hb_ot_name_get_utf8(face, id, chosen, &length, out) || !length) return 0;

    out[length < out_size ? length : out_size - 1] = '\0';
    return 1;
}

static int regular_style(const char *style) {
    return !style[0] || strcasecmp(style, "Regular") == 0 || strcasecmp(style, "Normal") == 0
           || strcasecmp(style, "Roman") == 0 || strcasecmp(style, "Book") == 0;
}

int font_info_read(const char *path, const unsigned int face_index, font_info_t *info) {
    if (!path || !*path || !info) return 0;
    memset(info, 0, sizeof(*info));

    hb_blob_t *blob = NULL;
    hb_face_t *face = open_face(path, face_index, &blob);
    if (!face) return 0;

    info->face_count = hb_face_count(blob);
    if (!read_name(face, HB_OT_NAME_ID_TYPOGRAPHIC_FAMILY, info->family, sizeof(info->family)))
        read_name(face, HB_OT_NAME_ID_FONT_FAMILY, info->family, sizeof(info->family));
    if (!read_name(face, HB_OT_NAME_ID_TYPOGRAPHIC_SUBFAMILY, info->style, sizeof(info->style)))
        read_name(face, HB_OT_NAME_ID_FONT_SUBFAMILY, info->style, sizeof(info->style));

    if (info->family[0]) {
        if (regular_style(info->style))
            snprintf(info->display, sizeof(info->display), "%s", info->family);
        else {
            const size_t family_length = strnlen(info->family, sizeof(info->display) - 1);
            memcpy(info->display, info->family, family_length);
            size_t used = family_length;
            if (used + 3 < sizeof(info->display)) {
                memcpy(info->display + used, " - ", 3);
                used += 3;
                const size_t style_length = strnlen(info->style, sizeof(info->display) - used - 1);
                memcpy(info->display + used, info->style, style_length);
                used += style_length;
            }
            info->display[used] = '\0';
        }
    } else {
        read_name(face, HB_OT_NAME_ID_FULL_NAME, info->display, sizeof(info->display));
    }

    hb_face_destroy(face);
    hb_blob_destroy(blob);

    return info->display[0] != '\0';
}

int font_info_axis(const char *path, const unsigned int face_index, const uint32_t tag, font_axis_info_t *axis) {
    if (!path || !*path || !axis) return 0;
    memset(axis, 0, sizeof(*axis));

    hb_blob_t *blob = NULL;
    hb_face_t *face = open_face(path, face_index, &blob);
    if (!face) return 0;

    hb_ot_var_axis_info_t info;
    const int found = hb_ot_var_find_axis_info(face, tag, &info) && (info.flags & HB_OT_VAR_AXIS_FLAG_HIDDEN) == 0;
    if (found) {
        axis->tag = info.tag;
        axis->minimum = info.min_value;
        axis->default_value = info.default_value;
        axis->maximum = info.max_value;
        read_name(face, info.name_id, axis->name, sizeof(axis->name));
    }

    hb_face_destroy(face);
    hb_blob_destroy(blob);

    return found;
}

static int font_size_compare(const void *left, const void *right) {
    const int a = *(const int *) left;
    const int b = *(const int *) right;

    return (a > b) - (a < b);
}

int font_info_fixed_sizes(const char *path, const unsigned int face_index, int *sizes, const size_t capacity) {
    if (!path || !*path || !sizes || capacity == 0) return 0;

    FT_Library library = NULL;
    FT_Face face = NULL;
    if (FT_Init_FreeType(&library) != 0) return 0;
    if (FT_New_Face(library, path, face_index, &face) != 0) {
        FT_Done_FreeType(library);
        return 0;
    }

    size_t count = 0;
    for (FT_Int i = 0; i < face->num_fixed_sizes && count < capacity; i++) {
        int size = (int) ((face->available_sizes[i].y_ppem + 32) >> 6);
        if (size <= 0) size = face->available_sizes[i].height;
        if (size <= 0) continue;

        int duplicate = 0;
        for (size_t existing = 0; existing < count; existing++)
            if (sizes[existing] == size) duplicate = 1;
        if (!duplicate) sizes[count++] = size;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    qsort(sizes, count, sizeof(*sizes), font_size_compare);

    return count <= INT_MAX ? (int) count : INT_MAX;
}
