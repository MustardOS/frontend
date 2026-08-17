#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/strutil.h"
#include "../video/image_writer.h"
#include "gamestate.h"
#include "content_hash.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../settings/settings.h"

#define MAX_STATE_SIZE 512

struct gamestate_slot gamestate_slots[GAMESTATE_MAX_SLOTS];
int gamestate_slot_count = 0;

struct gamestate_slot gamestate_autosave;
int gamestate_autosave_exists = 0;
static int gamestate_autosave_armed = 0;

struct gamestate_slot gamestate_quicksave;
int gamestate_quicksave_exists = 0;

struct gamestate_slot gamestate_timeline[GAMESTATE_TIMELINE_DEPTH];
int gamestate_timeline_exists[GAMESTATE_TIMELINE_DEPTH];

static char base_dir[MAX_STATE_SIZE] = "";
static char manifest_path[MAX_STATE_SIZE] = "";
static char pending_path[MAX_STATE_SIZE] = "";
static char autosave_state_path[MAX_STATE_SIZE] = "";
static char autosave_thumb_path[MAX_STATE_SIZE] = "";
static char quicksave_state_path[MAX_STATE_SIZE] = "";
static char quicksave_thumb_path[MAX_STATE_SIZE] = "";

#define RESUME_CANDIDATE_LIMIT (GAMESTATE_MAX_SLOTS + GAMESTATE_TIMELINE_DEPTH + 2)

struct resume_candidate {
    const struct gamestate_slot *slot;
    struct timespec modified;
    int order;
};

#define RESUME_INDEX_LIMIT 12

enum publish_kind { publish_none = 0, publish_slot, publish_autosave, publish_quicksave, publish_timeline };

static struct {
    enum publish_kind kind;
    int position;
    struct gamestate_slot previous;
    int previous_exists;
} pending_publish;

static int state_file_timestamp(const char *path, struct timespec *modified) {
    struct stat st;
    if (!path || !*path || stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) return 0;

    if (modified) *modified = st.st_mtim;
    return 1;
}

static int resume_candidate_compare(const void *left_ptr, const void *right_ptr) {
    const struct resume_candidate *left = left_ptr;
    const struct resume_candidate *right = right_ptr;

    if (left->slot->created != right->slot->created) return left->slot->created < right->slot->created ? 1 : -1;
    if (left->modified.tv_sec != right->modified.tv_sec) return left->modified.tv_sec < right->modified.tv_sec ? 1 : -1;
    if (left->modified.tv_nsec != right->modified.tv_nsec)
        return left->modified.tv_nsec < right->modified.tv_nsec ? 1 : -1;
    return right->order - left->order;
}

static void append_resume_candidate(
    struct resume_candidate *candidates, int *count, const struct gamestate_slot *slot, const int exists
) {
    if (!exists || *count >= RESUME_CANDIDATE_LIMIT) return;

    struct timespec modified;
    if (!state_file_timestamp(slot->state_path, &modified)) return;

    candidates[*count] = (struct resume_candidate) {
        .slot = slot,
        .modified = modified,
        .order = *count,
    };
    (*count)++;
}

static int collect_resume_candidates(struct resume_candidate *candidates) {
    int count = 0;
    append_resume_candidate(candidates, &count, &gamestate_quicksave, gamestate_quicksave_exists);
    append_resume_candidate(candidates, &count, &gamestate_autosave, gamestate_autosave_exists);
    for (int i = 0; i < GAMESTATE_TIMELINE_DEPTH; i++)
        append_resume_candidate(candidates, &count, &gamestate_timeline[i], gamestate_timeline_exists[i]);
    for (int i = 0; i < gamestate_slot_count; i++)
        append_resume_candidate(candidates, &count, &gamestate_slots[i], 1);

    qsort(candidates, (size_t) count, sizeof(*candidates), resume_candidate_compare);
    return count;
}

static int gamestate_resume_matches(const struct gamestate_slot *slot);

