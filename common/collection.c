#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "collection.h"
#include "common.h"
#include "config.h"
#include "device.h"
#include "options.h"

void reformat_display_name(char *display_name) {
    const char *suffix = ", The";
    size_t suffix_len = strlen(suffix);

    char *position = strstr(display_name, suffix);

    if (position != NULL) {
        memmove(position, position + suffix_len, strlen(position + suffix_len) + 1);

        size_t original_len = strlen(display_name);
        memmove(display_name + 4, display_name, original_len + 1);

        memcpy(display_name, "The ", 4);
    }
}

int is_in_list(char **list, int count, const char *sys_dir, const char *name) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", sys_dir, name);

    for (int i = 0; i < count; i++) {
        if (strcasecmp(path, list[i]) == 0) return 1;
    }

    return 0;
}

content_item *add_item(content_item **content_items, size_t *count, const char *name, const char *sort_name,
                       const char *extra_data, content_type content_type) {
    if (*content_items == NULL) {
        *content_items = malloc(sizeof(content_item));
    } else {
        *content_items = realloc(*content_items, (*count + 1) * sizeof(content_item));
    }

    (*content_items)[*count].name = strdup(name);
    (*content_items)[*count].display_name = strdup(sort_name);
    (*content_items)[*count].sort_name = strdup(sort_name);
    (*content_items)[*count].content_type = content_type;
    (*content_items)[*count].extra_data = strdup(extra_data);
    (*content_items)[*count].use_module = strdup(mux_module);
    (*content_items)[*count].glyph_icon = NULL;
    (*content_items)[*count].grid_image = NULL;
    (*content_items)[*count].grid_image_focused = NULL;

    if (config.VISUAL.THETITLEFORMAT) {
        reformat_display_name((*content_items)[*count].display_name);
    }

    (*count)++;

    return &(*content_items)[*count - 1];
}

void remove_item(content_item **content_items, size_t *count, size_t index) {
    if (!content_items || !*content_items || index >= *count) return;

    free((*content_items)[index].name);
    free((*content_items)[index].display_name);
    free((*content_items)[index].sort_name);
    free((*content_items)[index].extra_data);
    free((*content_items)[index].use_module);
    free((*content_items)[index].glyph_icon);
    free((*content_items)[index].grid_image);
    free((*content_items)[index].grid_image_focused);

    if (index < *count - 1) {
        memmove(&(*content_items)[index], &(*content_items)[index + 1],
                ((*count - index - 1) * sizeof(content_item)));
    }

    (*count)--;

    if (*count == 0) {
        free(*content_items);
        *content_items = NULL;
    } else {
        *content_items = realloc(*content_items, (*count) * sizeof(content_item));
    }
}

int bucket_item_compare(const void *a, const void *b) {
    const content_item *itemA = (content_item *) a;
    const content_item *itemB = (content_item *) b;

    if (!config.VISUAL.MIXEDCONTENT && itemA->content_type != itemB->content_type)
        return (itemA->content_type == FOLDER) ? -1 : 1;

    if (itemA->sort_bucket != itemB->sort_bucket)
        return itemB->sort_bucket - itemA->sort_bucket;

    return strcasecmp(itemA->display_name, itemB->display_name);
}

int content_item_compare(const void *a, const void *b) {
    content_item *itemA = (content_item *) a;
    content_item *itemB = (content_item *) b;

    // Compare content_type in descending order
    if (itemA->content_type < itemB->content_type) return -1;
    if (itemA->content_type > itemB->content_type) return 1;

    // Use strverscmp for natural sorting on sort_name
    return strverscmp(str_tolower(itemA->sort_name), str_tolower(itemB->sort_name));
}

int time_compare_for_history(const void *a, const void *b) {
    content_item *itemA = (content_item *) a;
    content_item *itemB = (content_item *) b;

    char mod_file_a[MAX_BUFFER_SIZE];
    char mod_file_b[MAX_BUFFER_SIZE];

    snprintf(mod_file_a, sizeof(mod_file_a), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(itemA->name));
    snprintf(mod_file_b, sizeof(mod_file_b), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(itemB->name));

    if (access(mod_file_a, F_OK) != 0) return 0;
    if (access(mod_file_b, F_OK) != 0) return 0;

    struct stat stat_a, stat_b;

    if (stat(mod_file_a, &stat_a) != 0) return 0;
    if (stat(mod_file_b, &stat_b) != 0) return 0;

    struct timespec time_a = stat_a.st_mtim;
    struct timespec time_b = stat_b.st_mtim;

    if (time_a.tv_sec > time_b.tv_sec) {
        return -1;
    } else if (time_a.tv_sec < time_b.tv_sec) {
        return 1;
    } else {
        if (time_a.tv_nsec > time_b.tv_nsec) {
            return -1;
        } else if (time_a.tv_nsec < time_b.tv_nsec) {
            return 1;
        } else {
            return 0;
        }
    }
}

int item_exists(content_item *content_items, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(content_items[i].name, name) == 0) return 1;
    }
    return 0;
}

void sort_items(content_item *content_items, size_t count) {
    qsort(content_items, count, sizeof(content_item), content_item_compare);
}

void sort_items_time(content_item *content_items, size_t count) {
    qsort(content_items, count, sizeof(content_item), time_compare_for_history);
}

content_item get_item_by_index(content_item *content_items, size_t index) {
    return content_items[index];
}

int get_item_index_by_name(content_item *content_items, size_t count, const char *name, content_type type) {
    for (int i = 0; i < count; i++) {
        if (content_items[i].content_type == type && strcasecmp(content_items[i].name, name) == 0) return i;
    }
    return -1;
}

int get_item_index_by_extra_data(content_item *content_items, size_t count, const char *extra_data) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(content_items[i].extra_data, extra_data) == 0) return i;
    }
    return -1;
}

void free_items(content_item **content_items, size_t *count) {
    for (size_t i = 0; i < *count; i++) {
        free((*content_items)[i].name);
        free((*content_items)[i].display_name);
        free((*content_items)[i].sort_name);  // Freeing all dynamically allocated strings
        free((*content_items)[i].extra_data);
    }
    free(*content_items); // Free the array itself
    *content_items = NULL; // Set the pointer to NULL
    *count = 0; // Set the count to 0
}

void free_item_list(char ***list, int *count) {
    if (!list || !*list) {
        if (count) *count = 0;
        return;
    }

    if (count) {
        for (int i = 0; i < *count; i++) free((*list)[i]);
        *count = 0;
    }

    free(*list);
    *list = NULL;
}

void print_items(content_item *content_items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        char message[MAX_BUFFER_SIZE];
        snprintf(message, sizeof(message),
                 "\nItem %zu\n\tfile_name=%s\n\tdisplay_name=%s\n\tsort_name=%s\n\tcontent_type=%d\n",
                 i, content_items[i].name, content_items[i].display_name, content_items[i].sort_name,
                 content_items[i].content_type);

        char log_location[PATH_MAX];
        snprintf(log_location, sizeof(log_location), "%s/MUOS/log/collection.log",
                 device.STORAGE.ROM.MOUNT);
        write_text_to_file(log_location, "a", CHAR, message);
    }
}
