#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/screenshot.h"
#include "gamestate.h"
#include "../core/muxretro.h"

#define MAX_STATE_SIZE 512

struct gamestate_slot gamestate_slots[GAMESTATE_MAX_SLOTS];
int gamestate_slot_count = 0;

struct gamestate_slot gamestate_autosave;
int gamestate_autosave_exists = 0;

struct gamestate_slot gamestate_quicksave;
int gamestate_quicksave_exists = 0;

static char base_dir[MAX_STATE_SIZE] = "";
static char manifest_path[MAX_STATE_SIZE] = "";
static char pending_path[MAX_STATE_SIZE] = "";
static char autosave_state_path[MAX_STATE_SIZE] = "";
static char autosave_thumb_path[MAX_STATE_SIZE] = "";
static char quicksave_state_path[MAX_STATE_SIZE] = "";
static char quicksave_thumb_path[MAX_STATE_SIZE] = "";

static void slot_paths(const int index, char *state_path, char *thumb_path) {
    snprintf(state_path, MAX_STATE_SIZE, "%s/slot_%d.state", base_dir, index);
    snprintf(thumb_path, MAX_STATE_SIZE, "%s/slot_%d.png", base_dir, index);
}

static int slot_index_from_group_id(const char *group_id) {
    int index = -1;
    if (sscanf(group_id, "slot_%d", &index) != 1) return -1;
    return index;
}

static void format_epoch(const long long epoch, char *buf, const size_t size) {
    const time_t t = epoch;
    const struct tm *tm_info = localtime(&t);
    if (!tm_info) {
        snprintf(buf, size, "%s", lang.generic.unknown);
        return;
    }
    strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
}

static void write_manifest_group(const char *group_id, const char *name, const long long created) {
    mini_t *ini = mini_try_load(manifest_path);
    if (!ini) ini = mini_create(manifest_path);
    if (!ini) return;

    mini_set_string(ini, group_id, "name", name);
    mini_set_int(ini, group_id, "created", created);

    mini_save(ini, 0);
    mini_free(ini);
}

static void write_manifest_entry(const int index, const char *name, const long long created) {
    char group_id[32];
    snprintf(group_id, sizeof(group_id), "slot_%d", index);
    write_manifest_group(group_id, name, created);
}

void gamestate_init(const char *state_dir) {
    snprintf(base_dir, sizeof(base_dir), "%s", state_dir);
    snprintf(manifest_path, sizeof(manifest_path), "%s/states.ini", base_dir);
    snprintf(pending_path, sizeof(pending_path), "%s/.pending.png", base_dir);
    snprintf(autosave_state_path, sizeof(autosave_state_path), "%s/autosave.state", base_dir);
    snprintf(autosave_thumb_path, sizeof(autosave_thumb_path), "%s/autosave.png", base_dir);
    snprintf(quicksave_state_path, sizeof(quicksave_state_path), "%s/quicksave.state", base_dir);
    snprintf(quicksave_thumb_path, sizeof(quicksave_thumb_path), "%s/quicksave.png", base_dir);

    create_directories(manifest_path, 1);

    gamestate_slot_count = 0;
    gamestate_autosave_exists = 0;
    gamestate_quicksave_exists = 0;

    mini_t *ini = mini_try_load(manifest_path);
    if (!ini) return;

    for (const mini_group_t *group = ini->head; group && gamestate_slot_count < GAMESTATE_MAX_SLOTS;
         group = group->next) {
        if (!group->id) continue;

        if (strcmp(group->id, "autosave") == 0) {
            gamestate_autosave.index = -1;
            snprintf(
                gamestate_autosave.name, sizeof(gamestate_autosave.name), "%s",
                get_ini_string(ini, group->id, "name", lang.muxretro.gamestate.auto_save)
            );
            gamestate_autosave.created = mini_get_int(ini, group->id, "created", 0);
            snprintf(gamestate_autosave.state_path, sizeof(gamestate_autosave.state_path), "%s", autosave_state_path);
            snprintf(gamestate_autosave.thumb_path, sizeof(gamestate_autosave.thumb_path), "%s", autosave_thumb_path);
            gamestate_autosave_exists = 1;
            continue;
        }

        if (strcmp(group->id, "quicksave") == 0) {
            gamestate_quicksave.index = -1;
            snprintf(
                gamestate_quicksave.name, sizeof(gamestate_quicksave.name), "%s",
                get_ini_string(ini, group->id, "name", lang.muxretro.gamestate.quick_save)
            );
            gamestate_quicksave.created = mini_get_int(ini, group->id, "created", 0);
            snprintf(
                gamestate_quicksave.state_path, sizeof(gamestate_quicksave.state_path), "%s", quicksave_state_path
            );
            snprintf(
                gamestate_quicksave.thumb_path, sizeof(gamestate_quicksave.thumb_path), "%s", quicksave_thumb_path
            );
            gamestate_quicksave_exists = 1;
            continue;
        }

        const int index = slot_index_from_group_id(group->id);
        if (index < 0) continue;

        struct gamestate_slot *slot = &gamestate_slots[gamestate_slot_count];
        slot->index = index;
        snprintf(
            slot->name, sizeof(slot->name), "%s", get_ini_string(ini, group->id, "name", lang.muxretro.gamestate.state)
        );
        slot->created = mini_get_int(ini, group->id, "created", 0);
        slot_paths(index, slot->state_path, slot->thumb_path);

        gamestate_slot_count++;
    }

    mini_free(ini);
}

