#pragma once

#include <stddef.h>

extern struct items item;

struct items {
    char **array;
    int size;
    int capacity;
};

void initialise_array(struct items *item);

void push_string(struct items *item, char *str);

char *get_string_at_index(struct items *item, int index);

void free_array(struct items *item);

void print_array(struct items *item, int split);
