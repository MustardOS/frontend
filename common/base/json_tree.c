#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/base/json_tree.h>
#include <common/storage/fileio.h>

typedef struct {
    char *data;
    size_t length;
    size_t size;
    int failed;
} json_buffer;

static char *copy_text(const char *text, const size_t length) {
    char *out = malloc(length + 1);
    if (!out) return NULL;

    memcpy(out, text, length);
    out[length] = '\0';
    return out;
}

static json_node *new_node(const enum json_type type) {
    json_node *node = calloc(1, sizeof(*node));
    if (node) node->type = type;
    return node;
}

static void append_child(json_node *parent, json_node *child) {
    if (parent->last) {
        parent->last->next = child;
    } else {
        parent->child = child;
    }

    parent->last = child;
}

static char *string_value(const struct json value) {
    const size_t length = json_string_length(value);
    char *out = malloc(length + 1);
    if (!out) return NULL;

    json_string_copy(value, out, length + 1);
    return out;
}

static json_node *build(const struct json value) {
    const enum json_type type = json_type(value);
    json_node *node = new_node(type);
    if (!node) return NULL;

    switch (type) {
        case JSON_STRING:
            node->text = string_value(value);
            if (!node->text) goto fail;
            break;
        case JSON_NUMBER:
            node->text = copy_text(json_raw(value), json_raw_length(value));
            if (!node->text) goto fail;
            break;
        case JSON_ARRAY:
            for (struct json item = json_first(value); json_exists(item); item = json_next(item)) {
                json_node *child = build(item);
                if (!child) goto fail;
                append_child(node, child);
            }
            break;
        case JSON_OBJECT:
            for (struct json key = json_first(value); json_exists(key); key = json_next(key)) {
                const struct json member = json_next(key);
                json_node *child = build(member);
                if (!child) goto fail;

                child->key = string_value(key);
                if (!child->key) {
                    json_tree_free(child);
                    goto fail;
                }

                append_child(node, child);
                key = member;
            }
            break;
        default:
            break;
    }

    return node;

fail:
    json_tree_free(node);
    return NULL;
}

json_node *json_tree_parse(const char *text, const size_t length) {
    if (!text || !json_validn(text, length)) return NULL;
    return build(json_parsen(text, length));
}

json_node *json_tree_load(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    size_t size = 0;
    size_t length = 0;
    char *data = NULL;
    char chunk[8192];
    size_t got;

    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (length + got + 1 > size) {
            size = (length + got + 1) * 2;
            char *grown = realloc(data, size);
            if (!grown) {
                free(data);
                fclose(file);
                return NULL;
            }
            data = grown;
        }

        memcpy(data + length, chunk, got);
        length += got;
    }

    const int failed = ferror(file);
    fclose(file);

    json_node *node = NULL;
    if (!failed && data) {
        data[length] = '\0';
        node = json_tree_parse(data, length);
    }

    free(data);
    return node;
}

void json_tree_free(json_node *node) {
    while (node) {
        json_node *next = node->next;
        json_tree_free(node->child);
        free(node->key);
        free(node->text);
        free(node);
        node = next;
    }
}

json_node *json_tree_object(void) {
    return new_node(JSON_OBJECT);
}

json_node *json_tree_array(void) {
    return new_node(JSON_ARRAY);
}

json_node *json_tree_string(const char *value) {
    json_node *node = new_node(JSON_STRING);
    if (!node) return NULL;

    node->text = copy_text(value ? value : "", value ? strlen(value) : 0);
    if (node->text) return node;

    free(node);
    return NULL;
}

json_node *json_tree_int(const long long value) {
    char text[32];
    snprintf(text, sizeof(text), "%lld", value);
    return json_tree_number(text);
}

json_node *json_tree_number(const char *text) {
    json_node *node = new_node(JSON_NUMBER);
    if (!node) return NULL;

    node->text = copy_text(text, strlen(text));
    if (node->text) return node;

    free(node);
    return NULL;
}

