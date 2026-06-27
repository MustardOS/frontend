#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include "log.h"
#include "fileio.h"
#include "language.h"
#include "union.h"
#include "input.h"
#include "../module/muxshare.h"

char **history_items = NULL;
int history_item_count = 0;

char **collection_items = NULL;
int collection_item_count = 0;

typedef struct {
    const char **slots;
    int cap;
} path_set_t;

static path_set_t collection_set;
static path_set_t history_set;

static uint32_t path_hash(const char *s) {
    uint32_t h = 2166136261U;
    for (; *s; s++) {
        h ^= (uint8_t) *s;
        h *= 16777619U;
    }

    return h;
}

static void path_set_build(path_set_t *s, char **items, const int count) {
    free(s->slots);

    s->slots = NULL;
    s->cap = 0;

    if (count <= 0) return;

    int cap = 16;
    while (cap < count * 2)
        cap <<= 1;

    s->slots = calloc((size_t) cap, sizeof(const char *));

    if (!s->slots) return;
    s->cap = cap;

    for (int i = 0; i < count; i++) {
        int h = (int) (path_hash(items[i]) & (uint32_t) (cap - 1));
        while (s->slots[h])
            h = (h + 1) & (cap - 1);
        s->slots[h] = items[i];
    }
}

static int path_set_contains(const path_set_t *s, const char *path) {
    if (!s->slots) return 0;

    int h = (int) (path_hash(path) & (uint32_t) (s->cap - 1));
    while (s->slots[h]) {
        if (strcmp(s->slots[h], path) == 0) return 1;
        h = (h + 1) & (s->cap - 1);
    }

    return 0;
}

static void populate_items(const char *base_path, char ***items, int *item_count, int *cap) {
    struct dirent *entry;
    char full_path[PATH_MAX];
    DIR *dir = opendir(base_path);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return;
    }

    const int dfd = dirfd(dir);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
            LOG_ERROR(mux_module, "%s", lang.system.fail_stat);
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            if (strstr(entry->d_name, ".cfg")) {
                if (*item_count >= *cap) {
                    const int new_cap = *cap ? *cap * 2 : 16;
                    char **tmp = realloc(*items, (size_t) new_cap * sizeof(char *));

                    if (!tmp) continue;

                    *items = tmp;
                    *cap = new_cap;
                }

                char *raw = read_line_char_from(full_path, 1);
                (*items)[*item_count] = strdup(raw);
                free(raw);
                (*item_count)++;
            }
        } else if (S_ISDIR(st.st_mode)) {
            populate_items(full_path, items, item_count, cap);
        }
    }

    closedir(dir);
}

static int path_uses_union(const char *path) {
    return path && strncmp(path, "/mnt/union", 10) == 0;
}

static int content_loader_needs_rebuild(
    const char *content_loader_file, const char *content_name, const char *core, const char *sys1, const char *sys2,
    const char *launch, const char *roms_path, const char *system_sub, const char *file_name
) {
    if (!file_exist(content_loader_file)) return 1;

    char *line1 = read_line_char_from(content_loader_file, 1);
    char *line2 = read_line_char_from(content_loader_file, 2);
    char *line3 = read_line_char_from(content_loader_file, 3);
    char *line4 = read_line_char_from(content_loader_file, 4);
    char *line5 = read_line_char_from(content_loader_file, 5);
    char *line6 = read_line_char_from(content_loader_file, 6);
    char *line7 = read_line_char_from(content_loader_file, 7);
    char *line8 = read_line_char_from(content_loader_file, 8);
    char *line9 = read_line_char_from(content_loader_file, 9);

    int rebuild = 0;

    if (!line1 || !*line1 || !line2 || !*line2 || !line3 || !*line3 || !line4 || !*line4 || !line5 || !*line5 || !line6
        || !*line6 || !line7 || !*line7 || !line8 || !*line8 || !line9 || !*line9) {
        rebuild = 1;
        goto done;
    }

    if (path_uses_union(line7) || path_uses_union(line8) || path_uses_union(line9)) {
        rebuild = 1;
        goto done;
    }

    if (strcmp(line1, content_name) != 0 || strcmp(line2, core) != 0 || strcmp(line3, sys1) != 0
        || strcmp(line4, sys2) != 0 || strcmp(line6, launch) != 0 || strcmp(line7, roms_path) != 0
        || strcmp(line8, system_sub) != 0 || strcmp(line9, file_name) != 0) {
        rebuild = 1;
    }

done:
    free(line1);
    free(line2);
    free(line3);
    free(line4);
    free(line5);
    free(line6);
    free(line7);
    free(line8);
    free(line9);

    return rebuild;
}

