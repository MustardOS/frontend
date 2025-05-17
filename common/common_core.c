#include <dirent.h>
#include "miniz/miniz.h"
#include "json/json.h"
#include "common.h"
#include "common_core.h"
#include "device.h"
#include "language.h"
#include "log.h"
#include "mini/mini.h"

void get_catalogue_name(char *sys_dir, char *content_label, char *catalogue_name, size_t catalogue_name_size) {
    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s/%s.cfg",
             INFO_COR_PATH, get_last_subdir(sys_dir, '/', 4), strip_ext(content_label));

    if (!file_exist(core_file)) {
        snprintf(core_file, sizeof(core_file), "%s/%s/core.cfg",
                 INFO_COR_PATH, get_last_subdir(sys_dir, '/', 4));
        snprintf(catalogue_name, catalogue_name_size, "%s",
                 read_line_char_from(core_file, 2));
    } else {
        snprintf(catalogue_name, catalogue_name_size, "%s",
                 read_line_char_from(core_file, 3));
    }

    LOG_INFO(mux_module, "Reading Configuration: %s", core_file)
}

char *get_catalogue_name_from_rom_path(char *sys_dir, char *content_label) {
    char rom_dir[MAX_BUFFER_SIZE];
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", sys_dir, content_label);
    return get_directory_core(rom_dir, 2);
}

void free_lines(char *lines[], int line_count) {
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
}

void modify_cfg_file(const char *filename, const char *core, const char *sys, const char *cache) {
    printf("Updating file: %s\n", filename);
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    int max_lines = 10;
    char *lines[max_lines];
    int line_count = 0;

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        if (line_count >= max_lines) {
            fprintf(stderr, "File too large, exceeding max lines limit\n");
            fclose(file);
            return;
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        lines[line_count] = strdup(line);
        line_count++;
    }
    fclose(file);

    if (line_count >= 2) {
        free(lines[1]);
        lines[1] = strdup(core);
    }
    if (line_count >= 3) {
        free(lines[2]);
        lines[2] = strdup(sys);
    }
    if (line_count >= 4) {
        free(lines[3]);
        lines[3] = strdup(cache);
    }

    if (remove(filename) != 0) {
        perror(lang.SYSTEM.FAIL_DELETE_FILE);
        free_lines(lines, line_count);
        return;
    }

    FILE *new_file = fopen(filename, "w");
    if (!new_file) {
        perror(lang.SYSTEM.FAIL_CREATE_FILE);
        free_lines(lines, line_count);
        return;
    }

    for (int i = 0; i < line_count; i++) {
        fprintf(new_file, "%s\n", lines[i]);
    }
    free_lines(lines, line_count);

    fclose(new_file);
}

// Function to scan the directory and find .cfg files
void update_cfg_files(const char *dirpath, const char *core, const char *sys, const int cache) {
    char cache_char[MAX_BUFFER_SIZE];
    snprintf(cache_char, sizeof(cache_char), "%d", cache);

    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".cfg") && strcmp(entry->d_name, "core.cfg") != 0) {
            char filepath[PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            modify_cfg_file(filepath, core, sys, cache_char);
        }
    }

    closedir(dir);
}

void assign_core_single(char *rom_dir, char *core_dir, const char *core, char *sys, char *rom, int cache) {
    char rom_path[MAX_BUFFER_SIZE];
    snprintf(rom_path, sizeof(rom_path), "%s/%s.cfg",
             core_dir, strip_ext(rom));

    if (file_exist(rom_path)) remove(rom_path);

    FILE *rom_file = fopen(rom_path, "w");
    if (!rom_file) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    char rom_content[MAX_BUFFER_SIZE]; /* tis a confusing one! */
    snprintf(rom_content, sizeof(rom_content), "%s\n%s\n%s\n%d\n%s\n%s\n%s",
             strip_ext(rom),
             core,
             str_trim(sys),
             cache,
             str_replace(rom_dir, get_last_subdir(rom_dir, '/', 4), ""),
             get_last_subdir(rom_dir, '/', 4),
             rom
    );

    LOG_INFO(mux_module, "Assign Content (Single): %s", str_replace(rom_content, "\n", "|"))
    fprintf(rom_file, "%s", rom_content);
    fclose(rom_file);
}

void assign_core_directory(char *core_dir, const char *core, char *sys, int cache, int purge) {
    if (purge) {
        delete_files_of_type(core_dir, "/core.cfg", NULL, 0);
        update_cfg_files(core_dir, core, str_trim(sys), cache);
    }

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/core.cfg", core_dir);

    FILE *file = fopen(core_file, "w");
    if (!file) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    char content[MAX_BUFFER_SIZE];
    snprintf(content, sizeof(content), "%s\n%s\n%d",
             core,
             str_trim(sys),
             cache
    );

    LOG_INFO(mux_module, "Assign Content (Directory): %s", str_replace(content, "\n", "|"))
    fprintf(file, "%s", content);
    fclose(file);
}