json_node *json_tree_null(void) {
    return new_node(JSON_NULL);
}

json_node *json_tree_bool(const int value) {
    return new_node(value ? JSON_TRUE : JSON_FALSE);
}

json_node *json_tree_clone(const json_node *node) {
    if (!node) return NULL;

    json_node *copy = new_node(node->type);
    if (!copy) return NULL;

    if ((node->key && !(copy->key = copy_text(node->key, strlen(node->key))))
        || (node->text && !(copy->text = copy_text(node->text, strlen(node->text))))) {
        json_tree_free(copy);
        return NULL;
    }

    for (const json_node *child = node->child; child; child = child->next) {
        json_node *item = json_tree_clone(child);
        if (!item) {
            json_tree_free(copy);
            return NULL;
        }
        append_child(copy, item);
    }

    return copy;
}

json_node *json_tree_get(const json_node *object, const char *key) {
    if (!object || object->type != JSON_OBJECT) return NULL;

    json_node *found = NULL;
    for (json_node *child = object->child; child; child = child->next) {
        if (strcmp(child->key, key) == 0) found = child;
    }

    return found;
}

json_node *json_tree_set(json_node *object, const char *key, json_node *value) {
    if (!object || object->type != JSON_OBJECT || !value) {
        json_tree_free(value);
        return NULL;
    }

    free(value->key);
    value->key = copy_text(key, strlen(key));
    if (!value->key) {
        json_tree_free(value);
        return NULL;
    }

    json_node *previous = NULL;
    for (json_node *child = object->child; child; previous = child, child = child->next) {
        if (strcmp(child->key, key) != 0) continue;

        value->next = child->next;
        if (previous) {
            previous->next = value;
        } else {
            object->child = value;
        }
        if (object->last == child) object->last = value;

        child->next = NULL;
        json_tree_free(child);
        return value;
    }

    append_child(object, value);
    return value;
}

void json_tree_remove(json_node *object, const char *key) {
    if (!object || object->type != JSON_OBJECT) return;

    json_node *previous = NULL;
    json_node *child = object->child;
    while (child) {
        json_node *next = child->next;
        if (strcmp(child->key, key) == 0) {
            if (previous) {
                previous->next = next;
            } else {
                object->child = next;
            }
            if (object->last == child) object->last = previous;

            child->next = NULL;
            json_tree_free(child);
        } else {
            previous = child;
        }
        child = next;
    }
}

void json_tree_push(json_node *array, json_node *value) {
    if (!array || array->type != JSON_ARRAY || !value) {
        json_tree_free(value);
        return;
    }

    free(value->key);
    value->key = NULL;
    append_child(array, value);
}

size_t json_tree_count(const json_node *node) {
    size_t count = 0;
    if (node) {
        for (const json_node *child = node->child; child; child = child->next)
            count++;
    }
    return count;
}

void json_tree_keep_last(json_node *array, const size_t count) {
    if (!array || array->type != JSON_ARRAY) return;

    size_t total = json_tree_count(array);
    while (total > count) {
        json_node *first = array->child;
        array->child = first->next;
        if (array->last == first) array->last = NULL;

        first->next = NULL;
        json_tree_free(first);
        total--;
    }
}

json_node *json_tree_member_object(json_node *object, const char *key) {
    json_node *member = json_tree_get(object, key);
    if (member && member->type == JSON_OBJECT) return member;
    return json_tree_set(object, key, json_tree_object());
}

int json_tree_truthy(const json_node *node) {
    return node && node->type != JSON_NULL && node->type != JSON_FALSE;
}

long long json_tree_int_or(const json_node *node, const long long fallback) {
    if (!json_tree_truthy(node) || node->type != JSON_NUMBER) return fallback;
    return (long long) strtod(node->text, NULL);
}

const char *json_tree_string_or(const json_node *node, const char *fallback) {
    if (!json_tree_truthy(node) || node->type != JSON_STRING) return fallback;
    return node->text;
}

