#pragma once

#include <stddef.h>

#define MACRO_MAX      32
#define MACRO_STEP_MAX 32
#define MACRO_NAME_MAX 128
#define MACRO_PATH_MAX 512

#define MACRO_WAIT_MS_DEFAULT    0
#define MACRO_HOLD_MS_DEFAULT    96
#define MACRO_REPEAT_DEFAULT     1
#define MACRO_LOOP_COUNT_DEFAULT 2
#define MACRO_ERROR_MAX          128

enum { macro_step_button = 0, macro_step_goto, macro_step_loop, macro_step_if };

enum { if_test_button_held = 0, if_test_count_compare };

enum { if_op_equals = 0, if_op_notequals, if_op_less, if_op_greater, if_op_atleast, if_op_atmost };

struct macro_step {
    int kind;
    int target_mask;
    int wait_ms;
    int hold_ms;
    int repeat;
    int jump_target;
    int loop_count;
    int if_test;
    int if_negate;
    int if_op;
    int if_loop_ref;
};

struct macro_entry {
    int index;
    char name[MACRO_NAME_MAX];
    long long created;
    struct macro_step steps[MACRO_STEP_MAX];
    int step_count;
    char path[MACRO_PATH_MAX];
    int is_relish;
    char compile_error[MACRO_ERROR_MAX];
};

extern struct macro_entry macro_list[MACRO_MAX];
extern int macro_count;

void macros_init(const char *macro_dir);

int macros_create(const char *name);

int macros_rename(int position, const char *new_name);

int macros_delete(int position);

int macros_save(int position);

int macros_add_step(int position, const struct macro_step *new_step);

int macros_remove_step(int position, int step_pos);

int macros_set_step_wait_ms(int position, int step_pos, int wait_ms);

int macros_set_step_hold_ms(int position, int step_pos, int hold_ms);

int macros_set_step_repeat(int position, int step_pos, int repeat);

int macros_cycle_step_time_ms(int current_ms, int direction);

int macros_cycle_step_repeat(int current_repeat, int direction);

const char *macros_time_ms_name(int time_ms);

const char *macros_step_compact_label(const struct macro_step *step, char *buf, size_t buf_len);

const struct macro_entry *macros_get_by_index(int index);

const char *macros_get_name_by_index(int index);
