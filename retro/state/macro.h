#pragma once

#define MACRO_MAX       32
#define MACRO_STEP_MAX  32
#define MACRO_NAME_MAX  128
#define MACRO_PATH_MAX  512

struct macro_step {
    int target_mask;
    int hz_rate;
};

struct macro_entry {
    int index;
    char name[MACRO_NAME_MAX];
    long long created;
    struct macro_step steps[MACRO_STEP_MAX];
    int step_count;
    char path[MACRO_PATH_MAX];
};

extern struct macro_entry macro_list[MACRO_MAX];
extern int macro_count;

void macros_init(const char *macro_dir);

int macros_create(const char *name);

int macros_rename(int position, const char *new_name);

int macros_delete(int position);

int macros_save(int position);

int macros_add_step(int position, int target_mask, int hz_rate);

int macros_remove_step(int position, int step_pos);

int macros_set_step_hz(int position, int step_pos, int hz_rate);

const struct macro_entry *macros_get_by_index(int index);

const char *macros_get_name_by_index(int index);
