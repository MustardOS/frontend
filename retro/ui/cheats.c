#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/core.h"
#include "../core/paths.h"
#include "cheats.h"

struct cheat_entry cheats_list[CHEAT_MAX];
int cheats_count = 0;

static char cheats_path[MAX_BUFFER_SIZE] = "";

static int write_cheats(void);

static int cheat_index_from_group_id(const char *group_id) {
    int index = -1;
    if (sscanf(group_id, "cheat_%d", &index) != 1) return -1;
    return index;
}

static void apply_enabled_cheats(void) {
    if (current_core.retro_cheat_reset) current_core.retro_cheat_reset();
    if (!current_core.retro_cheat_set) return;

    unsigned applied = 0;
    for (int i = 0; i < cheats_count; i++) {
        if (!cheats_list[i].enabled) continue;
        current_core.retro_cheat_set(applied++, true, cheats_list[i].code);
    }
}

static void trim_value(char *s) {
    char *start = s;
    while (*start == ' ' || *start == '\t')
        start++;

    size_t len = strlen(start);
    while (len > 0
           && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\r' || start[len - 1] == '\n')) {
        start[--len] = '\0';
    }

    if (len >= 2 && start[0] == '"' && start[len - 1] == '"') {
        start[len - 1] = '\0';
        start++;
    }

    if (start != s) memmove(s, start, strlen(start) + 1);
}

static void html_unescape(char *s) {
    char *out = s;

    for (const char *p = s; *p;) {
        if (*p != '&') {
            *out++ = *p++;
            continue;
        }

        if (strncmp(p, "&quot;", 6) == 0) {
            *out++ = '"';
            p += 6;
        } else if (strncmp(p, "&apos;", 6) == 0) {
            *out++ = '\'';
            p += 6;
        } else if (strncmp(p, "&amp;", 5) == 0) {
            *out++ = '&';
            p += 5;
        } else if (strncmp(p, "&lt;", 4) == 0) {
            *out++ = '<';
            p += 4;
        } else if (strncmp(p, "&gt;", 4) == 0) {
            *out++ = '>';
            p += 4;
        } else if (p[1] == '#') {
            const char *num = p + 2;
            const int base = (*num == 'x' || *num == 'X') ? (num++, 16) : 10;

            char *end = NULL;
            const long code = strtol(num, &end, base);

            if (end && *end == ';' && code > 0 && code < 128) {
                *out++ = (char) code;
                p = end + 1;
            } else {
                *out++ = *p++;
            }
        } else {
            *out++ = *p++;
        }
    }

    *out = '\0';
}

static int cheat_code_exists(const char *code) {
    for (int i = 0; i < cheats_count; i++) {
        if (strcmp(cheats_list[i].code, code) == 0) return 1;
    }

    return 0;
}

static int load_ra_cht(const char *save_prefix, const char *content_stem) {
    char cht_path[MAX_BUFFER_SIZE];
    snprintf(cht_path, sizeof(cht_path), "%s/%s/%s.cht", RETRO_CHT_PATH, save_prefix, content_stem);

    FILE *f = fopen(cht_path, "r");
    if (!f) return 0;

    static char cht_desc[CHEAT_MAX][CHEAT_DESC_MAX];
    static char cht_code[CHEAT_MAX][CHEAT_CODE_MAX];
    static int cht_enable[CHEAT_MAX];
    static int cht_have[CHEAT_MAX] = {0};

    memset(cht_enable, 0, sizeof(cht_enable));
    for (int i = 0; i < CHEAT_MAX; i++) {
        cht_desc[i][0] = '\0';
        cht_code[i][0] = '\0';
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        trim_value(key);
        trim_value(value);
        html_unescape(value);

        int index = -1;
        char field[16];
        if (sscanf(key, "cheat%d_%15s", &index, field) != 2) continue;
        if (index < 0 || index >= CHEAT_MAX) continue;

        if (strcmp(field, "desc") == 0) {
            snprintf(cht_desc[index], sizeof(cht_desc[index]), "%s", value);
        } else if (strcmp(field, "code") == 0) {
            snprintf(cht_code[index], sizeof(cht_code[index]), "%s", value);
            cht_have[index] = 1;
        } else if (strcmp(field, "enable") == 0) {
            cht_enable[index] = strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0;
        }
    }

    fclose(f);

    int parsed = 0;
    for (int i = 0; i < CHEAT_MAX; i++) {
        if (!cht_have[i] || !cht_code[i][0]) continue;
        parsed++;

        if (cheats_count >= CHEAT_MAX || cheat_code_exists(cht_code[i])) continue;

        struct cheat_entry *entry = &cheats_list[cheats_count];
        snprintf(entry->desc, sizeof(entry->desc), "%s", cht_desc[i][0] ? cht_desc[i] : cht_code[i]);
        snprintf(entry->code, sizeof(entry->code), "%s", cht_code[i]);
        entry->enabled = cht_enable[i];

        cheats_count++;
    }

    if (parsed > 0) LOG_INFO(mux_module, "Merged %d cheat(s) from '%s'", parsed, cht_path);
    return parsed;
}

void cheats_init(const char *core_path_arg, const char *content_path) {
    cheats_path[0] = '\0';
    cheats_count = 0;

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", content_base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix));

    snprintf(cheats_path, sizeof(cheats_path), "%s/%s/%s.ini", RETRO_CHT_PATH, save_prefix, content_stem);

    mini_t *ini = mini_try_load(cheats_path);
    if (ini) {
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
    }

    const int converted = load_ra_cht(save_prefix, content_stem);

    if (converted > 0 && write_cheats()) {
        char cht_path[MAX_BUFFER_SIZE];
        snprintf(cht_path, sizeof(cht_path), "%s/%s/%s.cht", RETRO_CHT_PATH, save_prefix, content_stem);
        if (remove(cht_path) == 0) LOG_INFO(mux_module, "Converted and removed '%s'", cht_path);
    }

    if (cheats_count == 0) return;

    LOG_INFO(mux_module, "Loaded %d cheat(s) from '%s'", cheats_count, cheats_path);
    apply_enabled_cheats();
}

static int write_cheats(void) {
    if (!cheats_path[0]) return 0;

    mini_t *ini = mini_try_load(cheats_path);
    if (!ini) ini = mini_create(cheats_path);
    if (!ini) return 0;

    mini_set_bool(ini, "meta", "managed", 1);

    for (int i = 0; i < cheats_count; i++) {
        char group_id[32];
        snprintf(group_id, sizeof(group_id), "cheat_%d", i);

        mini_set_string(ini, group_id, "desc", cheats_list[i].desc);
        mini_set_string(ini, group_id, "code", cheats_list[i].code);
        mini_set_bool(ini, group_id, "enabled", cheats_list[i].enabled);
    }

    create_directories(cheats_path, 1);
    const int ok = mini_save(ini, 0) == MINI_OK;
    mini_free(ini);

    return ok;
}

void cheats_toggle(const int index) {
    if (index < 0 || index >= cheats_count) return;

    cheats_list[index].enabled = !cheats_list[index].enabled;

    apply_enabled_cheats();
    write_cheats();
}
