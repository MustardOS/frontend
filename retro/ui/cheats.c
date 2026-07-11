#include <stdio.h>
#include <string.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "cheats.h"
#include "../core/core.h"
#include "../core/paths.h"

struct cheat_entry cheats_list[CHEAT_MAX];
int cheats_count = 0;

static char cheats_path[MAX_BUFFER_SIZE] = "";

static int cheat_index_from_group_id(const char *group_id) {
    int index = -1;
    if (sscanf(group_id, "cheat_%d", &index) != 1) return -1;
    return index;
}

static void apply_enabled_cheats(void) {
    if (current_core.retro_cheat_reset) current_core.retro_cheat_reset();
    if (!current_core.retro_cheat_set) return;

    for (int i = 0; i < cheats_count; i++) {
        if (cheats_list[i].enabled) current_core.retro_cheat_set((unsigned) i, true, cheats_list[i].code);
    }
}

void cheats_init(const char *core_path_arg, const char *content_path) {
    cheats_path[0] = '\0';
    cheats_count = 0;

    char core_name[MAX_BUFFER_SIZE];
    core_get_name(core_path_arg, core_name, sizeof(core_name));

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", content_base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    snprintf(cheats_path, sizeof(cheats_path), "%s/%s/%s.ini", RETRO_CHT_PATH, core_name, content_stem);

    mini_t *ini = mini_try_load(cheats_path);
    if (!ini) return;

    const int managed = (int) mini_get_bool(ini, "meta", "managed", 0);

    for (const mini_group_t *group = ini->head; group && cheats_count < CHEAT_MAX; group = group->next) {
        if (!group->id) continue;

        const int index = cheat_index_from_group_id(group->id);
        if (index < 0) continue;

        struct cheat_entry *entry = &cheats_list[cheats_count];
        snprintf(entry->desc, sizeof(entry->desc), "%s", get_ini_string(ini, group->id, "desc", ""));
        snprintf(entry->code, sizeof(entry->code), "%s", get_ini_string(ini, group->id, "code", ""));
        entry->enabled = managed ? (int) mini_get_bool(ini, group->id, "enabled", 0) : 0;

        cheats_count++;
    }

    mini_free(ini);

    LOG_INFO(mux_module, "Loaded %d cheat(s) from '%s'", cheats_count, cheats_path);

    apply_enabled_cheats();
}

static void write_cheats(void) {
    if (!cheats_path[0]) return;

    mini_t *ini = mini_try_load(cheats_path);
    if (!ini) ini = mini_create(cheats_path);
    if (!ini) return;

    mini_set_bool(ini, "meta", "managed", 1);

    for (int i = 0; i < cheats_count; i++) {
        char group_id[32];
        snprintf(group_id, sizeof(group_id), "cheat_%d", i);

        mini_set_string(ini, group_id, "desc", cheats_list[i].desc);
        mini_set_string(ini, group_id, "code", cheats_list[i].code);
        mini_set_bool(ini, group_id, "enabled", cheats_list[i].enabled);
    }

    create_directories(cheats_path, 1);
    mini_save(ini, 0);
    mini_free(ini);
}

void cheats_toggle(const int index) {
    if (index < 0 || index >= cheats_count) return;

    cheats_list[index].enabled = !cheats_list[index].enabled;

    if (current_core.retro_cheat_set) {
        current_core.retro_cheat_set((unsigned) index, cheats_list[index].enabled, cheats_list[index].code);
    }

    write_cheats();
}