static int write_content_loader_file(
    const char *content_loader_file, const char *content_name, const char *core, const char *sys1, const char *sys2,
    const char *zero, const char *launch, const char *roms_path, const char *system_sub, const char *file_name
) {
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", content_loader_file) >= (int) sizeof(tmp)) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, content_loader_file);
        return 0;
    }

    FILE *fp = fopen(tmp, "w");
    if (!fp) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, content_loader_file);
        return 0;
    }

    fprintf(fp, "%s\n", content_name);
    fprintf(fp, "%s\n", core);
    fprintf(fp, "%s\n", sys1);
    fprintf(fp, "%s\n", sys2);
    fprintf(fp, "%s\n", *zero ? zero : "0");
    fprintf(fp, "%s\n", launch);
    fprintf(fp, "%s\n", roms_path);
    fprintf(fp, "%s\n", system_sub);
    fprintf(fp, "%s\n", file_name);

    const int ok = fflush(fp) == 0 && fsync(fileno(fp)) == 0;
    fclose(fp);

    if (!ok || rename(tmp, content_loader_file) != 0) {
        remove(tmp);
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, content_loader_file);
        return 0;
    }

    return 1;
}

char *get_content_line(char *dir, const char *name, char *ext, const size_t line) {
    static char path[MAX_BUFFER_SIZE];
    char *subdir = get_last_subdir(dir, '/', 4);

    if (name == NULL) {
        snprintf(path, sizeof(path), INFO_CON_PATH "/%s/core.%s", subdir, ext);
    } else {
        snprintf(path, sizeof(path), INFO_CON_PATH "/%s/%s.%s", subdir, strip_ext(name), ext);
    }

    remove_double_slashes(path);
    if (file_exist(path)) return read_line_char_from(path, line);

    return "";
}

char *get_application_line(char *dir, char *ext, const size_t line) {
    static char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/mux_option.%s", dir, ext);

    if (file_exist(path)) return read_line_char_from(path, line);

    return "";
}

void populate_history_items(void) {
    int cap = 0;

    populate_items(INFO_HIS_PATH, &history_items, &history_item_count, &cap);
    path_set_build(&history_set, history_items, history_item_count);
}

void populate_collection_items(void) {
    int cap = 0;

    populate_items(INFO_COL_PATH, &collection_items, &collection_item_count, &cap);
    path_set_build(&collection_set, collection_items, collection_item_count);
}

char *get_content_explorer_glyph_name(const char *file_path) {
    if (config.visual.content_collect == 0 && path_set_contains(&collection_set, file_path)) return "collection";
    if (config.visual.content_history == 0 && path_set_contains(&history_set, file_path)) return "history";

    return "rom";
}

uint32_t fnv_hash_str(const char *str) {
    uint32_t hash = 2166136261U; // FNV offset basis

    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t) *p;
        hash *= 16777619; // FNV prime
    }

    return hash;
}

uint32_t fnv_hash_file(FILE *fp) {
    uint32_t hash = 2166136261U; // FNV offset basis
    unsigned char buf[65535];
    size_t n;

    while ((n = fread(buf, 1, sizeof buf, fp)) > 0) {
        for (size_t i = 0; i < n; i++) {
            hash ^= buf[i];
            hash *= 16777619; // FNV prime
        }
    }

    if (ferror(fp)) return 0;
    return hash;
}

