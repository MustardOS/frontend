#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/libarchive/archive.h"
#include "../../common/libarchive/archive_entry.h"
#include "../../common/log.h"
#include "../../common/strutil.h"
#include "../../common/union.h"
#include "muxretro.h"
#include "core.h"
#include "paths.h"
#include "../state/patch.h"
#include "../state/vfs.h"

struct core_cbs current_core = {0};
char core_content_path[PATH_MAX] = "";
char core_content_load_method[32] = "";
char core_active_patches[1024] = "";
int core_active_patch_count = 0;
char core_file_path[PATH_MAX] = "";
int core_restart_requested = 0;

static int open_core(const char *corefile) {
    if (current_core.initialized) {
        if (current_core.retro_deinit) current_core.retro_deinit();
    }

    if (current_core.handle) dlclose(current_core.handle);

    void (*set_environment)(retro_environment_t) = NULL;
    void (*set_video_refresh)(retro_video_refresh_t) = NULL;
    void (*set_audio_sample)(retro_audio_sample_t) = NULL;
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
    void (*set_input_poll)(retro_input_poll_t) = NULL;
    void (*set_input_state)(retro_input_state_t) = NULL;

    memset(&current_core, 0, sizeof(current_core));
    current_core.handle = dlopen(corefile, RTLD_LAZY);
    if (!current_core.handle) {
        LOG_ERROR(mux_module, "Failed to load core '%s': %s", corefile, dlerror());
        return -1;
    }

    // TODO: What else is missing from this list?
    current_core.retro_init = dlsym(current_core.handle, "retro_init");
    current_core.retro_deinit = dlsym(current_core.handle, "retro_deinit");
    current_core.retro_api_version = dlsym(current_core.handle, "retro_api_version");
    current_core.retro_get_system_info = dlsym(current_core.handle, "retro_get_system_info");
    current_core.retro_get_system_av_info = dlsym(current_core.handle, "retro_get_system_av_info");
    current_core.retro_set_controller_port_device = dlsym(current_core.handle, "retro_set_controller_port_device");
    current_core.retro_reset = dlsym(current_core.handle, "retro_reset");
    current_core.retro_run = dlsym(current_core.handle, "retro_run");
    current_core.retro_serialize_size = dlsym(current_core.handle, "retro_serialize_size");
    current_core.retro_serialize = dlsym(current_core.handle, "retro_serialize");
    current_core.retro_unserialize = dlsym(current_core.handle, "retro_unserialize");
    current_core.retro_load_game = dlsym(current_core.handle, "retro_load_game");
    current_core.retro_unload_game = dlsym(current_core.handle, "retro_unload_game");
    current_core.retro_get_memory_data = dlsym(current_core.handle, "retro_get_memory_data");
    current_core.retro_get_memory_size = dlsym(current_core.handle, "retro_get_memory_size");
    current_core.retro_cheat_reset = dlsym(current_core.handle, "retro_cheat_reset");
    current_core.retro_cheat_set = dlsym(current_core.handle, "retro_cheat_set");

    set_environment = dlsym(current_core.handle, "retro_set_environment");
    set_video_refresh = dlsym(current_core.handle, "retro_set_video_refresh");
    set_audio_sample = dlsym(current_core.handle, "retro_set_audio_sample");
    set_audio_sample_batch = dlsym(current_core.handle, "retro_set_audio_sample_batch");
    set_input_poll = dlsym(current_core.handle, "retro_set_input_poll");
    set_input_state = dlsym(current_core.handle, "retro_set_input_state");

    if (!current_core.retro_init || !current_core.retro_run || !current_core.retro_load_game || !set_environment
        || !set_video_refresh || !set_audio_sample_batch || !set_input_poll || !set_input_state) {
        LOG_ERROR(mux_module, "Core '%s' is missing required libretro symbols", corefile);
        dlclose(current_core.handle);
        memset(&current_core, 0, sizeof(current_core));
        return -1;
    }

    set_environment(mux_retro_environment_cb);
    set_video_refresh(mux_retro_video_refresh_cb);
    if (set_audio_sample) set_audio_sample(mux_retro_audio_sample_cb);
    set_audio_sample_batch(mux_retro_audio_sample_batch_cb);
    set_input_poll(mux_retro_input_poll_cb);
    set_input_state(mux_retro_input_state_cb);

    current_core.retro_init();
    current_core.initialized = true;

    current_core.need_fullpath = true;
    if (current_core.retro_get_system_info) {
        struct retro_system_info info = {0};
        current_core.retro_get_system_info(&info);
        current_core.need_fullpath = info.need_fullpath;
        current_core.block_extract = info.block_extract;
        current_core.valid_extensions = info.valid_extensions;
    }

    LOG_SUCCESS(mux_module, "Core loaded: %s", corefile);
    return 0;
}

