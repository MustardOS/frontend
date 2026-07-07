#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/archive.h"
#include "../common/fileio.h"
#include "../common/init.h"
#include "../common/language.h"
#include "../common/log.h"
#include "../common/miniz/miniz.h"
#include "muxretro.h"
#include "core.h"
#include "vfs.h"

#define ARCHIVE_EXTRACT_DIR "/tmp/muxretro_archive"

struct core_cbs current_core = {0};
char core_content_path[PATH_MAX] = "";
char core_content_load_method[32] = "";
static char core_file_path[PATH_MAX] = "";

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

static int find_content_in_archive(const char *archive_path, char *out_path) {
    remove_directory_recursive(ARCHIVE_EXTRACT_DIR);

    if (extract_zip_to_dir(archive_path, ARCHIVE_EXTRACT_DIR) != MUX_EXTRACT_OK) {
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

        const char *dirs[] = {ARCHIVE_EXTRACT_DIR};
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

    if (find_first_file(ARCHIVE_EXTRACT_DIR, out_path) == 0) return 0;

    LOG_ERROR(mux_module, "No matching content found inside archive: %s", archive_path);
    return -1;
}

static int find_entry_in_archive(const char *archive_path, char *out_entry) {
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, archive_path, 0)) {
        LOG_ERROR(mux_module, "Failed to open archive: %s", archive_path);
        return -1;
    }

    const mz_uint file_count = mz_zip_reader_get_num_files(&zip);
    char fallback_entry[256] = "";
    int has_fallback = 0;
    int found = -1;

    for (mz_uint i = 0; i < file_count && found != 0; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat) || file_stat.m_is_directory) continue;

        if (!has_fallback) {
            snprintf(fallback_entry, sizeof(fallback_entry), "%s", file_stat.m_filename);
            has_fallback = 1;
        }

        const char *dot = strrchr(file_stat.m_filename, '.');
        if (!dot || !current_core.valid_extensions || !current_core.valid_extensions[0]) continue;

        char list_copy[512];
        snprintf(list_copy, sizeof(list_copy), "%s", current_core.valid_extensions);

        char *saveptr = NULL;
        for (const char *tok = strtok_r(list_copy, "|", &saveptr); tok; tok = strtok_r(NULL, "|", &saveptr)) {
            if (strcasecmp(dot + 1, tok) == 0) {
                snprintf(out_entry, PATH_MAX, "%s", file_stat.m_filename);
                found = 0;
                break;
            }
        }
    }

    mz_zip_reader_end(&zip);

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

int core_load_content(const char *content_path) {
    snprintf(core_content_path, sizeof(core_content_path), "%s", content_path);

    const char *ext = strrchr(content_path, '.');
    const int is_archive = ext && strcasecmp(ext, ".zip") == 0;

    if (!is_archive || current_core.block_extract) {
        struct retro_game_info game_info = {.path = content_path};
        if (!current_core.retro_load_game(&game_info)) {
            LOG_ERROR(mux_module, "Core rejected content: %s", content_path);
            return -1;
        }
        snprintf(
            core_content_load_method, sizeof(core_content_load_method), "%s", lang.muxretro.information_screen.direct
        );
        LOG_SUCCESS(mux_module, "Content loaded: %s", content_path);
        return 0;
    }

    char resolved_path[PATH_MAX] = "";

    if (current_core.need_fullpath && vfs_bridge_is_active()) {
        char entry_name[256];
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

    struct retro_game_info game_info = {.path = resolved_path};
    void *heap_data = NULL;

    if (!current_core.need_fullpath) {
        if (read_whole_file(resolved_path, &heap_data, &game_info.size) != 0) return -1;
        game_info.data = heap_data;
    }

    const bool ok = current_core.retro_load_game(&game_info);
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
