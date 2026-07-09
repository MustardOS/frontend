#include <stdio.h>
#include <string.h>
#include "../common/fileio.h"
#include "../common/init.h"
#include "../common/log.h"
#include "core.h"
#include "paths.h"
#include "sram.h"

static char sram_path[MAX_BUFFER_SIZE] = "";

void sram_bridge_init(const char *core_path_arg, const char *content_path) {
    sram_path[0] = '\0';

    if (!current_core.retro_get_memory_data || !current_core.retro_get_memory_size) return;

    const size_t size = current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *data = current_core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0) return;

    char core_name[MAX_BUFFER_SIZE];
    core_get_name(core_path_arg, core_name, sizeof(core_name));

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", content_base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    snprintf(sram_path, sizeof(sram_path), "%s/%s/%s.srm", RETRO_SRM_PATH, core_name, content_stem);
    create_directories(sram_path, 1);

    FILE *f = fopen(sram_path, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    const long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size > 0) {
        const size_t to_read = (size_t) file_size < size ? (size_t) file_size : size;
        if (fread(data, 1, to_read, f) == to_read) {
            LOG_SUCCESS(mux_module, "Loaded SRAM: %s (%zu bytes)", sram_path, to_read);
        } else {
            LOG_ERROR(mux_module, "Failed to read SRAM: %s", sram_path);
        }
    }
    fclose(f);
}

void sram_bridge_save(void) {
    if (!sram_path[0] || !current_core.retro_get_memory_data || !current_core.retro_get_memory_size) return;

    const size_t size = current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *data = current_core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0) return;

    FILE *f = fopen(sram_path, "wb");
    if (!f) {
        LOG_ERROR(mux_module, "Failed to open SRAM for writing: %s", sram_path);
        return;
    }

    if (fwrite(data, 1, size, f) != size) LOG_ERROR(mux_module, "Failed to write SRAM: %s", sram_path);
    fclose(f);
}