int core_open(const char *corefile) {
    snprintf(core_file_path, sizeof(core_file_path), "%s", corefile);
    return open_core(corefile);
}

void core_get_name(const char *core_path, char *out, const size_t out_size) {
    const char *base = strrchr(core_path, '/');
    base = base ? base + 1 : core_path;

    snprintf(out, out_size, "%s", base);

    char *ext = strstr(out, "_libretro.so");
    if (ext) *ext = '\0';
}

void core_content_rel_dir(const char *content_path, char *out, const size_t out_size) {
    char *content_dir = get_content_path((char *) content_path);
    char rel_path[PATH_MAX];
    union_get_relative_path(content_dir, rel_path, sizeof(rel_path));
    free(content_dir);

    char *sub = rel_path;
    if (strncasecmp(sub, MAIN_ROM_DIR, strlen(MAIN_ROM_DIR)) == 0) {
        sub += strlen(MAIN_ROM_DIR);
        while (*sub == '/')
            sub++;
    }

    snprintf(out, out_size, "%s", sub);
}

void core_content_save_prefix(const char *core_path_arg, const char *content_path, char *out, const size_t out_size) {
    char core_name[MAX_BUFFER_SIZE];
    core_get_name(core_path_arg, core_name, sizeof(core_name));

    char rel_dir[PATH_MAX];
    core_content_rel_dir(content_path, rel_dir, sizeof(rel_dir));

    if (*rel_dir) {
        snprintf(out, out_size, "%s/%s", core_name, rel_dir);
    } else {
        snprintf(out, out_size, "%s", core_name);
    }
}

static int reopen_core(void) {
    return open_core(core_file_path);
}

static int find_first_file(const char *dir, char *out_path) {
    DIR *d = opendir(dir);
    if (!d) return -1;

    int found = -1;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_type != DT_REG) continue;
        snprintf(out_path, PATH_MAX, "%s/%s", dir, ent->d_name);
        found = 0;
        break;
    }

    closedir(d);
    return found;
}

static int extract_archive_to_dir(const char *archive_path, const char *output_dir) {
    remove_directory_recursive(output_dir);
    create_directories(output_dir, 0);

    char resolved_output[PATH_MAX];
    if (!realpath(output_dir, resolved_output)) {
        LOG_ERROR(mux_module, "Cannot resolve output path: '%s'", output_dir);
        return -1;
    }
    const size_t resolved_len = strlen(resolved_output);

    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, archive_path, 65536) != ARCHIVE_OK) {
        LOG_ERROR(mux_module, "Failed to open archive '%s': %s", archive_path, archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    struct archive_entry *entry;
    int rc = 0;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (!name) continue;

        if (name[0] == '/' || strstr(name, "..")) {
            LOG_ERROR(mux_module, "Blocked unsafe path in archive: '%s'", name);
            rc = -1;
            break;
        }

        char dest_file[PATH_MAX];
        snprintf(dest_file, sizeof(dest_file), "%s/%s", resolved_output, name);

        if (strncmp(dest_file, resolved_output, resolved_len) != 0
            || (dest_file[resolved_len] != '/' && dest_file[resolved_len] != '\0')) {
            LOG_ERROR(mux_module, "Blocked path escape in archive: '%s'", name);
            rc = -1;
            break;
        }

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            create_directories(dest_file, 0);
            continue;
        }

        if (archive_entry_filetype(entry) != AE_IFREG) {
            archive_read_data_skip(a);
            continue;
        }

        create_directories(dest_file, 1);

        FILE *out = fopen(dest_file, "wb");
        if (!out) {
            LOG_ERROR(mux_module, "Could not create '%s'", dest_file);
            rc = -1;
            break;
        }

        char buf[65536];
        la_ssize_t got;
        int write_failed = 0;
        while ((got = archive_read_data(a, buf, sizeof(buf))) > 0) {
            if (fwrite(buf, 1, (size_t) got, out) != (size_t) got) {
                write_failed = 1;
                break;
            }
        }
        fclose(out);

        if (got < 0 || write_failed) {
            LOG_ERROR(mux_module, "Failed extracting '%s' from archive: %s", name, archive_error_string(a));
            rc = -1;
            break;
        }
    }

    archive_read_free(a);
    return rc;
}

