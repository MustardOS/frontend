#include "miniz/miniz.h"
#include "json/json.h"
#include "common.h"
#include "common_core.h"
#include "device.h"
#include "language.h"
#include "log.h"
#include "mini/mini.h"

void get_catalogue_name(char *sys_dir, char *content_label, char *catalogue_name, size_t catalogue_name_size) {
    char sys_dir_lower[MAX_BUFFER_SIZE];
    char *raw = get_last_subdir(sys_dir, '/', 4); // rawr XD...
    if (strcmp(raw, "") == 0) {
        sys_dir_lower[0] = '\0';
    } else {
        snprintf(sys_dir_lower, sizeof(sys_dir_lower), "%s/", raw);
    }

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%s%s.cfg",
             sys_dir_lower, strip_ext(content_label));

    if (!file_exist(core_file)) {
        snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%score.cfg",
                 sys_dir_lower);
        snprintf(catalogue_name, catalogue_name_size, "%s",
                 read_line_char_from(core_file, GLOBAL_CATALOGUE));
    } else {
        snprintf(catalogue_name, catalogue_name_size, "%s",
                 read_line_char_from(core_file, CONTENT_CATALOGUE));
    }

    LOG_INFO(mux_module, "Reading Configuration: %s", core_file)
}

char *get_catalogue_name_from_rom_path(char *sys_dir, char *content_label) {
    char rom_dir[MAX_BUFFER_SIZE];
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", sys_dir, content_label);

    return get_content_line(rom_dir, NULL, "cfg", GLOBAL_CATALOGUE);
}

void write_core_file(char *def_core, char *path, char *core, char *sys, char *cat, int lookup,
                     char *rom_name, char *rom_mount, char *rom_base, char *rom_full) {
    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path)
        return;
    }

    if (rom_name) {
        fprintf(f, "%s\n%s\n%s\n%s\n%d\n%s\n%s\n%s\n%s",
                rom_name, core, sys, cat, lookup, def_core, rom_mount, rom_base, rom_full);
        LOG_INFO(mux_module, "Assign Content (Single): %s|%s|%s|%s|%d|%s|%s|%s|%s",
                 rom_name, core, sys, cat, lookup, def_core, rom_mount, rom_base, rom_full)
    } else {
        fprintf(f, "%s\n%s\n%s\n%d\n%s", core, sys, cat, lookup, def_core);
        LOG_INFO(mux_module, "Assign Content: %s|%s|%s|%d|%s", core, sys, cat, lookup, def_core)
    }

    fclose(f);
}

void write_gov_file(char *path, char *gov, char *rom_name) {
    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path)
        return;
    }

    if (rom_name) {
        fprintf(f, "%s", gov);
        LOG_INFO(mux_module, "Assign Governor (Single): %s", gov)
    } else {
        fprintf(f, "%s", gov);
        LOG_INFO(mux_module, "Assign Governor: %s", gov)
    }

    fclose(f);
}

void write_control_file(char *path, char *control, char *rom_name) {
    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path)
        return;
    }

    if (rom_name) {
        fprintf(f, "%s", control);
        LOG_INFO(mux_module, "Assign Control (Single): %s", control)
    } else {
        fprintf(f, "%s", control);
        LOG_INFO(mux_module, "Assign Control: %s", control)
    }

    fclose(f);
}