static uint64_t resume_path_key(const char *path) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *) path; *cursor; cursor++) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static const struct gamestate_slot *latest_resume_slot(const char **type) {
    struct resume_candidate candidates[RESUME_CANDIDATE_LIMIT];
    const int count = collect_resume_candidates(candidates);
    *type = "";

    for (int i = 0; i < count; i++) {
        const struct gamestate_slot *slot = candidates[i].slot;
        if (!gamestate_resume_matches(slot)) continue;

        if (slot == &gamestate_quicksave)
            *type = "quicksave";
        else if (slot == &gamestate_autosave)
            *type = "autosave";
        else {
            *type = "manual";
            for (int timeline = 0; timeline < GAMESTATE_TIMELINE_DEPTH; timeline++) {
                if (slot != &gamestate_timeline[timeline]) continue;
                *type = "timeline";
                break;
            }
        }
        return slot;
    }

    return NULL;
}

static void prune_resume_index(mini_t *ini) {
    for (;;) {
        int count = 0;
        long long oldest_time = LLONG_MAX;
        char oldest_group[48] = "";

        for (const mini_group_t *group = ini->head; group; group = group->next) {
            if (!group->id || strncmp(group->id, "content_", 8) != 0) continue;
            count++;
            const long long created = mini_get_int(ini, group->id, "created", 0);
            if (created < oldest_time) {
                oldest_time = created;
                snprintf(oldest_group, sizeof(oldest_group), "%s", group->id);
            }
        }

        if (count <= RESUME_INDEX_LIMIT || !oldest_group[0]) return;
        mini_delete_group(ini, oldest_group);
    }
}

static void refresh_resume_index(void) {
    if (!core_content_path[0]) return;

    char group[48];
    snprintf(group, sizeof(group), "content_%016llx", (unsigned long long) resume_path_key(core_content_path));

    const char *type;
    const struct gamestate_slot *slot = latest_resume_slot(&type);
    const char index_path[] = RETRO_SHARE_PATH "resume.ini";
    mini_t *ini = mini_try_load(index_path);
    if (!ini) return;

    if (!slot) {
        mini_delete_group(ini, group);
    } else {
        const char *content_name = strrchr(core_content_path, '/');
        content_name = content_name ? content_name + 1 : core_content_path;
        mini_set_int(ini, group, "version", 1);
        mini_set_string(ini, group, "content", core_content_path);
        mini_set_string(ini, group, "content_name", content_name);
        mini_set_string(ini, group, "state", slot->state_path);
        mini_set_string(ini, group, "preview", slot->thumb_path);
        mini_set_string(ini, group, "name", slot->name);
        mini_set_string(ini, group, "type", type);
        mini_set_string(ini, group, "core", slot->core);
        mini_set_string(ini, group, "core_version", slot->core_version);
        mini_set_int(ini, group, "created", slot->created);
    }

    prune_resume_index(ini);
    if (mini_save(ini, 0) != MINI_OK) LOG_WARN(mux_module, "Could not update the resume index");
    mini_free(ini);
}

static void slot_paths(const int index, char *state_path, char *thumb_path) {
    str_format_checked(state_path, MAX_STATE_SIZE, "%s/slot_%d.state", base_dir, index);
    str_format_checked(thumb_path, MAX_STATE_SIZE, "%s/slot_%d.png", base_dir, index);
}

static void timeline_paths(const int slot, char *state_path, char *thumb_path) {
    str_format_checked(state_path, MAX_STATE_SIZE, "%s/timeline_%d.state", base_dir, slot);
    str_format_checked(thumb_path, MAX_STATE_SIZE, "%s/timeline_%d.png", base_dir, slot);
}