json_node *json_tree_value_or(const json_node *node, json_node *fallback) {
    if (!json_tree_truthy(node)) return fallback;

    json_tree_free(fallback);
    return json_tree_clone(node);
}

static void put(json_buffer *buffer, const char *text, const size_t length) {
    if (buffer->failed) return;

    if (buffer->length + length + 1 > buffer->size) {
        size_t size = buffer->size ? buffer->size : 1024;
        while (buffer->length + length + 1 > size)
            size *= 2;

        char *grown = realloc(buffer->data, size);
        if (!grown) {
            buffer->failed = 1;
            return;
        }

        buffer->data = grown;
        buffer->size = size;
    }

    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static void put_string(json_buffer *buffer, const char *text) {
    put(buffer, "\"", 1);

    const char *run = text;
    for (const unsigned char *p = (const unsigned char *) text; *p; p++) {
        char escape[8];
        const char *swap = NULL;

        switch (*p) {
            case '"':
                swap = "\\\"";
                break;
            case '\\':
                swap = "\\\\";
                break;
            case '\b':
                swap = "\\b";
                break;
            case '\f':
                swap = "\\f";
                break;
            case '\n':
                swap = "\\n";
                break;
            case '\r':
                swap = "\\r";
                break;
            case '\t':
                swap = "\\t";
                break;
            default:
                if (*p < 0x20 || *p == 0x7f) {
                    snprintf(escape, sizeof(escape), "\\u%04x", *p);
                    swap = escape;
                }
                break;
        }

        if (!swap) continue;

        put(buffer, run, (size_t) ((const char *) p - run));
        put(buffer, swap, strlen(swap));
        run = (const char *) p + 1;
    }

    put(buffer, run, strlen(run));
    put(buffer, "\"", 1);
}

static void put_node(json_buffer *buffer, const json_node *node) {
    switch (node->type) {
        case JSON_STRING:
            put_string(buffer, node->text);
            break;
        case JSON_NUMBER:
            put(buffer, node->text, strlen(node->text));
            break;
        case JSON_TRUE:
            put(buffer, "true", 4);
            break;
        case JSON_FALSE:
            put(buffer, "false", 5);
            break;
        case JSON_ARRAY:
        case JSON_OBJECT: {
            const int object = node->type == JSON_OBJECT;
            put(buffer, object ? "{" : "[", 1);
            for (const json_node *child = node->child; child; child = child->next) {
                if (child != node->child) put(buffer, ",", 1);
                if (object) {
                    put_string(buffer, child->key);
                    put(buffer, ":", 1);
                }
                put_node(buffer, child);
            }
            put(buffer, object ? "}" : "]", 1);
            break;
        }
        default:
            put(buffer, "null", 4);
            break;
    }
}

char *json_tree_dump(const json_node *node, size_t *length) {
    json_buffer buffer = {0};
    if (node) put_node(&buffer, node);

    if (buffer.failed || !buffer.data) {
        free(buffer.data);
        return NULL;
    }

    if (length) *length = buffer.length;
    return buffer.data;
}

int json_tree_write(const json_node *node, const char *path) {
    size_t length = 0;
    char *text = json_tree_dump(node, &length);
    if (!text) return 0;

    const size_t temp_size = strlen(path) + 32;
    char *temp = malloc(temp_size);
    if (!temp) {
        free(text);
        return 0;
    }
    snprintf(temp, temp_size, "%s.tmp.%ld", path, (long) getpid());

    FILE *file = fopen(temp, "wb");
    int written = 0;
    if (file) {
        written = fwrite(text, 1, length, file) == length && fputc('\n', file) != EOF;
        written = fclose(file) == 0 && written;
    }

    if (written && file_rename(temp, path) == 0) {
        free(temp);
        free(text);
        return 1;
    }

    unlink(temp);
    free(temp);
    free(text);
    return 0;
}