static int find_content_in_archive(const char *archive_path, char *out_path) {
    if (extract_archive_to_dir(archive_path, RETRO_EXT_PATH) != 0) {
        LOG_ERROR(mux_module, "Failed to extract archive: %s", archive_path);
        return -1;
    }

    if (current_core.valid_extensions && current_core.valid_extensions[0]) {
        char list[512];
        snprintf(list, sizeof(list), "%s", current_core.valid_extensions);

        char *ext_buf[32];
        size_t ext_count = 0;

        char *saveptr = NULL;
        for (char *tok = strtok_r(list, "|", &saveptr); tok && ext_count < 32; tok = strtok_r(NULL, "|", &saveptr)) {
            char with_dot[64];
            snprintf(with_dot, sizeof(with_dot), ".%s", tok);
            ext_buf[ext_count++] = strdup(with_dot);
        }

        const char *dirs[] = {RETRO_EXT_PATH};
        const char *exts[32];
        for (size_t i = 0; i < ext_count; i++)
            exts[i] = ext_buf[i];

        char **results = NULL;
        size_t result_count = 0;
        const int matched =
            scan_directory_list(dirs, exts, &results, 1, ext_count, &result_count) == 0 && result_count > 0;

        if (matched) snprintf(out_path, PATH_MAX, "%s", results[0]);

        for (size_t i = 0; i < result_count; i++)
            free(results[i]);
        free(results);
        for (size_t i = 0; i < ext_count; i++)
            free(ext_buf[i]);

        if (matched) return 0;
    }

    if (find_first_file(RETRO_EXT_PATH, out_path) == 0) return 0;

    LOG_ERROR(mux_module, "No matching content found inside archive: %s", archive_path);
    return -1;
}

static int find_entry_in_archive(const char *archive_path, char *out_entry) {
    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, archive_path, 65536) != ARCHIVE_OK) {
        LOG_ERROR(mux_module, "Failed to open archive '%s': %s", archive_path, archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    char fallback_entry[256] = "";
    int has_fallback = 0;
    int found = -1;

    struct archive_entry *entry;
    while (found != 0 && archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (archive_entry_filetype(entry) != AE_IFREG) continue;

        const char *name = archive_entry_pathname(entry);
        if (!name) continue;

        if (!has_fallback) {
            snprintf(fallback_entry, sizeof(fallback_entry), "%s", name);
            has_fallback = 1;
        }

        const char *dot = strrchr(name, '.');
        if (!dot || !current_core.valid_extensions || !current_core.valid_extensions[0]) continue;

        char list_copy[512];
        snprintf(list_copy, sizeof(list_copy), "%s", current_core.valid_extensions);

        char *saveptr = NULL;
        for (const char *tok = strtok_r(list_copy, "|", &saveptr); tok; tok = strtok_r(NULL, "|", &saveptr)) {
            if (strcasecmp(dot + 1, tok) == 0) {
                snprintf(out_entry, PATH_MAX, "%s", name);
                found = 0;
                break;
            }
        }
    }

    archive_read_free(a);

    if (found == 0) return 0;

    if (has_fallback) {
        snprintf(out_entry, PATH_MAX, "%s", fallback_entry);
        return 0;
    }

    LOG_ERROR(mux_module, "No files found inside archive: %s", archive_path);
    return -1;
}

static int read_whole_file(const char *path, void **out_data, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR(mux_module, "Failed to open extracted content: %s", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        LOG_ERROR(mux_module, "Extracted content is empty: %s", path);
        return -1;
    }

    void *data = malloc((size_t) size);
    if (!data || fread(data, 1, (size_t) size, f) != (size_t) size) {
        fclose(f);
        free(data);
        LOG_ERROR(mux_module, "Failed to read extracted content: %s", path);
        return -1;
    }
    fclose(f);

    *out_data = data;
    *out_size = (size_t) size;
    return 0;
}

static int write_whole_file(const char *path, const void *data, const size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR(mux_module, "Failed to create patched content file: %s", path);
        return -1;
    }

    const int ok = fwrite(data, 1, size, f) == size;
    fclose(f);

    if (!ok) {
        LOG_ERROR(mux_module, "Failed to write patched content: %s", path);
        return -1;
    }

    return 0;
}

static int write_patched_content(const void *data, const size_t size, const char *orig_path, char *out_path) {
    create_directories(RETRO_PAT_PATH, 0);

    const char *name = strrchr(orig_path, '/');
    name = name ? name + 1 : orig_path;
    snprintf(out_path, PATH_MAX, "%s/%s", RETRO_PAT_PATH, name);

    return write_whole_file(out_path, data, size);
}

