#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "array.h"

void initialise_array(struct items *item) {
    item->array = (char **) malloc(sizeof(char *));
    item->size = 0;
    item->capacity = 1;
}

void push_string(struct items *item, char *str) {
    if (item->size == item->capacity) {
        item->capacity *= 2;
        item->array = (char **) realloc(item->array, item->capacity * sizeof(char *));
    }

    item->array[item->size] = strdup(str);
    item->size++;
}

char *get_string_at_index(struct items *item, int index) {
    if (index < item->size) {
        return item->array[index];
    } else {
        return NULL;
    }
}

void free_array(struct items *item) {
    for (size_t i = 0; i < item->size; i++) {
        free(item->array[i]);
    }
    free(item->array);
    item->size = 0;
    item->capacity = 0;
}

void print_array(struct items *item, int split) {
    printf("[\n");
    for (int i = 0; i < item->size; i++) {
        if (i > 0 && (i + 1) % split == 0) {
            printf("");
        }
        printf("\"%s\"", item->array[i]);
        if (i < item->size - 1) {
            printf(", ");
        }
        if ((i + 1) % split == 0) {
            printf("\n");
        }
    }
    printf("\n]\n");
}