static void current_metadata(
    char *crc, const size_t crc_len, char *core, const size_t core_len, char *version, const size_t version_len
) {
    crc[0] = core[0] = version[0] = '\0';

    if (content_hash_is_ready(content_hash_archive)) {
        snprintf(crc, crc_len, "%s", content_hash_get(content_hash_archive));
    }

    if (current_core.retro_get_system_info) {
        struct retro_system_info info = {0};
        current_core.retro_get_system_info(&info);
        if (info.library_name) snprintf(core, core_len, "%s", info.library_name);
        if (info.library_version) snprintf(version, version_len, "%s", info.library_version);
    }
}

static void stamp_current_metadata(struct gamestate_slot *slot) {
    current_metadata(
        slot->crc, sizeof(slot->crc), slot->core, sizeof(slot->core), slot->core_version, sizeof(slot->core_version)
    );
}

static int meta_component_matches(const char *stamped, const char *current) {
    if (!stamped[0] || !current[0]) return 1;
    return strcmp(stamped, current) == 0;
}

int gamestate_metadata_matches(const struct gamestate_slot *slot) {
    char crc[16], core[64], version[64];
    current_metadata(crc, sizeof(crc), core, sizeof(core), version, sizeof(version));

    return meta_component_matches(slot->crc, crc) && meta_component_matches(slot->core, core)
           && meta_component_matches(slot->core_version, version);
}