int core_load_content(const char *content_path) {
    snprintf(core_content_path, sizeof(core_content_path), "%s", content_path);
    core_active_patches[0] = '\0';
    core_active_patch_count = 0;
    if (dir_exist(RETRO_EXT_PATH)) remove_directory_recursive(RETRO_EXT_PATH);
    if (dir_exist(RETRO_PAT_PATH)) remove_directory_recursive(RETRO_PAT_PATH);

    // These archive extensions should be plenty but libarchive supports a
    // heck of a lot more than these, unfortunately no arj or ace!
    static const char *archive_exts[] = {".zip", ".7z", ".gz", ".tar", ".rar"};

    const char *ext = strrchr(content_path, '.');
    int is_archive = 0;
    if (ext) {
        for (size_t i = 0; i < A_SIZE(archive_exts); i++) {
            if (strcasecmp(ext, archive_exts[i]) == 0) {
                is_archive = 1;
                break;
            }
        }
    }

    if (!is_archive || current_core.block_extract) {
        if (patch_exists(content_path)) {
            void *data = NULL;
            size_t size = 0;
            if (read_whole_file(content_path, &data, &size) != 0) return -1;

            core_active_patch_count =
                patch_apply(content_path, &data, &size, core_active_patches, sizeof(core_active_patches));

            struct retro_game_info game_info = {0};
            char patched_path[PATH_MAX] = "";

            if (current_core.need_fullpath) {
                if (write_patched_content(data, size, content_path, patched_path) != 0) {
                    free(data);
                    return -1;
                }
                game_info.path = patched_path;
            } else {
                game_info.data = data;
                game_info.size = size;
                game_info.path = content_path;
            }

            const int ok = current_core.retro_load_game(&game_info);
            free(data);

            if (!ok) {
                LOG_ERROR(mux_module, "Core rejected patched content: %s", content_path);
                return -1;
            }

            snprintf(
                core_content_load_method, sizeof(core_content_load_method), "%s",
                lang.muxretro.information_screen.direct
            );
            LOG_SUCCESS(mux_module, "Content loaded (patched): %s", content_path);
            return 0;
        }

        struct retro_game_info game_info = {.path = content_path};
        void *direct_data = NULL;
        size_t direct_size = 0;

        if (!current_core.need_fullpath) {
            if (read_whole_file(content_path, &direct_data, &direct_size) != 0) return -1;
            game_info.data = direct_data;
            game_info.size = direct_size;
        }

        const int ok = current_core.retro_load_game(&game_info);
        free(direct_data);

        if (!ok) {
            LOG_ERROR(mux_module, "Core rejected content: %s", content_path);
            return -1;
        }
        snprintf(
            core_content_load_method, sizeof(core_content_load_method), "%s", lang.muxretro.information_screen.direct
        );
        LOG_SUCCESS(mux_module, "Content loaded: %s", content_path);
        return 0;
    }

    const int has_patch = patch_exists(content_path);
    char resolved_path[PATH_MAX] = "";

    if (!has_patch && current_core.need_fullpath && vfs_bridge_is_active()) {
        char entry_name[PATH_MAX];
        if (find_entry_in_archive(content_path, entry_name) == 0) {
            snprintf(resolved_path, sizeof(resolved_path), "%s#%s", content_path, entry_name);

            struct retro_game_info game_info = {.path = resolved_path};
            if (current_core.retro_load_game(&game_info)) {
                snprintf(
                    core_content_load_method, sizeof(core_content_load_method), "%s",
                    lang.muxretro.information_screen.vfs_stream
                );
                LOG_SUCCESS(mux_module, "Content loaded: %s", content_path);
                return 0;
            }

            LOG_WARN(mux_module, "Core rejected VFS-streamed archive, falling back to extraction: %s", content_path);
            resolved_path[0] = '\0';

            if (reopen_core() != 0) return -1;
        }
    }

    if (find_content_in_archive(content_path, resolved_path) != 0) return -1;

    struct retro_game_info game_info = {0};
    void *heap_data = NULL;
    size_t heap_size = 0;

    if (has_patch || !current_core.need_fullpath) {
        if (read_whole_file(resolved_path, &heap_data, &heap_size) != 0) return -1;
        if (has_patch) {
            core_active_patch_count =
                patch_apply(content_path, &heap_data, &heap_size, core_active_patches, sizeof(core_active_patches));
        }
    }

    if (current_core.need_fullpath) {
        if (has_patch) {
            if (write_whole_file(resolved_path, heap_data, heap_size) != 0) {
                free(heap_data);
                return -1;
            }
            free(heap_data);
            heap_data = NULL;
        }
        game_info.path = resolved_path;
    } else {
        game_info.data = heap_data;
        game_info.size = heap_size;
        game_info.path = resolved_path;
    }

    const int ok = current_core.retro_load_game(&game_info);
    free(heap_data);

    if (!ok) {
        LOG_ERROR(mux_module, "Core rejected content: %s", content_path);
        return -1;
    }

    snprintf(
        core_content_load_method, sizeof(core_content_load_method), "%s", lang.muxretro.information_screen.extracted
    );
    LOG_SUCCESS(mux_module, "Content loaded: %s", content_path);
    return 0;
}

void core_unload_content(void) {
    if (current_core.retro_unload_game) current_core.retro_unload_game();
}

void core_unload(void) {
    if (!current_core.initialized) return;

    if (current_core.retro_deinit) current_core.retro_deinit();
    if (current_core.handle) dlclose(current_core.handle);

    memset(&current_core, 0, sizeof(current_core));
}
