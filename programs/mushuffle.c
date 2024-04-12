#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "options.h"

int lpl_count(const char *directory) {
    int num_files = 0;
    DIR *dir = opendir(directory);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                char *dot = strrchr(entry->d_name, '.');
                if (dot && !strcmp(dot, ".lpl")) {
                    num_files++;
                }
            }
        }
        closedir(dir);
    }
    return num_files;
}

void lpl_read(const char *directory, char **file_names, int num_files) {
    DIR *dir = opendir(directory);
    if (dir != NULL) {
        struct dirent *entry;
        int i = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                char *dot = strrchr(entry->d_name, '.');
                if (dot && !strcmp(dot, ".lpl")) {
                    char full_path[512];
                    snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
                    file_names[i] = (char *)malloc(strlen(full_path) + 1);
                    if (file_names[i] == NULL) {
                        printf("ERROR\nOut of memory.\n");
                        exit(1);
                    }
                    strcpy(file_names[i], full_path);
                    i++;
                }
            }
        }
        closedir(dir);
    }
}

void rom_shuffle(char *rom_path, char *core_path, const char **file_names, int num_files) {
    int selected_file_index = rand() % num_files;

    for (int i = 0; i < num_files; i++) {
        FILE *fp = fopen(file_names[i], "r");
        if (fp != NULL) {
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char *json_data = (char *)malloc(file_size + 1);
            fread(json_data, 1, file_size, fp);
            fclose(fp);
            json_data[file_size] = '\0';

            cJSON *json_root = cJSON_Parse(json_data);
            free(json_data);

            cJSON *default_core_path_json = cJSON_GetObjectItemCaseSensitive(json_root, "default_core_path");
            cJSON *items = cJSON_GetObjectItemCaseSensitive(json_root, "items");

            if (cJSON_IsArray(items) > 0 && cJSON_GetArraySize(items) > 0) {
                int selected_array_item = rand() % cJSON_GetArraySize(items);
                cJSON *item = cJSON_GetArrayItem(items, selected_array_item);

                cJSON *rom_path_json = cJSON_GetObjectItemCaseSensitive(item, "path");
                cJSON *core_path_json = cJSON_GetObjectItemCaseSensitive(item, "core_path");

                if (cJSON_IsString(rom_path_json) && cJSON_IsString(core_path_json)) {
                    if (i == selected_file_index) {
                        strcpy(rom_path, rom_path_json->valuestring);
                        if (strcmp(core_path_json->valuestring, "DETECT") == 0) {
                            if (cJSON_IsString(default_core_path_json)) {
                                strcpy(core_path, default_core_path_json->valuestring);
                            }
                        } else {
                            strcpy(core_path, core_path_json->valuestring);
                        }
                    }
                }
            }

            cJSON_Delete(json_root);
        }
    }
}

void lpl_free(char **file_names, int num_files) {
    for (int i = 0; i < num_files; i++) {
        free(file_names[i]);
    }

    free(file_names);
}

int main() {
    srand(time(NULL));

    char shuf_core[256];
    char shuf_rom[256];

    const char *directory = PLAYLIST_DIR;
    int num_files = lpl_count(directory);

    if (num_files == 0) {
        printf("ERROR\nNo playlists.\n");
        return 1;
    }

    char **file_names = (char **)malloc(num_files * sizeof(char *));
    if (file_names == NULL) {
        printf("ERROR\nOut of memory.\n");
        return 1;
    }

    lpl_read(directory, file_names, num_files);
    rom_shuffle(shuf_rom, shuf_core, (const char **)file_names, num_files);
    lpl_free(file_names, num_files);

    printf("%s\n%s\n", shuf_core, shuf_rom);

    return 0;
}