static int gamestate_resume_matches(const struct gamestate_slot *slot) {
    char crc[16], core[64], version[64];
    current_metadata(crc, sizeof(crc), core, sizeof(core), version, sizeof(version));

    return meta_component_matches(slot->crc, crc) && meta_component_matches(slot->core, core);
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

static void write_manifest_group(
    const char *group_id, const char *name, const long long created, const struct gamestate_slot *meta
) {
    mini_t *ini = mini_try_load(manifest_path);
    if (!ini) ini = mini_create(manifest_path);
    if (!ini) return;

    mini_set_string(ini, group_id, "name", name);
    mini_set_int(ini, group_id, "created", created);

    if (meta) {
        mini_set_string(ini, group_id, "crc", meta->crc);
        mini_set_string(ini, group_id, "core", meta->core);
        mini_set_string(ini, group_id, "core_version", meta->core_version);
    }

    mini_save(ini, 0);
    mini_free(ini);
}

static void
write_manifest_entry(const int index, const char *name, const long long created, const struct gamestate_slot *meta) {
    char group_id[32];
    snprintf(group_id, sizeof(group_id), "slot_%d", index);
    write_manifest_group(group_id, name, created, meta);
}

static void publish_arm(
    const enum publish_kind kind, const int position, const struct gamestate_slot *previous, const int previous_exists
) {
    pending_publish.kind = kind;
    pending_publish.position = position;
    pending_publish.previous_exists = previous_exists;
    if (previous) pending_publish.previous = *previous;
}

static void publish_commit(void) {
    const enum publish_kind kind = pending_publish.kind;
    const int position = pending_publish.position;
    pending_publish.kind = publish_none;

    switch (kind) {
        case publish_slot: {
            const struct gamestate_slot *slot = &gamestate_slots[position];
            write_manifest_entry(slot->index, slot->name, slot->created, slot);
            break;
        }
        case publish_autosave:
            write_manifest_group("autosave", gamestate_autosave.name, gamestate_autosave.created, &gamestate_autosave);
            break;
        case publish_quicksave:
            write_manifest_group(
                "quicksave", gamestate_quicksave.name, gamestate_quicksave.created, &gamestate_quicksave
            );
            break;
        case publish_timeline: {
            const struct gamestate_slot *slot = &gamestate_timeline[position];
            char group_id[32];
            snprintf(group_id, sizeof(group_id), "timeline_%d", position);
            write_manifest_group(group_id, slot->name, slot->created, slot);
            break;
        }
        default:
            return;
    }

    refresh_resume_index();
}

static void publish_rollback(void) {
    const enum publish_kind kind = pending_publish.kind;
    const int position = pending_publish.position;
    pending_publish.kind = publish_none;

    switch (kind) {
        case publish_slot:
            remove(gamestate_slots[position].thumb_path);
            for (int i = position; i < gamestate_slot_count - 1; i++)
                gamestate_slots[i] = gamestate_slots[i + 1];
            gamestate_slot_count--;
            break;
        case publish_autosave:
            gamestate_autosave = pending_publish.previous;
            gamestate_autosave_exists = pending_publish.previous_exists;
            break;
        case publish_quicksave:
            gamestate_quicksave = pending_publish.previous;
            gamestate_quicksave_exists = pending_publish.previous_exists;
            break;
        case publish_timeline:
            gamestate_timeline[position] = pending_publish.previous;
            gamestate_timeline_exists[position] = pending_publish.previous_exists;
            break;
        default:
            return;
    }

    LOG_WARN(mux_module, "Dropped the save entry because its state did not reach storage");
}

void gamestate_publish_task(void) {
    if (pending_publish.kind == publish_none) return;

    int result = 0;
    if (!state_write_poll(&result)) return;

    if (result != 0) {
        publish_rollback();
        return;
    }

    publish_commit();
}

void gamestate_publish_flush(void) {
    if (pending_publish.kind == publish_none) return;

    state_flush();
    gamestate_publish_task();
}

static void read_manifest_meta(mini_t *ini, const char *group_id, struct gamestate_slot *slot) {
    snprintf(slot->crc, sizeof(slot->crc), "%s", get_ini_string(ini, group_id, "crc", ""));
    snprintf(slot->core, sizeof(slot->core), "%s", get_ini_string(ini, group_id, "core", ""));
    snprintf(slot->core_version, sizeof(slot->core_version), "%s", get_ini_string(ini, group_id, "core_version", ""));
}

int gamestate_init(const char *state_dir) {
    if (!str_copy_checked(base_dir, sizeof(base_dir), state_dir)
        || !str_format_checked(manifest_path, sizeof(manifest_path), "%s/states.ini", base_dir)
        || !str_format_checked(pending_path, sizeof(pending_path), "%s/.pending.png", base_dir)
        || !str_format_checked(autosave_state_path, sizeof(autosave_state_path), "%s/autosave.state", base_dir)
        || !str_format_checked(autosave_thumb_path, sizeof(autosave_thumb_path), "%s/autosave.png", base_dir)
        || !str_format_checked(quicksave_state_path, sizeof(quicksave_state_path), "%s/quicksave.state", base_dir)
        || !str_format_checked(quicksave_thumb_path, sizeof(quicksave_thumb_path), "%s/quicksave.png", base_dir)) {
        base_dir[0] = '\0';
        return 0;
    }

    create_directories(manifest_path, 1);

    gamestate_slot_count = 0;
    gamestate_autosave_exists = 0;
    gamestate_autosave_armed = 0;
    gamestate_quicksave_exists = 0;
    pending_publish.kind = publish_none;

    for (int i = 0; i < GAMESTATE_TIMELINE_DEPTH; i++)
        gamestate_timeline_exists[i] = 0;

    mini_t *ini = mini_try_load(manifest_path);
    if (!ini) return 1;

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
            read_manifest_meta(ini, group->id, &gamestate_autosave);
            gamestate_autosave_exists = state_file_timestamp(gamestate_autosave.state_path, NULL);
            if (!gamestate_autosave_exists)
                LOG_WARN(mux_module, "Ignoring stale autosave manifest entry '%s'", gamestate_autosave.state_path);
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
            read_manifest_meta(ini, group->id, &gamestate_quicksave);
            gamestate_quicksave_exists = state_file_timestamp(gamestate_quicksave.state_path, NULL);
            if (!gamestate_quicksave_exists)
                LOG_WARN(mux_module, "Ignoring stale quicksave manifest entry '%s'", gamestate_quicksave.state_path);
            continue;
        }

        int timeline_slot = -1;
        if (sscanf(group->id, "timeline_%d", &timeline_slot) == 1) {
            if (timeline_slot < 0 || timeline_slot >= GAMESTATE_TIMELINE_DEPTH) continue;

            struct gamestate_slot *t = &gamestate_timeline[timeline_slot];
            t->index = timeline_slot;
            snprintf(
                t->name, sizeof(t->name), "%s", get_ini_string(ini, group->id, "name", lang.muxretro.gamestate.timeline)
            );
            t->created = mini_get_int(ini, group->id, "created", 0);
            timeline_paths(timeline_slot, t->state_path, t->thumb_path);
            read_manifest_meta(ini, group->id, t);
            gamestate_timeline_exists[timeline_slot] = state_file_timestamp(t->state_path, NULL);
            if (!gamestate_timeline_exists[timeline_slot])
                LOG_WARN(mux_module, "Ignoring stale timeline manifest entry '%s'", t->state_path);
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
        read_manifest_meta(ini, group->id, slot);

        if (!state_file_timestamp(slot->state_path, NULL)) {
            LOG_WARN(mux_module, "Ignoring stale manual save manifest entry '%s'", slot->state_path);
            continue;
        }

        gamestate_slot_count++;
    }

    mini_free(ini);
    return 1;
}

