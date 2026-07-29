#include <stdlib.h>
#include <string.h>
#include "tag.h"
#include "../options.h"
#include "../strutil.h"
#include "../fileio.h"
#include "../sysinfo.h"
#include "../init.h"

void load_tag_items(tag_item **tag_items, size_t *count) {
    int tag_count;

    const char *tag_path = resolve_info_path("name/tag.txt");
    if (!tag_path) return;

    char **tags = str_parse_file(tag_path, &tag_count, parse_lines);
    if (!tags) return;

    size_t cap = 0;

    for (int i = 0; i < tag_count; ++i) {
        if (strcasecmp(tags[i], "None") == 0) continue;

        if (*count >= cap) {
            const size_t new_cap = cap ? cap * 2 : 16;
            tag_item *tmp = realloc(*tag_items, new_cap * sizeof(tag_item));

            if (!tmp) break;

            *tag_items = tmp;
            cap = new_cap;
        }

        char *glyph_name_dup = strdup(tags[i]);
        char *glyph_name = str_tolower(str_remchar(str_trim(glyph_name_dup), ' '));
        free(glyph_name_dup);

        (*tag_items)[*count].name = str_capital(strdup(tags[i]));
        (*tag_items)[*count].glyph = glyph_name;

        char sorting_config_path[MAX_BUFFER_SIZE];
        snprintf(sorting_config_path, sizeof(sorting_config_path), SORTING_CONFIG_PATH "%s", glyph_name);

        (*tag_items)[*count].sort_bucket =
            file_exist(sorting_config_path) ? read_line_int_from(sorting_config_path, 1) : 0;

        (*count)++;
    }
}

int get_tag_sort_bucket(const tag_item *tag_items, const size_t count, const char *glyph) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(tag_items[i].glyph, glyph) == 0) return tag_items[i].sort_bucket;
    }

    return 0;
}

int tag_item_compare(const void *a, const void *b) {
    const tag_item *item_a = a;
    const tag_item *item_b = b;

    const char *sa = item_a->name ? item_a->name : "";
    const char *sb = item_b->name ? item_b->name : "";

    return str_compare(&sa, &sb);
}

void sort_tag_items(tag_item *tag_items, const size_t count) {
    qsort(tag_items, count, sizeof(tag_item), tag_item_compare);
}

void free_tag_items(tag_item **tag_items, size_t *count) {
    for (size_t i = 0; i < *count; i++) {
        free((*tag_items)[i].name);
        free((*tag_items)[i].glyph);
    }

    free(*tag_items);  // Free the array itself
    *tag_items = NULL; // Set the pointer to NULL
    *count = 0;        // Set the count to 0
}

char *read_content_tag(const char *sub_path, const char *content_path) {
    if (!sub_path || !*sub_path || !content_path || !*content_path) return NULL;

    char base_name[MAX_BUFFER_SIZE];
    snprintf(base_name, sizeof(base_name), "%s", get_file_name(content_path));

    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    char marker_path[MAX_BUFFER_SIZE];
    snprintf(marker_path, sizeof(marker_path), INFO_CON_PATH "/%s/%s.tag", sub_path, base_name);
    remove_double_slashes(marker_path);

    if (!file_exist(marker_path)) return NULL;

    char *tag = read_line_char_from(marker_path, 1);
    if (!tag) return NULL;

    const char *trimmed = str_remchar(str_trim(tag), ' ');
    char *normalised = *trimmed ? str_tolower(trimmed) : NULL;

    free(tag);
    return normalised;
}

const char *tag_filter_path(void) {
    if (strcasecmp(mux_module, "muxcollect") == 0) return TAG_SORT_COLL;
    if (strcasecmp(mux_module, "muxhistory") == 0) return TAG_SORT_HIST;

    return NULL;
}

int tag_filter_get(char *output, const size_t size) {
    if (!output || size == 0) return 0;
    output[0] = '\0';

    const char *path = tag_filter_path();
    if (!path || !file_exist(path)) return 0;

    char *tag = read_line_char_from(path, 1);
    const int active = tag && *tag;

    if (active) snprintf(output, size, "%s", tag);
    free(tag);

    return active;
}

void tag_filter_apply(void) {
    if (!file_exist(TAG_SORT_PICK)) return;

    const char *path = tag_filter_path();
    if (!path) {
        remove(TAG_SORT_PICK);
        return;
    }

    char *tag = read_line_char_from(TAG_SORT_PICK, 1);

    // An empty pick means we chose None so drop the filter entirely!
    if (tag && *tag) {
        write_text_to_file(path, "w", CHAR, tag);
    } else {
        remove(path);
    }

    free(tag);
    remove(TAG_SORT_PICK);
}

void print_tag_items(const tag_item *tag_items, const size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf(
            "Tag Name: %s  Tag Glyph: %s Tag Sort Bucket: %d\n", tag_items[i].name, tag_items[i].glyph,
            tag_items[i].sort_bucket
        );
    }
}
