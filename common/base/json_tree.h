#pragma once

#include <stddef.h>
#include <json/json.h>

typedef struct json_node json_node;

struct json_node {
    enum json_type type;
    char *key;
    char *text;
    json_node *child;
    json_node *last;
    json_node *next;
};

json_node *json_tree_parse(const char *text, size_t length);

json_node *json_tree_load(const char *path);

void json_tree_free(json_node *node);

json_node *json_tree_object(void);

json_node *json_tree_array(void);

json_node *json_tree_string(const char *value);

json_node *json_tree_int(long long value);

json_node *json_tree_number(const char *text);

json_node *json_tree_null(void);

json_node *json_tree_bool(int value);

json_node *json_tree_clone(const json_node *node);

json_node *json_tree_get(const json_node *object, const char *key);

json_node *json_tree_set(json_node *object, const char *key, json_node *value);

void json_tree_remove(json_node *object, const char *key);

void json_tree_push(json_node *array, json_node *value);

size_t json_tree_count(const json_node *node);

void json_tree_keep_last(json_node *array, size_t count);

json_node *json_tree_member_object(json_node *object, const char *key);

int json_tree_truthy(const json_node *node);

long long json_tree_int_or(const json_node *node, long long fallback);

const char *json_tree_string_or(const json_node *node, const char *fallback);

json_node *json_tree_value_or(const json_node *node, json_node *fallback);

char *json_tree_dump(const json_node *node, size_t *length);

int json_tree_write(const json_node *node, const char *path);