void assign_core_single(char *def_core, char *rom_dir, char *core_dir, char *core,
                        char *sys, char *cat, char *rom, char *gov, char *control, int lookup) {
    char base_path[MAX_BUFFER_SIZE];
    char *rom_no_ext = strip_ext(rom);

    snprintf(base_path, sizeof(base_path), "%s/%s", core_dir, rom_no_ext);

    char cfg_path[MAX_BUFFER_SIZE];
    char gov_path[MAX_BUFFER_SIZE];
    char control_path[MAX_BUFFER_SIZE];

    snprintf(cfg_path, sizeof(cfg_path), "%s.cfg", base_path);
    snprintf(gov_path, sizeof(gov_path), "%s.gov", base_path);
    snprintf(control_path, sizeof(control_path), "%s.con", base_path);

    char *paths[] = {cfg_path, gov_path, control_path};
    for (size_t i = 0; i < A_SIZE(paths); ++i) if (file_exist(paths[i])) remove(paths[i]);

    char *last_sub = get_last_subdir(rom_dir, '/', 4);
    char *base_dir = (last_sub[0] == '\0') ? rom_dir : str_replace(rom_dir, last_sub, "");

    write_core_file(def_core, cfg_path, core, str_trim(sys), cat, lookup, rom_no_ext,
                    base_dir, last_sub, rom);
    write_gov_file(gov_path, gov, rom_no_ext);
    write_control_file(control_path, control, rom_no_ext);
}

void assign_core_directory(char *def_core, char *core_dir, char *core, char *sys, char *cat,
                           char *gov, char *control, int lookup, int purge) {
    if (purge) {
        delete_files_of_type(core_dir, ".cfg", NULL, 0);
        delete_files_of_type(core_dir, ".gov", NULL, 0);
        delete_files_of_type(core_dir, ".con", NULL, 0);
    }

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/core.cfg", core_dir);
    write_core_file(def_core, core_file, core, str_trim(sys), cat, lookup, NULL, NULL, NULL, NULL);

    char gov_file[MAX_BUFFER_SIZE];
    snprintf(gov_file, sizeof(gov_file), "%s/core.gov", core_dir);
    write_gov_file(gov_file, gov, NULL);

    char control_file[MAX_BUFFER_SIZE];
    snprintf(control_file, sizeof(control_file), "%s/core.con", core_dir);
    write_control_file(control_file, control, NULL);
}

void assign_core_parent(char *def_core, char *rom_dir, char *core_dir, char *core,
                        char *sys, char *cat, char *gov, char *control, int lookup) {
    delete_files_of_type(core_dir, ".cfg", NULL, 1);
    delete_files_of_type(core_dir, ".gov", NULL, 1);
    delete_files_of_type(core_dir, ".con", NULL, 1);

    assign_core_directory(def_core, core_dir, core, sys, cat, gov, control, lookup, 1);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char subdir_path[MAX_BUFFER_SIZE];
        snprintf(subdir_path, sizeof(subdir_path), "%s/%s", core_dir, subdirs[i]);

        create_directories(subdir_path);

        char subdir_core[MAX_BUFFER_SIZE];
        snprintf(subdir_core, sizeof(subdir_core), "%s/%s/core.cfg", core_dir, subdirs[i]);
        write_core_file(def_core, subdir_core, core, str_trim(sys), cat, lookup, NULL, NULL, NULL, NULL);

        char subdir_gov[MAX_BUFFER_SIZE];
        snprintf(subdir_gov, sizeof(subdir_gov), "%s/%s/core.gov", core_dir, subdirs[i]);
        write_gov_file(subdir_gov, gov, NULL);

        char subdir_control[MAX_BUFFER_SIZE];
        snprintf(subdir_control, sizeof(subdir_control), "%s/%s/core.con", core_dir, subdirs[i]);
        write_gov_file(subdir_control, control, NULL);
    }

    free_subdirectories(subdirs);
}

void create_core_assignment(char *def_core, char *rom_dir, char *core, char *sys, char *cat, char *rom,
                            char *gov, char *control, int lookup, enum gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), INFO_COR_PATH "/%s",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(core_dir);

    create_directories(core_dir);

    switch (method) {
        case SINGLE:
            assign_core_single(def_core, rom_dir, core_dir, core, sys, cat, rom, gov, control, lookup);
            break;
        case PARENT:
            assign_core_parent(def_core, rom_dir, core_dir, core, sys, cat, gov, control, lookup);
            break;
        case DIRECTORY:
            assign_core_directory(def_core, core_dir, core, sys, cat, gov, control, lookup, 1);
            break;
        case DIRECTORY_NO_WIPE:
            assign_core_directory(def_core, core_dir, core, sys, cat, gov, control, lookup, 0);
            break;
    }

    char p8_splore[MAX_BUFFER_SIZE];
    snprintf(p8_splore, sizeof(p8_splore), "%s/Splore.p8", rom_dir);
    if (strncmp(core, "ext-pico8", 9) == 0 && !file_exist(p8_splore)) write_text_to_file(p8_splore, "w", CHAR, "");

    if (file_exist(MUOS_SAA_LOAD)) remove(MUOS_SAA_LOAD);
}

