#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdint.h>
#include <common/content/collection/common.h>
#include <common/config/config.h>
#include <common/platform/device.h>
#include <common/base/strutil.h>
#include <common/storage/fileio.h>
#include <common/runtime/init.h>

static void reformat_display_name(char *display_name) {
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

    const size_t name_size = strlen(name) + 1;
    const size_t sort_size = strlen(sort_name) + 1;
    const size_t extra_size = strlen(extra_data) + 1;
    if (name_size > SIZE_MAX - sort_size || name_size + sort_size > SIZE_MAX - extra_size) return NULL;

    content_item *item = &(*content_items)[*count];
    memset(item, 0, sizeof(*item));
    item->string_storage = malloc(name_size + sort_size + extra_size);
    item->display_name = strdup(sort_name);
    if (!item->string_storage || !item->display_name) {
        free(item->string_storage);
        free(item->display_name);
        memset(item, 0, sizeof(*item));
        return NULL;
    }

    item->name = item->string_storage;
    item->sort_name = item->name + name_size;
    item->extra_data = item->sort_name + sort_size;
    memcpy(item->name, name, name_size);
    memcpy(item->sort_name, sort_name, sort_size);
    memcpy(item->extra_data, extra_data, extra_size);
    item->content_type = content_type;
    item->use_module = mux_module;

    if (config.visual.the_title_format && content_label_module()) {
        reformat_display_name(item->display_name);
    }

    adjust_content_label(item->display_name);

    (*count)++;

    return &(*content_items)[*count - 1];
}

static void free_item_fields(const content_item *item) {
    free(item->string_storage);
    free(item->display_name);
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

int content_item_compare(const void *a, const void *b) {
    const content_item *item_a = (content_item *) a;
    const content_item *item_b = (content_item *) b;

    // Compare content_type in descending order
    if (item_a->content_type < item_b->content_type) return -1;
    if (item_a->content_type > item_b->content_type) return 1;

    // Case-insensitive natural sort on sort_name, allocation-free
    return str_compare(&item_a->sort_name, &item_b->sort_name);
}

static int history_time_compare(const void *a, const void *b) {
    const content_item *item_a = a;
    const content_item *item_b = b;

    if (item_a->order.added != item_b->order.added) return item_a->order.added > item_b->order.added ? -1 : 1;
    if (item_a->order.file_size != item_b->order.file_size)
        return item_a->order.file_size > item_b->order.file_size ? -1 : 1;

    return str_compare(&item_a->sort_name, &item_b->sort_name);
}

int item_exists(const content_item *content_items, const size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(content_items[i].name, name) == 0) return 1;
    }
    return 0;
}

void sort_items(content_item *content_items, const size_t count) {
    if (!content_items || count < 2U) return;
    qsort(content_items, count, sizeof(content_item), content_item_compare);
}

void sort_items_time(content_item *content_items, const size_t count) {
    if (!content_items || count < 2U) return;

    for (size_t i = 0; i < count; i++) {
        content_item *item = &content_items[i];
        char history_file[PATH_MAX];
        struct stat st;

        item->order.added = 0;
        item->order.file_size = 0;

        const int written = snprintf(history_file, sizeof(history_file), "%s/%s", INFO_HIS_PATH, item->name);
        if (written < 0 || (size_t) written >= sizeof(history_file)) continue;

        if (stat(history_file, &st) == 0) {
            item->order.added = st.st_mtim.tv_sec;
            item->order.file_size = st.st_mtim.tv_nsec;
        }
    }

    qsort(content_items, count, sizeof(content_item), history_time_compare);
}

int get_item_index_by_name(
    const content_item *content_items, const size_t count, const char *name, const content_type type
) {
    for (size_t i = 0; i < count; i++) {
        if (content_items[i].content_type == type && strcasecmp(content_items[i].name, name) == 0) return (int) i;
    }
    return -1;
}

int get_item_index_by_extra_data(const content_item *content_items, const size_t count, const char *extra_data) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(content_items[i].extra_data, extra_data) == 0) return (int) i;
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
