#pragma once

#include <stddef.h>
#include <stdio.h>

#define REMOTE_CONTROL_BUFFER      512
#define REMOTE_CONTROL_POLL_MS     8
#define REMOTE_CONTROL_INITIALIZER {.reader = -1, .keeper = -1}

typedef struct {
    int reader;
    int keeper;
    char buffer[REMOTE_CONTROL_BUFFER];
    size_t length;
} remote_control;

int remote_control_open(remote_control *control, const char *directory, const char *fifo);

void remote_control_close(remote_control *control, const char *fifo);

int remote_control_read_line(remote_control *control, char *line, size_t size);

void remote_json_text(FILE *file, const char *key, const char *value);