int load_content(int add_collection, char *file_path) {
    char resolved_path[PATH_MAX];

    if (!union_resolve_to_real(file_path, resolved_path, sizeof(resolved_path)) || !file_exist(resolved_path)) {

        play_sound(snd_error);
        toast_message(lang.generic.no_load, tst_wait_m);

        LOG_ERROR(mux_module, "Could not launch content: %s", resolved_path[0] ? resolved_path : file_path);
        return 0;
    }

    file_path = resolved_path;

    char *assigned_core = load_content_core(0, !add_collection, file_path);
    if (!assigned_core || strcasestr(assigned_core, "(null)")) {
        free(assigned_core);
        return 0;
    }

    char *content_path = get_content_path(file_path);
    char *file_name = get_file_name(file_path);
    char *content_name = strip_ext(file_name);
    char *item_dir = strip_dir(file_path);

    char cfg_sub[PATH_MAX];
    char system_sub[PATH_MAX];
    char mount_path[PATH_MAX];

    union_get_relative_path(content_path, cfg_sub, sizeof(cfg_sub));
    union_get_relative_path(item_dir, system_sub, sizeof(system_sub));
    union_get_mount_path(file_path, mount_path, sizeof(mount_path));

    if (strncasecmp(cfg_sub, MAIN_ROM_DIR, 4) == 0) {
        char *p = cfg_sub + 4;
        while (*p == '/')
            p++;
        memmove(cfg_sub, p, strlen(p) + 1);
    }

    if (strncasecmp(system_sub, MAIN_ROM_DIR, 4) == 0) {
        char *p = system_sub + 4;
        while (*p == '/')
            p++;
        memmove(system_sub, p, strlen(p) + 1);
    }

    char content_loader_file[MAX_BUFFER_SIZE];
    snprintf(content_loader_file, sizeof(content_loader_file), INFO_CON_PATH "/%s/%s.cfg", cfg_sub, content_name);

    remove_double_slashes(content_loader_file);
    LOG_INFO(mux_module, "Configuration File: %s", content_loader_file);

    char core[MAX_BUFFER_SIZE] = {0};
    char sys1[MAX_BUFFER_SIZE] = {0};
    char sys2[MAX_BUFFER_SIZE] = {0};
    char zero[MAX_BUFFER_SIZE] = {0};
    char launch[MAX_BUFFER_SIZE] = {0};

    sscanf(assigned_core, "%1023[^\n]\n%1023[^\n]\n%1023[^\n]\n%1023[^\n]\n%1023[^\n]", core, sys1, sys2, zero, launch);

    char roms_path[PATH_MAX];
    snprintf(roms_path, sizeof(roms_path), "%s/ROMS/", mount_path);
    remove_double_slashes(roms_path);

    int rebuild_cfg = content_loader_needs_rebuild(
        content_loader_file, content_name, core, sys1, sys2, launch, roms_path, system_sub, file_name
    );
    if (rebuild_cfg) {
        if (!write_content_loader_file(
                content_loader_file, content_name, core, sys1, sys2, zero, launch, roms_path, system_sub, file_name
            )) {
            free(content_name);
            free(item_dir);
            free(assigned_core);
            return 0;
        }

        LOG_INFO(mux_module, "Rebuilt Content Loader: %s", content_loader_file);
    }

    if (!file_exist(content_loader_file)) {
        free(content_name);
        free(item_dir);
        free(assigned_core);
        return 0;
    }

    LOG_INFO(mux_module, "Using Configuration: %s", content_loader_file);

    if (add_collection) {
        add_to_collection(file_name, content_loader_file, content_path);
    } else {
        LOG_INFO(mux_module, "Assigned System: %s", sys1);
        LOG_INFO(mux_module, "Assigned Core: %s", core);

        char *assigned_gov =
            specify_asset(load_content_governor(content_path, content_name, 0, 1, 0), device.cpu.dflt, "Governor");
        char *assigned_con =
            specify_asset(load_content_control_scheme(content_path, content_name, 0, 1, 0), "system", "Control Scheme");
        char *assigned_rac =
            specify_asset(load_content_retroarch(content_path, content_name, 0, 1, 0), "false", "RetroArch Config");
        char *assigned_flt =
            specify_asset(load_content_filter(content_path, content_name, 0, 1, 0), "none", "Colour Filter");
        char *assigned_shd = specify_asset(load_content_shader(content_path, content_name, 0, 1, 0), "none", "Shader");

        unsigned int new_hash = fnv_hash_str(file_path);
        char new_history[PATH_MAX];
        snprintf(new_history, sizeof(new_history), INFO_HIS_PATH "/%s-%08X.cfg", content_name, new_hash);

        DIR *d = opendir(INFO_HIS_PATH);
        if (d) {
            struct dirent *ent;

            while ((ent = readdir(d))) {
                if (ent->d_type != DT_REG) continue;
                if (!strstr(ent->d_name, content_name)) continue;

                char old_file[PATH_MAX];
                snprintf(old_file, sizeof(old_file), "%s/%s", INFO_HIS_PATH, ent->d_name);

                if (!file_exist(old_file)) continue;
                if (strcmp(old_file, new_history) == 0) continue;

                char *line1 = read_line_char_from(old_file, 1);
                if (!*line1) continue;

                char resolved_old[PATH_MAX];
                if (union_resolve_to_real(line1, resolved_old, sizeof(resolved_old))
                    && strcmp(resolved_old, file_path) == 0) {

                    if (file_exist(new_history)) {
                        remove(old_file);
                    } else {
                        rename(old_file, new_history);
                    }

                    free(line1);
                    continue;
                }

                free(line1);
            }

            closedir(d);
        }

        char pointer[MAX_BUFFER_SIZE];
        snprintf(pointer, sizeof(pointer), "%s\n%s\n%s", file_path, system_sub, content_name);

        write_text_to_file(new_history, "w", CHAR, pointer);

        write_text_to_file(LAST_PLAY_FILE, "w", CHAR, content_loader_file);
        write_text_to_file(MUOS_GOV_LOAD, "w", CHAR, assigned_gov);
        write_text_to_file(MUOS_CON_LOAD, "w", CHAR, assigned_con);
        write_text_to_file(MUOS_RAC_LOAD, "w", CHAR, assigned_rac);
        write_text_to_file(MUOS_FLT_LOAD, "w", CHAR, assigned_flt);
        write_text_to_file(MUOS_SHD_LOAD, "w", CHAR, assigned_shd);

        char *loader_text = read_all_char_from(content_loader_file);
        write_text_to_file(MUOS_ROM_LOAD, "w", CHAR, loader_text);
        free(loader_text);
    }

    LOG_SUCCESS(mux_module, "Content Loaded Successfully");

    free(content_name);
    free(item_dir);
    free(assigned_core);
    return 1;
}

