#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "common.h"
#include "../config.h"
#include "../device.h"
#include "../strutil.h"
#include "../fileio.h"
#include "../init.h"

void reformat_display_name(char *display_name) {
    const char *suffix = ", The";
    const size_t suffix_len = strlen(suffix);

    char *position = strstr(display_name, suffix);

    if (position != NULL) {
        memmove(position, position + suffix_len, strlen(position + suffix_len) + 1);

        const size_t original_len = strlen(display_name);
        memmove(display_name + 4, display_name, original_len + 1);

        memcpy(display_name, "The ", 4);
    }
}

content_item *add_item(
    content_item **content_items, size_t *count, const char *name, const char *sort_name, const char *extra_data,
    const content_type content_type
) {
    if (*content_items == NULL) {
        content_item *new_items = malloc(sizeof(content_item));
        if (!new_items) return NULL;
        *content_items = new_items;
    } else if (*count > 0 && (*count & (*count - 1)) == 0) {
        content_item *new_items = realloc(*content_items, *count * 2 * sizeof(content_item));
        if (!new_items) return NULL;
        *content_items = new_items;
    }

    (*content_items)[*count].name = strdup(name);
    (*content_items)[*count].display_name = strdup(sort_name);
    (*content_items)[*count].sort_name = strdup(sort_name);
    (*content_items)[*count].content_type = content_type;
    (*content_items)[*count].extra_data = strdup(extra_data);
    (*content_items)[*count].use_module = strdup(mux_module);
    (*content_items)[*count].help = NULL;
    (*content_items)[*count].glyph_icon = NULL;
    (*content_items)[*count].grid_image = NULL;
    (*content_items)[*count].grid_image_focused = NULL;
    (*content_items)[*count].sort_bucket = 0;
    (*content_items)[*count].folder_item_count = 0;
    (*content_items)[*count].group_tag[0] = '\0';

    if (config.visual.the_title_format) {
        reformat_display_name((*content_items)[*count].display_name);
    }

    adjust_visual_label((*content_items)[*count].display_name, config.visual.name, config.visual.dash);

    (*count)++;

    return &(*content_items)[*count - 1];
}

static void free_item_fields(content_item *item) {
    free(item->name);
    free(item->display_name);
    free(item->sort_name);
    free(item->extra_data);
    free(item->use_module);
    free(item->help);
    free(item->glyph_icon);
    free(item->grid_image);
    free(item->grid_image_focused);
}

void remove_item(content_item **content_items, size_t *count, const size_t index) {
    if (!content_items || !*content_items || index >= *count) return;

    free_item_fields(&(*content_items)[index]);

    if (index < *count - 1) {
        memmove(&(*content_items)[index], &(*content_items)[index + 1], (*count - index - 1) * sizeof(content_item));
    }

    (*count)--;

    if (*count == 0) {
        free(*content_items);
        *content_items = NULL;
    }
}

int bucket_item_compare(const void *a, const void *b) {
    const content_item *item_a = (content_item *) a;
    const content_item *item_b = (content_item *) b;

    if (!config.visual.mixed_content && item_a->content_type != item_b->content_type)
        return item_a->content_type == FOLDER ? -1 : 1;

    if (item_a->sort_bucket != item_b->sort_bucket) return item_b->sort_bucket - item_a->sort_bucket;

    // Case-insensitive natural sort on sort_name, allocation-free
    return str_compare(&item_a->sort_name, &item_b->sort_name);
}

int content_item_compare(const void *a, const void *b) {
    const content_item *item_a = (content_item *) a;
    const content_item *item_b = (content_item *) b;

    // Compare content_type in descending order
    if (item_a->content_type < item_b->content_type) return -1;
    if (item_a->content_type > item_b->content_type) return 1;

    // Case-insensitive natural sort on sort_name, allocation-free
    return str_compare(&item_a->sort_name, &item_b->sort_name);
}

int time_compare_for_history(const void *a, const void *b) {
    const content_item *item_a = (content_item *) a;
    const content_item *item_b = (content_item *) b;

    char mod_file_a[MAX_BUFFER_SIZE];
    char mod_file_b[MAX_BUFFER_SIZE];

    snprintf(mod_file_a, sizeof(mod_file_a), "%s/%s.cfg", INFO_HIS_PATH, strip_ext(item_a->name));
    snprintf(mod_file_b, sizeof(mod_file_b), "%s/%s.cfg", INFO_HIS_PATH, strip_ext(item_b->name));

    if (access(mod_file_a, F_OK) != 0) return 0;
    if (access(mod_file_b, F_OK) != 0) return 0;

    struct stat stat_a, stat_b;

    if (stat(mod_file_a, &stat_a) != 0) return 0;
    if (stat(mod_file_b, &stat_b) != 0) return 0;

    const struct timespec time_a = stat_a.st_mtim;
    const struct timespec time_b = stat_b.st_mtim;

    if (time_a.tv_sec > time_b.tv_sec) {
        return -1;
    }
    if (time_a.tv_sec < time_b.tv_sec) {
        return 1;
    }
    if (time_a.tv_nsec > time_b.tv_nsec) {
        return -1;
    }
    if (time_a.tv_nsec < time_b.tv_nsec) {
        return 1;
    }
    return 0;
}

int item_exists(const content_item *content_items, const size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(content_items[i].name, name) == 0) return 1;
    }
    return 0;
}

void sort_items(content_item *content_items, const size_t count) {
    qsort(content_items, count, sizeof(content_item), content_item_compare);
}

void sort_items_time(content_item *content_items, const size_t count) {
    qsort(content_items, count, sizeof(content_item), time_compare_for_history);
}

content_item get_item_by_index(const content_item *items, const size_t index) {
    return items[index];
}

int get_item_index_by_name(
    const content_item *content_items, const size_t count, const char *name, const content_type type
) {
    for (int i = 0; i < count; i++) {
        if (content_items[i].content_type == type && strcasecmp(content_items[i].name, name) == 0) return i;
    }
    return -1;
}

int get_item_index_by_extra_data(const content_item *content_items, const size_t count, const char *extra_data) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(content_items[i].extra_data, extra_data) == 0) return i;
    }
    return -1;
}

void free_items(content_item **content_items, size_t *count) {
    for (size_t i = 0; i < *count; i++) {
        free_item_fields(&(*content_items)[i]);
    }
    free(*content_items);  // Free the array itself
    *content_items = NULL; // Set the pointer to NULL
    *count = 0;            // Set the count to 0
}

void free_item_list(char ***list, int *count) {
    if (!list || !*list) {
        if (count) *count = 0;
        return;
    }

    if (count) {
        for (int i = 0; i < *count; i++)
            free((*list)[i]);
        *count = 0;
    }

    free(*list);
    *list = NULL;
}

void print_items(const content_item *content_items, const size_t count) {
    for (size_t i = 0; i < count; i++) {
        char message[MAX_BUFFER_SIZE];
        snprintf(
            message, sizeof(message),
            "\nItem %zu\n\tfile_name=%s\n\tdisplay_name=%s\n\tsort_name=%s\n\tcontent_type=%d\n", i,
            content_items[i].name, content_items[i].display_name, content_items[i].sort_name,
            content_items[i].content_type
        );

        char log_location[PATH_MAX];
        snprintf(log_location, sizeof(log_location), "%s/MUOS/log/collection.log", device.storage.rom.mount);
        write_text_to_file(log_location, "a", CHAR, message);
    }
}
