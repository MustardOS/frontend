#include <stdlib.h>
#include <string.h>
#include "tag.h"
#include "../options.h"
#include "../strutil.h"
#include "../fileio.h"
#include "../sysinfo.h"

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

        char *glyph_name = str_tolower(str_remchar(str_trim(strdup(tags[i])), ' '));

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

void print_tag_items(const tag_item *tag_items, const size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf(
            "Tag Name: %s  Tag Glyph: %s Tag Sort Bucket: %d\n", tag_items[i].name, tag_items[i].glyph,
            tag_items[i].sort_bucket
        );
    }
}
