#pragma once

#define GAMESTATE_MAX_SLOTS 64
#define GAMESTATE_NAME_MAX  128

struct gamestate_slot {
    int index;
    char name[GAMESTATE_NAME_MAX];
    long long created;
    char state_path[512];
    char thumb_path[512];
};

extern struct gamestate_slot gamestate_slots[GAMESTATE_MAX_SLOTS];
extern int gamestate_slot_count;
extern struct gamestate_slot gamestate_autosave;
extern int gamestate_autosave_exists;
extern struct gamestate_slot gamestate_quicksave;
extern int gamestate_quicksave_exists;

void gamestate_init(const char *state_dir);

void gamestate_capture_pending(void);

int gamestate_create(const char *name);

int gamestate_rename(int index, const char *new_name);

int gamestate_delete(int index);

int gamestate_load(int index);

void gamestate_autosave_save(void);

int gamestate_autosave_load(void);

int gamestate_autosave_delete(void);

int gamestate_quicksave_save(void);

int gamestate_quicksave_load(void);

int gamestate_quicksave_delete(void);

int gamestate_load_most_recent(void);
