#pragma once

#define CHEAT_MAX      64
#define CHEAT_DESC_MAX 128
#define CHEAT_CODE_MAX 256

struct cheat_entry {
    char desc[CHEAT_DESC_MAX];
    char code[CHEAT_CODE_MAX];
    int enabled;
};

extern struct cheat_entry cheats_list[CHEAT_MAX];
extern int cheats_count;

void cheats_init(const char *core_path_arg, const char *content_path);

int cheats_supported(void);

int cheats_toggle(int index);

int cheats_create(const char *description, const char *code);

int cheats_update(int index, const char *description, const char *code);

int cheats_delete(int index);

void cheats_set_suppressed(int suppressed);