char *load_content_core(const int force, const int run_quit, char *file_path) {
    char resolved_path[PATH_MAX];

    if (!union_resolve_to_real(file_path, resolved_path, sizeof(resolved_path))) {
        if (run_quit) mux_input_stop();
        LOG_ERROR(mux_module, "Failed to resolve content path: %s", file_path);
        return NULL;
    }

    file_path = resolved_path;

    char *sys_dir = get_content_path(file_path);
    const char *file_name = get_file_name(file_path);
    char *content_name = strip_ext(file_name);

    char rel_sys_dir[PATH_MAX];
    union_get_relative_path(sys_dir, rel_sys_dir, sizeof(rel_sys_dir));

    if (strncasecmp(rel_sys_dir, MAIN_ROM_DIR, 4) == 0) {
        const char *p = rel_sys_dir + 4;
        while (*p == '/')
            p++;
        memmove(rel_sys_dir, p, strlen(p) + 1);
    }

    char core_cfg[MAX_BUFFER_SIZE];
    snprintf(core_cfg, sizeof(core_cfg), INFO_CON_PATH "/%s/%s.cfg", rel_sys_dir, content_name);

    remove_double_slashes(core_cfg);

    if (file_exist(core_cfg) && !force) {
        LOG_SUCCESS(mux_module, "Loading Content Core: %s", core_cfg);

        char *core =
            build_core(core_cfg, content_core, content_system, content_catalogue, content_lookup, content_assign);
        if (core) {
            free(content_name);
            free(sys_dir);
            return core;
        }
    }

    snprintf(core_cfg, sizeof(core_cfg), INFO_CON_PATH "/%s/core.cfg", rel_sys_dir);

    if (file_exist(core_cfg) && !force) {
        LOG_SUCCESS(mux_module, "Loading Global Core: %s", core_cfg);

        char *core = build_core(core_cfg, global_core, global_system, global_catalogue, global_lookup, global_assign);
        if (core) {
            free(content_name);
            free(sys_dir);
            return core;
        }

        LOG_ERROR(mux_module, "Failed to build Global Core: %s", core_cfg);
    }

    load_assign(MUOS_ASS_LOAD, file_name, sys_dir, "none", force, 0);
    if (run_quit) mux_input_stop();

    LOG_INFO(mux_module, "No core detected");

    free(content_name);
    free(sys_dir);
    return NULL;
}

char *build_core(
    char core_path[MAX_BUFFER_SIZE], const int line_core, const int line_system, const int line_catalogue,
    const int line_lookup, const int line_launch
) {
    char *core_line = read_line_char_from(core_path, line_core);
    char *system_line = read_line_char_from(core_path, line_system);
    char *catalogue_line = read_line_char_from(core_path, line_catalogue);
    char *lookup_line = read_line_char_from(core_path, line_lookup);
    char *launch_line = read_line_char_from(core_path, line_launch);

    const char *core_val = core_line && *core_line ? core_line : "unknown";
    const char *system_val = system_line && *system_line ? system_line : "unknown";
    const char *catalogue_val = catalogue_line && *catalogue_line ? catalogue_line : "unknown";
    const char *lookup_val = lookup_line && *lookup_line ? lookup_line : "unknown";
    const char *launch_val = launch_line && *launch_line ? launch_line : "unknown";

    const size_t required_size =
        snprintf(NULL, 0, "%s\n%s\n%s\n%s\n%s", core_val, system_val, catalogue_val, lookup_val, launch_val) + 1;

    char *b_core = malloc(required_size);
    if (!b_core) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        free(core_line);
        free(system_line);
        free(catalogue_line);
        free(lookup_line);
        free(launch_line);
        return NULL;
    }

    snprintf(b_core, required_size, "%s\n%s\n%s\n%s\n%s", core_val, system_val, catalogue_val, lookup_val, launch_val);

    free(core_line);
    free(system_line);
    free(catalogue_line);
    free(lookup_line);
    free(launch_line);

    return b_core;
}