void assign_core_parent(char *rom_dir, char *core_dir, const char *core, char *sys, int cache) {
    assign_core_directory(core_dir, core, sys, cache, 1);

    char **subdirs = get_subdirectories(rom_dir);
    if (subdirs) {
        for (int i = 0; subdirs[i]; i++) {
            char subdir_file[MAX_BUFFER_SIZE];
            snprintf(subdir_file, sizeof(subdir_file), "%s/%s/core.cfg", core_dir, subdirs[i]);

            create_directories(strip_dir(subdir_file));

            FILE *subdir_file_handle = fopen(subdir_file, "w");
            if (!subdir_file_handle) {
                perror(lang.SYSTEM.FAIL_FILE_OPEN);
                continue;
            }

            char content[MAX_BUFFER_SIZE];
            snprintf(content, sizeof(content), "%s\n%s\n%d",
                     core,
                     str_trim(sys),
                     cache
            );

            LOG_INFO(mux_module, "Assign Content (Recursive): %s", str_replace(content, "\n", "|"))
            fprintf(subdir_file_handle, "%s", content);
            fclose(subdir_file_handle);

            char core_dir_path[MAX_BUFFER_SIZE];
            snprintf(core_dir_path, sizeof(core_dir_path), "%s%s", core_dir, subdirs[i]);
            update_cfg_files(core_dir_path, core, str_trim(sys), cache);
        }
        free_subdirectories(subdirs);
    }
}

void
create_core_assignment(char *rom_dir, const char *core, char *sys, char *rom, int cache, enum core_gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), "%s/%s",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));

    create_directories(core_dir);

    switch (method) {
        case SINGLE:
            assign_core_single(rom_dir, core_dir, core, sys, rom, cache);
            break;
        case PARENT:
            assign_core_parent(rom_dir, core_dir, core, sys, cache);
            break;
        case DIRECTORY:
            assign_core_directory(core_dir, core, sys, cache, 1);
            break;
        case DIRECTORY_NO_WIPE:
        default:
            assign_core_directory(core_dir, core, sys, cache, 0);
            break;
    }

    char pico8_splore[MAX_BUFFER_SIZE];
    snprintf(pico8_splore, sizeof(pico8_splore), "%s/Splore.p8", rom_dir);
    if (!strcasecmp(core, "ext-pico8") && !file_exist(pico8_splore)) write_text_to_file(pico8_splore, "w", CHAR, "");

    if (file_exist(MUOS_SAA_LOAD)) remove(MUOS_SAA_LOAD);
}

bool automatic_assign_core(char *rom_dir) {
    LOG_INFO(mux_module, "Automatic Assign Core Initiated")

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s/core.cfg",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));

    if (file_exist(core_file)) return true;
    int auto_assign_good = 0;

    char assign_file[MAX_BUFFER_SIZE];
    snprintf(assign_file, sizeof(assign_file), "%s/%s.json",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN);

    if (json_valid(read_all_char_from(assign_file))) {
        static char assign_check[MAX_BUFFER_SIZE];
        snprintf(assign_check, sizeof(assign_check), "%s",
                 str_tolower(get_last_dir(rom_dir)));
        str_remchars(assign_check, " -_+");

        struct json auto_assign_config = json_object_get(
                json_parse(read_all_char_from(assign_file)),
                assign_check);

        if (json_exists(auto_assign_config)) {
            char ass_config[MAX_BUFFER_SIZE];
            json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

            LOG_INFO(mux_module, "\tSystem Assigned: %s", ass_config)

            char assigned_core_global[MAX_BUFFER_SIZE];
            snprintf(assigned_core_global, sizeof(assigned_core_global), "%s/%s/%s/global.ini",
                     device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, ass_config);

            LOG_INFO(mux_module, "\tObtaining System Global INI: %s", assigned_core_global)

            mini_t *global_ini = mini_load(assigned_core_global);

            static char def_core[MAX_BUFFER_SIZE];
            strcpy(def_core, get_ini_string(global_ini, "global", "default", "none"));

            LOG_INFO(mux_module, "\tDefault Core: %s", def_core)

            if (strcmp(def_core, "none") != 0) {
                char default_core[MAX_BUFFER_SIZE];
                snprintf(default_core, sizeof(default_core), "%s/%s/%s/%s.ini",
                         device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, ass_config, def_core);

                mini_t *core_ini = mini_load(default_core);

                static char auto_core[MAX_BUFFER_SIZE];
                strcpy(auto_core, get_ini_string(core_ini, def_core, "core", "none"));

                if (strcmp(auto_core, "none") != 0) {
                    LOG_INFO(mux_module, "\tAssigned Core To: %s", auto_core)

                    static char core_catalogue[MAX_BUFFER_SIZE];
                    static int core_lookup;

                    char *use_local_catalogue = get_ini_string(core_ini, def_core, "catalogue", "none");
                    if (strcmp(use_local_catalogue, "none") != 0) {
                        strcpy(core_catalogue, use_local_catalogue);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Catalogue: %s", core_catalogue)
                    } else {
                        strcpy(core_catalogue, get_ini_string(global_ini, "global", "catalogue", "none"));
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Catalogue: %s", core_catalogue)
                    }

                    int use_local_lookup = get_ini_int(core_ini, def_core, "lookup", 0);
                    if (use_local_lookup) {
                        core_lookup = use_local_lookup;
                        LOG_INFO(mux_module, "\t(LOCAL) Core Lookup: %d", core_lookup)
                    } else {
                        core_lookup = get_ini_int(global_ini, "global", "lookup", 0);
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Lookup: %d", core_lookup)
                    }

                    create_core_assignment(rom_dir, auto_core, core_catalogue, "", core_lookup, DIRECTORY_NO_WIPE);

                    auto_assign_good = 1;
                    LOG_SUCCESS(mux_module, "\tSystem and Core Assignment Successful")
                } else {
                    LOG_ERROR(mux_module, "\tInvalid Core or Not Found: %s", auto_core)
                }

                mini_free(core_ini);
            } else {
                LOG_ERROR(mux_module, "\tInvalid Core or Not Found: %s", def_core)
            }

            mini_free(global_ini);
        }
    }

    return auto_assign_good == 1;
}
