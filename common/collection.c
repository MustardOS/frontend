#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "collection.h"
#include "common.h"
#include "config.h"
#include "options.h"

content_item *add_item(content_item **content_items, size_t *count, const char *name, const char *sort_name,
                       content_type content_type) {
    *content_items = realloc(*content_items, (*count + 1) * sizeof(content_item));

    (*content_items)[*count].name = strdup(name);
    (*content_items)[*count].display_name = strdup(sort_name);
    (*content_items)[*count].sort_name = strdup(sort_name);
    (*content_items)[*count].content_type = content_type;

    (*count)++;

    return &(*content_items)[*count - 1];
}

int content_item_compare(const void *a, const void *b) {
    content_item *itemA = (content_item *) a;
    content_item *itemB = (content_item *) b;

    // Compare content_type in descending order
    if (itemA->content_type < itemB->content_type) return -1;
    if (itemA->content_type > itemB->content_type) return 1;

    const char *str1 = itemA->sort_name;
    const char *str2 = itemB->sort_name;

    while (*str1 && *str2) {
        char c1 = tolower(*str1);
        char c2 = tolower(*str2);

        if (c1 != c2) {
            return c1 - c2;
        }

        str1++;
        str2++;
    }

    return *str1 - *str2;
}

int time_compare_for_history(const void *a, const void *b) {
    content_item *itemA = (content_item *) a;
    content_item *itemB = (content_item *) b;

    char mod_file_a[MAX_BUFFER_SIZE];
    char mod_file_b[MAX_BUFFER_SIZE];

    snprintf(mod_file_a, sizeof(mod_file_a), "%s/MUOS/info/history/%s.cfg",
             get_default_storage(config.STORAGE.FAV), strip_ext(itemA->name));
    snprintf(mod_file_b, sizeof(mod_file_b), "%s/MUOS/info/history/%s.cfg",
             get_default_storage(config.STORAGE.FAV), strip_ext(itemB->name));

    if (access(mod_file_a, F_OK) != 0) {
        printf("Error: %s does not exist\n", mod_file_a);
        return 0;
    }

    if (access(mod_file_b, F_OK) != 0) {
        printf("Error: %s does not exist\n", mod_file_b);
        return 0;
    }

    struct stat stat_a, stat_b;

    if (stat(mod_file_a, &stat_a) != 0) {
        printf("Error getting file information for %s\n", mod_file_a);
        return 0;
    }

    if (stat(mod_file_b, &stat_b) != 0) {
        printf("Error getting file information for %s\n", mod_file_b);
        return 0;
    }

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

void sort_items(content_item *content_items, size_t count) {
    qsort(content_items, count, sizeof(content_item), content_item_compare);
}

void sort_items_time(content_item *content_items, size_t count) {
    qsort(content_items, count, sizeof(content_item), time_compare_for_history);
}

content_item get_item_by_index(content_item *content_items, size_t index) {
    return content_items[index];
}

void free_items(content_item *content_items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(content_items[i].name);
        free(content_items[i].display_name);
    }
    free(content_items);
}

void print_items(content_item *content_items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        char message[1024];
        snprintf(message, sizeof(message), "Item %zu  file_name=%s  display_name=%s  sort_name=%s  content_type=%d\n",
                 i, content_items[i].name, content_items[i].display_name, content_items[i].sort_name,
                 content_items[i].content_type);
        write_text_to_file("/mnt/mmc/MUOS/log/collection.log", "a", CHAR, message);
    }
}