void rewrite_launch_file(const char *file, const char *new_path) {
    if (!file_exist(file)) return;

    char *line1 = read_line_char_from(file, 1);
    char *line2 = read_line_char_from(file, 2);
    char *line3 = read_line_char_from(file, 3);

    if (!*line1 || strcmp(line1, new_path) == 0) {
        free(line1);
        free(line2);
        free(line3);
        return;
    }

    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", file) >= (int) sizeof(tmp)) {
        free(line1);
        free(line2);
        free(line3);
        return;
    }

    FILE *fp = fopen(tmp, "w");
    if (!fp) {
        free(line1);
        free(line2);
        free(line3);
        return;
    }

    fprintf(fp, "%s\n%s\n%s", new_path, line2, line3);
    const int ok = fflush(fp) == 0 && fsync(fileno(fp)) == 0;
    fclose(fp);

    if (!ok || rename(tmp, file) != 0) remove(tmp);

    free(line1);
    free(line2);
    free(line3);
}

void migrate_history_entry(const char *old_file, const char *new_path, const char *content_name) {
    if (!old_file || !new_path || !content_name) return;
    if (!file_exist(old_file)) return;

    const unsigned int new_hash = fnv_hash_str(new_path);

    char new_file[PATH_MAX];
    snprintf(new_file, sizeof(new_file), INFO_HIS_PATH "/%s-%08X.cfg", content_name, new_hash);

    if (strcmp(old_file, new_file) == 0) return;

    if (file_exist(new_file)) {
        remove(old_file);
        return;
    }

    rename(old_file, new_file);
}

void check_collection(const char *col_file) {
    if (!col_file || !*col_file) return;

    FILE *new_fp = fopen(col_file, "r");
    if (!new_fp) return;

    const uint32_t new_hash = fnv_hash_file(new_fp);
    fclose(new_fp);

    if (new_hash == 0) return;

    const char *new_col_file = strrchr(col_file, '/');
    new_col_file = new_col_file ? new_col_file + 1 : col_file;

    char base_name[PATH_MAX];
    snprintf(base_name, sizeof(base_name), "%s", new_col_file);

    char *dash = strrchr(base_name, '-');
    if (dash) *dash = '\0';

    DIR *col_dir = opendir(INFO_COL_PATH);
    if (!col_dir) return;

    struct dirent *ent;
    while ((ent = readdir(col_dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!strstr(ent->d_name, ".cfg")) continue;

        // Only look at the files sharing the same base name
        const size_t prefix_len = strlen(base_name);
        if (strncmp(ent->d_name, base_name, prefix_len) != 0) continue;
        if (ent->d_name[prefix_len] != '-') continue;

        char cfg_path[PATH_MAX];
        snprintf(cfg_path, sizeof(cfg_path), "%s/%s", INFO_COL_PATH, ent->d_name);

        // Do NOT fucking delete the file we just wrote...
        if (strcmp(cfg_path, col_file) == 0) continue;

        FILE *fp = fopen(cfg_path, "r");
        if (!fp) continue;

        // Compare the hash, that's good hash!
        const uint32_t existing_hash = fnv_hash_file(fp);
        fclose(fp);

        if (existing_hash == new_hash) {
            LOG_INFO(mux_module, "Removing duplicate collection entry: %s", cfg_path);
            remove(cfg_path);
        }
    }

    closedir(col_dir);
}

void add_to_collection(char *filename, const char *pointer, char *sys_dir) {
    play_sound(snd_confirm);

    char new_content[MAX_BUFFER_SIZE];
    snprintf(new_content, sizeof(new_content), "%s\n%s\n%s", filename, pointer, get_last_subdir(sys_dir, '/', 4));

    write_text_to_file(ADD_MODE_WORK, "w", CHAR, new_content);
    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    load_mux("collection");

    mux_input_stop();
}
