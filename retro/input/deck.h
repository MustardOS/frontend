#pragma once

#include "../macro/macro.h"
#include "../settings/settings.h"

#define DECK_MAX      16
#define DECK_NAME_MAX 128
#define DECK_PATH_MAX 512

struct deck_entry {
    int index;
    char name[DECK_NAME_MAX];
    long long created;
    int route;
    int priority;
    int source_target[PORT_SOURCE_COUNT];
    int source_turbo[PORT_SOURCE_COUNT];
    int source_macro[PORT_SOURCE_COUNT];
    char source_macro_name[PORT_SOURCE_COUNT][MACRO_NAME_MAX];
    char path[DECK_PATH_MAX];
    int dirty;
};

extern struct deck_entry deck_list[DECK_MAX];
extern int deck_count;

void decks_init(const char *deck_dir);

int decks_create(const char *name);

int decks_rename(int position, const char *new_name);

int decks_delete(int position);

int decks_save(int position);

void decks_mark_dirty(int position);

void decks_flush(void);

int decks_position_from_index(int index);

void decks_clear_macro_references(int macro_index);

void decks_set_macro_name(int position, int source, int macro_index);

void decks_resolve_macros(void);
