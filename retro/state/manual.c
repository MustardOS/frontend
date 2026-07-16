#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/catalogue.h"
#include "../../common/core/common.h"
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/strutil.h"
#include "../core/core.h"
#include "../core/paths.h"
#include "manual.h"

static int available = 0;

static char manual_path[PATH_MAX] = "";
static char position_path[PATH_MAX] = "";

void manual_init(const char *core_path_arg, const char *content_path) {
    available = 0;
    manual_path[0] = '\0';
    position_path[0] = '\0';

    char *sys_dir = get_content_path((char *) content_path);
    const char *content_label = get_file_name(content_path);

    char catalogue_name[MAX_BUFFER_SIZE];
    get_catalogue_name(sys_dir, content_label, catalogue_name, sizeof(catalogue_name));
    free(sys_dir);

    char *program_no_ext = strip_ext(content_label);
    char program[MAX_BUFFER_SIZE];
    snprintf(program, sizeof(program), "%s", program_no_ext ? program_no_ext : content_label);
    free(program_no_ext);

    if (catalogue_name[0]
        && load_manual_catalogue(catalogue_name, program, program, manual_path, sizeof(manual_path))) {
        available = 1;
        LOG_INFO(mux_module, "Catalogue manual found: %s", manual_path);
    }

    char save_prefix[MAX_BUFFER_SIZE];
    core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix));

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", content_base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    snprintf(position_path, sizeof(position_path), "%s/%s/%s.pos", RETRO_MAN_PATH, save_prefix, content_stem);
}

int manual_is_available(void) {
    return available;
}

const char *manual_get_path(void) {
    return manual_path;
}

int manual_load_position(void) {
    if (!position_path[0]) return 0;

    FILE *f = fopen(position_path, "r");
    if (!f) return 0;

    int value = 0;
    const int got = fscanf(f, "%d", &value);
    fclose(f);

    return got == 1 && value >= 0 ? value : 0;
}

void manual_save_position(const int value) {
    if (!position_path[0]) return;

    create_directories(position_path, 1);

    FILE *f = fopen(position_path, "w");
    if (!f) return;

    fprintf(f, "%d", value);
    fclose(f);
}

static void font_size_path(char *out) {
    snprintf(out, PATH_MAX, "%s/fontsize", RETRO_MAN_PATH);
}

int manual_load_font_size(const int default_size) {
    char path[PATH_MAX];
    font_size_path(path);

    FILE *f = fopen(path, "r");
    if (!f) return default_size;

    int value = 0;
    const int got = fscanf(f, "%d", &value);
    fclose(f);

    return got == 1 && value > 0 ? value : default_size;
}

void manual_save_font_size(const int value) {
    char path[PATH_MAX];
    font_size_path(path);

    create_directories(path, 1);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "%d", value);
    fclose(f);
}

static void wrap_enabled_path(char *out) {
    snprintf(out, PATH_MAX, "%s/wrapenabled", RETRO_MAN_PATH);
}

int manual_load_wrap_enabled(void) {
    char path[PATH_MAX];
    wrap_enabled_path(path);

    FILE *f = fopen(path, "r");
    if (!f) return 0; // default: no wrap

    int value = 0;
    const int got = fscanf(f, "%d", &value);
    fclose(f);

    return got == 1 && value == 1 ? 1 : 0;
}

void manual_save_wrap_enabled(const int value) {
    char path[PATH_MAX];
    wrap_enabled_path(path);

    create_directories(path, 1);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "%d", value ? 1 : 0);
    fclose(f);
}
