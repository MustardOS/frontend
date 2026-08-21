#pragma once

#include <stddef.h>

#define GAMESTATE_MAX_SLOTS      64
#define GAMESTATE_NAME_MAX       128
#define GAMESTATE_TIMELINE_DEPTH 10
#define GAMESTATE_TRASH_MAX      10

struct gamestate_slot {
    int index;
    char name[GAMESTATE_NAME_MAX];
    long long created;
    char state_path[512];
    char thumb_path[512];
    char crc[16];
    char core[64];
    char core_version[64];
};

extern struct gamestate_slot gamestate_slots[GAMESTATE_MAX_SLOTS];
extern int gamestate_slot_count;
extern struct gamestate_slot gamestate_autosave;
extern int gamestate_autosave_exists;
extern struct gamestate_slot gamestate_quicksave;
extern int gamestate_quicksave_exists;
extern struct gamestate_slot gamestate_timeline[GAMESTATE_TIMELINE_DEPTH];
extern int gamestate_timeline_exists[GAMESTATE_TIMELINE_DEPTH];

extern struct gamestate_slot gamestate_trash[GAMESTATE_TRASH_MAX];
extern int gamestate_trash_count;

int gamestate_trash_restore(int index);

int gamestate_trash_delete(int index);

int gamestate_trash_empty(void);

int gamestate_init(const char *state_dir);

void gamestate_capture_pending(int restore_visibility);

const char *gamestate_pending_thumbnail(void);

void gamestate_publish_task(void);

void gamestate_publish_flush(void);

int gamestate_create(const char *name);

int gamestate_rename(int index, const char *new_name);

int gamestate_delete(int index);

int gamestate_load(int index);

int gamestate_autosave_save(void);

void gamestate_autosave_arm(void);

int gamestate_autosave_is_armed(void);

int gamestate_autosave_load(void);

int gamestate_autosave_delete(void);

int gamestate_quicksave_save(void);

int gamestate_quicksave_load(void);

int gamestate_quicksave_delete(void);

int gamestate_timeline_save(void);

int gamestate_timeline_load(int slot);

int gamestate_timeline_delete(int slot);

int gamestate_metadata_matches(const struct gamestate_slot *slot);

int gamestate_protect_mismatched_autosave(void);

int gamestate_load_most_recent(int *mismatch_blocked, int show_message);

int gamestate_find_most_recent(char *path, size_t path_len, int *mismatch_blocked);
