#pragma once

#include <stddef.h>

#define TASK_LINE_MAX   4096
#define TASK_FIELD_MAX  128
#define TASK_TEXT_MAX   512
#define TASK_OPTION_MAX 4
#define TASK_PREFIX     "MUOS:" // We have our own protocol... kinda! :D

typedef enum {
    task_event_none = 0,
    task_event_begin,
    task_event_status,
    task_event_detail,
    task_event_progress,
    task_event_prompt,
    task_event_log,
    task_event_complete,
    task_event_error,
    task_event_unknown
} task_event_type;

typedef struct {
    task_event_type type;

    char id[TASK_FIELD_MAX];
    char title[TASK_TEXT_MAX];
    char text[TASK_TEXT_MAX];
    char code[TASK_FIELD_MAX];
    char level[TASK_FIELD_MAX];
    char prompt_type[TASK_FIELD_MAX];
    char message[TASK_TEXT_MAX];
    char fallback[TASK_FIELD_MAX];

    char options[TASK_OPTION_MAX][TASK_FIELD_MAX];
    int option_count;

    long value;
    long max;
    int has_value;
    int has_max;

    int malformed;
} task_event;

typedef struct {
    char line[TASK_LINE_MAX];
    size_t len;
    int truncated;
} task_parser;

typedef void (*task_event_cb)(const task_event *event, void *user_data);

void task_parser_reset(task_parser *parser);

void task_parser_feed(task_parser *parser, const char *data, size_t len, task_event_cb callback, void *user_data);

int task_parse_line(const char *line, task_event *event);

const char *task_event_name(task_event_type type);
