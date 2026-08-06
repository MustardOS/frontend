#pragma once

#include <stddef.h>
#include "task_protocol.h"

typedef enum { task_mode_progress = 0, task_mode_prompt, task_mode_terminal, task_mode_background } task_exec_mode;

typedef enum {
    task_state_idle = 0,
    task_state_starting,
    task_state_running,
    task_state_prompt,
    task_state_cancelling,
    task_state_complete,
    task_state_error
} task_exec_state_id;

typedef struct {
    const char *const *argv;
    size_t argc;
    task_exec_mode mode;
    int can_cancel;
    int turbo;
    const char *title;
    const char *success_message;
    const char *failure_message;
} task_exec_spec;

typedef struct {
    task_exec_state_id state;

    char title[TASK_TEXT_MAX];
    char status[TASK_TEXT_MAX];
    char detail[TASK_TEXT_MAX];
    char message[TASK_TEXT_MAX];

    long value;
    long max;
    int determinate;

    int can_cancel;
    int exit_code;
    int cancelled;

    char error_code[TASK_FIELD_MAX];

    int prompt_active;
    task_event prompt;

    unsigned long started_ms;
    unsigned long elapsed_ms;
} task_exec_status;

int task_exec_start(const task_exec_spec *spec);

void task_exec_poll(void);

int task_exec_respond(const char *prompt_id, const char *value);

int task_exec_cancel(void);

int task_exec_active(void);

void task_exec_acknowledge(void);

void task_exec_shutdown(void);

const task_exec_status *task_exec_get_status(void);