void gamestate_capture_pending(const int restore_visibility) {
    if (!base_dir[0] || !state_saves_supported()) return;
    pause_menu_store_clean_screenshot(pending_path, restore_visibility);
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
    gamestate_publish_flush();

    const int position = gamestate_slot_count;
    const int index = next_free_index();

    struct gamestate_slot *slot = &gamestate_slots[position];
    slot->index = index;
    slot_paths(index, slot->state_path, slot->thumb_path);

    if (state_save(slot->state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_create: failed to save state to '%s'", slot->state_path);
        return -1;
    }

    image_writer_flush();
    copy_file(pending_path, slot->thumb_path);

    slot->created = (long long) time(NULL);

    if (name && *name) {
        snprintf(slot->name, sizeof(slot->name), "%s", name);
    } else {
        format_epoch(slot->created, slot->name, sizeof(slot->name));
    }

    stamp_current_metadata(slot);

    gamestate_slot_count++;
    publish_arm(publish_slot, position, NULL, 0);
    return position;
}

int gamestate_rename(const int index, const char *new_name) {
    gamestate_publish_flush();
    if (index < 0 || index >= gamestate_slot_count) return -1;

    struct gamestate_slot *slot = &gamestate_slots[index];
    snprintf(slot->name, sizeof(slot->name), "%s", new_name);
    write_manifest_entry(slot->index, slot->name, slot->created, NULL);
    refresh_resume_index();

    return 0;
}

int gamestate_delete(const int index) {
    gamestate_publish_flush();
    if (index < 0 || index >= gamestate_slot_count) return -1;
    if (state_flush() != 0) return -1;

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
    refresh_resume_index();

    return 0;
}

int gamestate_load(const int index) {
    gamestate_publish_flush();
    if (index < 0 || index >= gamestate_slot_count) return -1;
    return state_load(gamestate_slots[index].state_path, 1);
}

int gamestate_autosave_save(void) {
    if (!base_dir[0] || !state_saves_supported() || !gamestate_autosave_armed) {
        if (!gamestate_autosave_armed)
            LOG_INFO(mux_module, "Ignored automatic save before the first post-resume gameplay frame");
        return -1;
    }
    gamestate_publish_flush();

    const struct gamestate_slot previous = gamestate_autosave;
    const int previous_exists = gamestate_autosave_exists;

    if (state_save(autosave_state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_autosave_save: failed to save state to '%s'", autosave_state_path);
        return -1;
    }

    image_writer_flush();
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

    stamp_current_metadata(&gamestate_autosave);

    gamestate_autosave_exists = 1;
    publish_arm(publish_autosave, 0, &previous, previous_exists);
    return 0;
}

void gamestate_autosave_arm(void) {
    if (gamestate_autosave_armed) return;
    gamestate_autosave_armed = 1;
    LOG_DEBUG(mux_module, "Automatic state saves armed after gameplay began");
}

int gamestate_autosave_is_armed(void) {
    return gamestate_autosave_armed;
}

int gamestate_autosave_load(void) {
    if (!gamestate_autosave_exists) return -1;
    return state_load(gamestate_autosave.state_path, 1);
}

int gamestate_autosave_delete(void) {
    gamestate_publish_flush();
    if (!gamestate_autosave_exists) return -1;
    if (state_flush() != 0) return -1;

    remove(gamestate_autosave.state_path);
    remove(gamestate_autosave.thumb_path);

    mini_t *ini = mini_try_load(manifest_path);
    if (ini) {
        mini_delete_group(ini, "autosave");
        mini_save(ini, 0);
        mini_free(ini);
    }

    gamestate_autosave_exists = 0;
    refresh_resume_index();

    return 0;
}

int gamestate_quicksave_save(void) {
    if (!base_dir[0]) return -1;
    gamestate_publish_flush();

    const struct gamestate_slot previous = gamestate_quicksave;
    const int previous_exists = gamestate_quicksave_exists;

    if (state_save(quicksave_state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_quicksave_save: failed to save state to '%s'", quicksave_state_path);
        return -1;
    }

    pause_menu_store_clean_screenshot(quicksave_thumb_path, 1);
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

    stamp_current_metadata(&gamestate_quicksave);

    gamestate_quicksave_exists = 1;
    publish_arm(publish_quicksave, 0, &previous, previous_exists);
    return 0;
}

int gamestate_quicksave_load(void) {
    if (!gamestate_quicksave_exists) return -1;
    return state_load(gamestate_quicksave.state_path, 1);
}

int gamestate_quicksave_delete(void) {
    gamestate_publish_flush();
    if (!gamestate_quicksave_exists) return -1;
    if (state_flush() != 0) return -1;

    remove(gamestate_quicksave.state_path);
    remove(gamestate_quicksave.thumb_path);

    mini_t *ini = mini_try_load(manifest_path);
    if (ini) {
        mini_delete_group(ini, "quicksave");
        mini_save(ini, 0);
        mini_free(ini);
    }

    gamestate_quicksave_exists = 0;
    refresh_resume_index();

    return 0;
}

static int timeline_next_slot(void) {
    int cap = session_settings.timeline_count;
    if (cap < 2) cap = 2;
    if (cap > GAMESTATE_TIMELINE_DEPTH) cap = GAMESTATE_TIMELINE_DEPTH;

    int oldest = 0;
    long long oldest_created = -1;

    for (int i = 0; i < cap; i++) {
        if (!gamestate_timeline_exists[i]) return i;

        if (oldest_created < 0 || gamestate_timeline[i].created < oldest_created) {
            oldest_created = gamestate_timeline[i].created;
            oldest = i;
        }
    }

    return oldest;
}

int gamestate_timeline_save(void) {
    if (!base_dir[0] || !state_saves_supported()) return -1;
    gamestate_publish_flush();

    const int slot = timeline_next_slot();
    struct gamestate_slot *t = &gamestate_timeline[slot];

    const struct gamestate_slot previous = *t;
    const int previous_exists = gamestate_timeline_exists[slot];

    t->index = slot;
    timeline_paths(slot, t->state_path, t->thumb_path);

    if (state_save(t->state_path) != 0) {
        LOG_ERROR(mux_module, "gamestate_timeline_save: failed to save state to '%s'", t->state_path);
        return -1;
    }

    pause_menu_store_clean_screenshot(t->thumb_path, 1);
    t->created = (long long) time(NULL);

    char created_str[32];
    format_epoch(t->created, created_str, sizeof(created_str));
    snprintf(t->name, sizeof(t->name), "%s - %s", lang.muxretro.gamestate.timeline, created_str);

    stamp_current_metadata(t);

    gamestate_timeline_exists[slot] = 1;
    publish_arm(publish_timeline, slot, &previous, previous_exists);
    return 0;
}

int gamestate_timeline_load(const int slot) {
    if (slot < 0 || slot >= GAMESTATE_TIMELINE_DEPTH || !gamestate_timeline_exists[slot]) return -1;
    return state_load(gamestate_timeline[slot].state_path, 1);
}

int gamestate_timeline_delete(const int slot) {
    gamestate_publish_flush();
    if (slot < 0 || slot >= GAMESTATE_TIMELINE_DEPTH || !gamestate_timeline_exists[slot]) return -1;
    if (state_flush() != 0) return -1;

    remove(gamestate_timeline[slot].state_path);
    remove(gamestate_timeline[slot].thumb_path);

    mini_t *ini = mini_try_load(manifest_path);
    if (ini) {
        char group_id[32];
        snprintf(group_id, sizeof(group_id), "timeline_%d", slot);
        mini_delete_group(ini, group_id);
        mini_save(ini, 0);
        mini_free(ini);
    }

    gamestate_timeline_exists[slot] = 0;
    refresh_resume_index();

    return 0;
}

int gamestate_protect_mismatched_autosave(void) {
    gamestate_publish_flush();
    if (!base_dir[0] || !gamestate_autosave_exists) return 0;
    if (gamestate_resume_matches(&gamestate_autosave)) return 0;
    if (gamestate_slot_count >= GAMESTATE_MAX_SLOTS) return 0;
    if (state_flush() != 0) return 0;

    const int position = gamestate_slot_count;
    const int index = next_free_index();

    struct gamestate_slot *slot = &gamestate_slots[position];
    slot->index = index;
    slot_paths(index, slot->state_path, slot->thumb_path);

    if (copy_file(gamestate_autosave.state_path, slot->state_path) != 0) return 0;
    copy_file(gamestate_autosave.thumb_path, slot->thumb_path);

    slot->created = gamestate_autosave.created;

    char created_str[32];
    format_epoch(slot->created, created_str, sizeof(created_str));
    snprintf(slot->name, sizeof(slot->name), "%s - %s", lang.muxretro.gamestate.preserved, created_str);

    snprintf(slot->crc, sizeof(slot->crc), "%s", gamestate_autosave.crc);
    snprintf(slot->core, sizeof(slot->core), "%s", gamestate_autosave.core);
    snprintf(slot->core_version, sizeof(slot->core_version), "%s", gamestate_autosave.core_version);

    write_manifest_entry(index, slot->name, slot->created, slot);
    gamestate_slot_count++;

    gamestate_autosave_delete();

    LOG_INFO(mux_module, "Preserved mismatched autosave as slot %d", index);
    return 1;
}

int gamestate_find_most_recent(char *path, const size_t path_len, int *mismatch_blocked) {
    struct resume_candidate candidates[RESUME_CANDIDATE_LIMIT];
    const int count = collect_resume_candidates(candidates);

    if (mismatch_blocked) *mismatch_blocked = count > 0 && !gamestate_resume_matches(candidates[0].slot);

    if (!path || path_len == 0) return -1;
    for (int i = 0; i < count; i++) {
        if (!gamestate_resume_matches(candidates[i].slot)) continue;
        snprintf(path, path_len, "%s", candidates[i].slot->state_path);
        return 0;
    }
    return -1;
}

int gamestate_load_most_recent(int *mismatch_blocked, const int show_message) {
    struct resume_candidate candidates[RESUME_CANDIDATE_LIMIT];
    const int count = collect_resume_candidates(candidates);

    if (mismatch_blocked) *mismatch_blocked = count > 0 && !gamestate_resume_matches(candidates[0].slot);

    for (int i = 0; i < count; i++) {
        const struct gamestate_slot *slot = candidates[i].slot;
        if (!gamestate_resume_matches(slot)) continue;
        if (state_load(slot->state_path, show_message) == 0) return 0;
        LOG_WARN(mux_module, "Could not restore '%s'; trying the next most recent compatible state", slot->state_path);
    }
    return -1;
}
