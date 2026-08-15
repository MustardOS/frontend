#include <stdlib.h>
#include <string.h>
#include "task_protocol.h"

static const struct {
    const char *name;
    task_event_type type;
} event_names[] = {
    {.name = "BEGIN", .type = task_event_begin},       {.name = "STATUS", .type = task_event_status},
    {.name = "DETAIL", .type = task_event_detail},     {.name = "PROGRESS", .type = task_event_progress},
    {.name = "PROMPT", .type = task_event_prompt},     {.name = "LOG", .type = task_event_log},
    {.name = "COMPLETE", .type = task_event_complete}, {.name = "ERROR", .type = task_event_error},
};

void task_parser_reset(task_parser *parser) {
    if (!parser) return;

    parser->len = 0;
    parser->line[0] = '\0';
    parser->truncated = 0;
}

static void copy_field(char *dst, const size_t cap, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++)
        dst[i] = src[i];

    dst[i] = '\0';
}

static void parse_options(task_event *event, const char *value) {
    event->option_count = 0;

    const char *start = value;
    while (*start && event->option_count < TASK_OPTION_MAX) {
        const char *sep = strchr(start, '|');
        const size_t len = sep ? (size_t) (sep - start) : strlen(start);

        char *slot = event->options[event->option_count];
        const size_t copy = len < TASK_FIELD_MAX - 1 ? len : TASK_FIELD_MAX - 1;
        memcpy(slot, start, copy);
        slot[copy] = '\0';

        event->option_count++;
        if (!sep) break;
        start = sep + 1;
    }
}

static int parse_long(const char *value, long *out) {
    if (!value || !*value) return 0;

    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (end == value || (end && *end)) return 0;

    *out = parsed;
    return 1;
}

static void apply_field(task_event *event, const char *key, const char *value) {
    if (strcmp(key, "ID") == 0) {
        copy_field(event->id, sizeof(event->id), value);
    } else if (strcmp(key, "TITLE") == 0) {
        copy_field(event->title, sizeof(event->title), value);
    } else if (strcmp(key, "TEXT") == 0) {
        copy_field(event->text, sizeof(event->text), value);
    } else if (strcmp(key, "CODE") == 0) {
        copy_field(event->code, sizeof(event->code), value);
    } else if (strcmp(key, "LEVEL") == 0) {
        copy_field(event->level, sizeof(event->level), value);
    } else if (strcmp(key, "TYPE") == 0) {
        copy_field(event->prompt_type, sizeof(event->prompt_type), value);
    } else if (strcmp(key, "MESSAGE") == 0) {
        copy_field(event->message, sizeof(event->message), value);
    } else if (strcmp(key, "DEFAULT") == 0) {
        copy_field(event->fallback, sizeof(event->fallback), value);
    } else if (strcmp(key, "OPTIONS") == 0) {
        parse_options(event, value);
    } else if (strcmp(key, "VALUE") == 0) {
        event->has_value = parse_long(value, &event->value);
        if (!event->has_value) event->malformed = 1;
    } else if (strcmp(key, "MAX") == 0) {
        event->has_max = parse_long(value, &event->max);
        if (!event->has_max) event->malformed = 1;
    }
}

int task_parse_line(const char *line, task_event *event) {
    if (!line || !event) return 0;

    const size_t prefix_len = strlen(TASK_PREFIX);
    if (strncmp(line, TASK_PREFIX, prefix_len) != 0) return 0;

    memset(event, 0, sizeof(*event));
    event->type = task_event_unknown;

    const char *cursor = line + prefix_len;

    // The record name runs to the first tab!
    const char *tab = strchr(cursor, '\t');
    const size_t name_len = tab ? (size_t) (tab - cursor) : strlen(cursor);

    for (size_t i = 0; i < sizeof(event_names) / sizeof(event_names[0]); i++) {
        if (strlen(event_names[i].name) != name_len) continue;
        if (strncmp(cursor, event_names[i].name, name_len) != 0) continue;

        event->type = event_names[i].type;
        break;
    }

    while (tab) {
        const char *field = tab + 1;
        tab = strchr(field, '\t');

        const size_t field_len = tab ? (size_t) (tab - field) : strlen(field);
        if (field_len == 0) continue;

        char pair[TASK_TEXT_MAX + TASK_FIELD_MAX];
        const size_t copy = field_len < sizeof(pair) - 1 ? field_len : sizeof(pair) - 1;
        memcpy(pair, field, copy);
        pair[copy] = '\0';

        char *equals = strchr(pair, '=');
        if (!equals) {
            event->malformed = 1;
            continue;
        }

        *equals = '\0';
        apply_field(event, pair, equals + 1);
    }

    if (event->type == task_event_progress && (!event->has_max || event->max <= 0)) event->has_max = 0;

    return 1;
}

void task_parser_feed(
    task_parser *parser, const char *data, const size_t len, const task_event_cb callback, void *user_data
) {
    if (!parser || !data) return;

    for (size_t i = 0; i < len; i++) {
        const char c = data[i];

        if (c != '\n') {
            if (parser->len + 1 < sizeof(parser->line)) {
                parser->line[parser->len++] = c;
            } else {
                parser->truncated = 1;
            }
            continue;
        }

        parser->line[parser->len] = '\0';

        if (!parser->truncated && parser->len > 0) {
            task_event event;
            if (task_parse_line(parser->line, &event) && callback) callback(&event, user_data);
        }

        parser->len = 0;
        parser->truncated = 0;
    }
}