void gamestate_capture_pending(void) {
    if (!base_dir[0]) return;
    screenshot_save(pending_path, screenshot_auto, (screenshot_hue) {0, 0, 0});
}

static int next_free_index(void) {
    int max_index = -1;
    for (int i = 0; i < gamestate_slot_count; i++) {
        if (gamestate_slots[i].index > max_index) max_index = gamestate_slots[i].index;
    }
    return max_index + 1;
}

int gamestate_create(const char *name) {
    if (!base_dir[0] || gamestate_slot_count >= GAMESTATE_MAX_SLOTS) return -1;

    const int position = gamestate_slot_count;
    const int index = next_free_index();

    struct gamestate_slot *slot = &gamestate_slots[position];
    slot->index = index;
    slot_paths(index, slot->state_path, slot->thumb_path);

    if (state_save(slot->state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_create: failed to save state to '%s'", slot->state_path);
        return -1;
    }

    copy_file(pending_path, slot->thumb_path);

    slot->created = (long long) time(NULL);

    if (name && *name) {
        snprintf(slot->name, sizeof(slot->name), "%s", name);
    } else {
        format_epoch(slot->created, slot->name, sizeof(slot->name));
    }

    write_manifest_entry(index, slot->name, slot->created);

    gamestate_slot_count++;
    return position;
}

int gamestate_rename(const int index, const char *new_name) {
    if (index < 0 || index >= gamestate_slot_count) return -1;

    struct gamestate_slot *slot = &gamestate_slots[index];
    snprintf(slot->name, sizeof(slot->name), "%s", new_name);
    write_manifest_entry(slot->index, slot->name, slot->created);

    return 0;
}

int gamestate_delete(const int index) {
    if (index < 0 || index >= gamestate_slot_count) return -1;

    const struct gamestate_slot *slot = &gamestate_slots[index];

    remove(slot->state_path);
    remove(slot->thumb_path);

    mini_t *ini = mini_try_load(manifest_path);
    if (ini) {
        char group_id[32];
        snprintf(group_id, sizeof(group_id), "slot_%d", slot->index);
        mini_delete_group(ini, group_id);
        mini_save(ini, 0);
        mini_free(ini);
    }

    for (int i = index; i < gamestate_slot_count - 1; i++) {
        gamestate_slots[i] = gamestate_slots[i + 1];
    }
    gamestate_slot_count--;

    return 0;
}

int gamestate_load(const int index) {
    if (index < 0 || index >= gamestate_slot_count) return -1;
    return state_load(gamestate_slots[index].state_path);
}

void gamestate_autosave_save(void) {
    if (!base_dir[0]) return;

    if (state_save(autosave_state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_autosave_save: failed to save state to '%s'", autosave_state_path);
        return;
    }

    copy_file(pending_path, autosave_thumb_path);

    gamestate_autosave.created = (long long) time(NULL);

    char created_str[32];
    format_epoch(gamestate_autosave.created, created_str, sizeof(created_str));
    snprintf(
        gamestate_autosave.name, sizeof(gamestate_autosave.name), "%s - %s", lang.muxretro.gamestate.auto_save,
        created_str
    );
    gamestate_autosave.index = -1;
    snprintf(gamestate_autosave.state_path, sizeof(gamestate_autosave.state_path), "%s", autosave_state_path);
    snprintf(gamestate_autosave.thumb_path, sizeof(gamestate_autosave.thumb_path), "%s", autosave_thumb_path);

    write_manifest_group("autosave", gamestate_autosave.name, gamestate_autosave.created);

    gamestate_autosave_exists = 1;
}

int gamestate_autosave_load(void) {
    if (!gamestate_autosave_exists) return -1;
    return state_load(gamestate_autosave.state_path);
}

int gamestate_autosave_delete(void) {
    if (!gamestate_autosave_exists) return -1;

    remove(gamestate_autosave.state_path);
    remove(gamestate_autosave.thumb_path);

    mini_t *ini = mini_try_load(manifest_path);
    if (ini) {
        mini_delete_group(ini, "autosave");
        mini_save(ini, 0);
        mini_free(ini);
    }

    gamestate_autosave_exists = 0;

    return 0;
}

int gamestate_quicksave_save(void) {
    if (!base_dir[0]) return -1;

    if (state_save(quicksave_state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_quicksave_save: failed to save state to '%s'", quicksave_state_path);
        return -1;
    }

    screenshot_save(quicksave_thumb_path, screenshot_auto, (screenshot_hue) {0, 0, 0});
    gamestate_quicksave.created = (long long) time(NULL);

    char created_str[32];
    format_epoch(gamestate_quicksave.created, created_str, sizeof(created_str));
    snprintf(
        gamestate_quicksave.name, sizeof(gamestate_quicksave.name), "%s - %s", lang.muxretro.gamestate.quick_save,
        created_str
    );
    gamestate_quicksave.index = -1;
    snprintf(gamestate_quicksave.state_path, sizeof(gamestate_quicksave.state_path), "%s", quicksave_state_path);
    snprintf(gamestate_quicksave.thumb_path, sizeof(gamestate_quicksave.thumb_path), "%s", quicksave_thumb_path);

    write_manifest_group("quicksave", gamestate_quicksave.name, gamestate_quicksave.created);

    gamestate_quicksave_exists = 1;
    return 0;
}

int gamestate_quicksave_load(void) {
    if (!gamestate_quicksave_exists) return -1;
    return state_load(gamestate_quicksave.state_path);
}

int gamestate_quicksave_delete(void) {
    if (!gamestate_quicksave_exists) return -1;

    remove(gamestate_quicksave.state_path);
    remove(gamestate_quicksave.thumb_path);

    mini_t *ini = mini_try_load(manifest_path);
    if (ini) {
        mini_delete_group(ini, "quicksave");
        mini_save(ini, 0);
        mini_free(ini);
    }

    gamestate_quicksave_exists = 0;

    return 0;
}

int gamestate_load_most_recent(void) {
    long long best_created = -1;
    const char *best_path = NULL;

    if (gamestate_autosave_exists) {
        best_created = gamestate_autosave.created;
        best_path = gamestate_autosave.state_path;
    }

    for (int i = 0; i < gamestate_slot_count; i++) {
        if (gamestate_slots[i].created > best_created) {
            best_created = gamestate_slots[i].created;
            best_path = gamestate_slots[i].state_path;
        }
    }

    if (!best_path) return -1;
    return state_load(best_path);
}