bool automatic_assign_core(char *rom_dir) {
    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%s/core.cfg",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(core_file);

    if (file_exist(core_file)) return true;
    LOG_INFO(mux_module, "Automatic Assign Core Initiated")
    int auto_assign_good = 0;

    char assign_file[MAX_BUFFER_SIZE];
    snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

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
            snprintf(assigned_core_global, sizeof(assigned_core_global), STORE_LOC_ASIN "/%s/global.ini",
                     ass_config);

            LOG_INFO(mux_module, "\tObtaining System Global INI: %s", assigned_core_global)

            mini_t *global_ini = mini_load(assigned_core_global);

            static char def_core[MAX_BUFFER_SIZE];
            strcpy(def_core, get_ini_string(global_ini, "global", "default", "none"));

            LOG_INFO(mux_module, "\tDefault Core: %s", def_core)

            if (strcmp(def_core, "none") != 0) {
                char default_core[MAX_BUFFER_SIZE];
                snprintf(default_core, sizeof(default_core), STORE_LOC_ASIN "/%s/%s.ini",
                         ass_config, def_core);

                mini_t *core_ini = mini_load(default_core);

                static char auto_core[MAX_BUFFER_SIZE];
                strcpy(auto_core, get_ini_string(core_ini, def_core, "core", "none"));

                if (strcmp(auto_core, "none") != 0) {
                    LOG_INFO(mux_module, "\tAssigned Core To: %s", auto_core)

                    static char core_catalogue[MAX_BUFFER_SIZE];
                    static char core_governor[MAX_BUFFER_SIZE];
                    static char core_control[MAX_BUFFER_SIZE];
                    static int core_lookup;

                    char *use_local_catalogue = get_ini_string(core_ini, def_core, "catalogue", "none");
                    if (strcmp(use_local_catalogue, "none") != 0) {
                        strcpy(core_catalogue, use_local_catalogue);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Catalogue: %s", core_catalogue)
                    } else {
                        strcpy(core_catalogue, get_ini_string(global_ini, "global", "catalogue", "none"));
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Catalogue: %s", core_catalogue)
                    }

                    char *use_local_governor = get_ini_string(core_ini, def_core, "governor", "none");
                    if (strcmp(use_local_governor, "none") != 0) {
                        strcpy(core_governor, use_local_governor);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Governor: %s", core_governor)
                    } else {
                        strcpy(core_governor, get_ini_string(global_ini, "global", "governor", device.CPU.DEFAULT));
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Governor: %s", core_governor)
                    }

                    char *use_local_control = get_ini_string(core_ini, def_core, "control", "none");
                    if (strcmp(use_local_control, "none") != 0) {
                        strcpy(core_control, use_local_control);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Control: %s", core_control)
                    } else {
                        strcpy(core_control, get_ini_string(global_ini, "global", "control", "system"));
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Governor: %s", core_control)
                    }

                    int use_local_lookup = get_ini_int(core_ini, def_core, "lookup", 0);
                    if (use_local_lookup) {
                        core_lookup = use_local_lookup;
                        LOG_INFO(mux_module, "\t(LOCAL) Core Lookup: %d", core_lookup)
                    } else {
                        core_lookup = get_ini_int(global_ini, "global", "lookup", 0);
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Lookup: %d", core_lookup)
                    }

                    create_core_assignment(def_core, rom_dir, auto_core, ass_config, core_catalogue,
                                           "", core_governor, core_control, core_lookup, DIRECTORY);

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
