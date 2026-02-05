#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "collection_tag.h"
#include "common.h"

void load_tag_items(tag_item **tag_items, size_t *count) {
    int tag_count;
    char **tags = str_parse_file(INFO_NAM_PATH "/tag.txt", &tag_count, PARSE_LINES);
    if (!tags) return;

    for (int i = 0; i < tag_count; ++i) {
        if (strcasecmp(tags[i], "None") == 0) continue;;;
        if (*tag_items == NULL) {
            *tag_items = malloc(sizeof(tag_item));
        } else {
            *tag_items = realloc(*tag_items, (*count + 1) * sizeof(tag_item));
        }

        char *glpyh_name = str_tolower(str_remchar(str_trim(strdup(tags[i])), ' '));

        (*tag_items)[*count].name = str_capital(strdup(tags[i]));
        (*tag_items)[*count].glyph = glpyh_name;

        char sorting_config_path[MAX_BUFFER_SIZE];
        snprintf(sorting_config_path, sizeof(sorting_config_path), SORTING_CONFIG_PATH "%s", glpyh_name);

        (*tag_items)[*count].sort_bucket = (file_exist(sorting_config_path)) ? read_line_int_from(sorting_config_path, 1) : 0;

        (*count)++;
    }
}

int get_tag_sort_bucket(tag_item *tag_items, size_t count, char *glyph) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(tag_items[i].glyph, glyph) == 0) return tag_items[i].sort_bucket;
    }
    return 0;
}

int tag_item_compare(const void *a, const void *b) {
    tag_item *itemA = (tag_item *) a;
    tag_item *itemB = (tag_item *) b;

    // Use strverscmp for natural sorting on sort_name
    return strverscmp(str_tolower(itemA->name), str_tolower(itemB->name));
}

void sort_tag_items(tag_item *tag_items, size_t count) {
    qsort(tag_items, count, sizeof(tag_item), tag_item_compare);
}

void free_tag_items(tag_item **tag_items, size_t *count) {
    for (size_t i = 0; i < *count; i++) {
        free((*tag_items)[i].name);
        free((*tag_items)[i].glyph);
    }
    free(*tag_items); // Free the array itself
    *tag_items = NULL; // Set the pointer to NULL
    *count = 0; // Set the count to 0
}

void print_tag_items(tag_item *tag_items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf("Tag Name: %s  Tag Glyph: %s Tag Sort Bucket: %d\n", tag_items[i].name, tag_items[i].glyph, tag_items[i].sort_bucket);
    }
}
