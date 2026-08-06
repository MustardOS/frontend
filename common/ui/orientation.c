#include <stdio.h>
#include <sys/stat.h>
#include "orientation.h"
#include "../../module/muxshare.h"
#include "nav.h"
#include "../audio.h"
#include "../config.h"
#include "../fileio.h"
#include "../options.h"

#define ORIENTATION_DIR  CONF_CONFIG_PATH "orientation"
#define ORIENTATION_FLAG CONF_CONFIG_PATH "settings/general/orientation"

#define ORIENTATION_OFF 0
#define ORIENTATION_ON  1
#define ORIENTATION_NEW 2

static int showing = 0;

static void marker_path(const char *module, char *out, const size_t size) {
    snprintf(out, size, "%s/%s", ORIENTATION_DIR, module ? module : "");
}

static int install_mode(void) {
    return config.boot.factory_reset != 0;
}

int orientation_pending(void) {
    return !install_mode() && config.settings.general.orientation == ORIENTATION_NEW;
}

static void store(const int value) {
    config.settings.general.orientation = (int16_t) value;
    write_text_to_file(ORIENTATION_FLAG, "w", INT, value);
}

void orientation_accept(void) {
    store(ORIENTATION_ON);
}

void orientation_decline(void) {
    store(ORIENTATION_OFF);
}

int orientation_should_show(const char *module) {
    if (install_mode()) return 0;
    if (config.settings.general.orientation != ORIENTATION_ON || !module || !module[0]) return 0;

    char path[MAX_BUFFER_SIZE];
    marker_path(module, path, sizeof(path));

    return !file_exist(path);
}

void orientation_mark_shown(const char *module) {
    if (!module || !module[0]) return;

    showing = 1;

    mkdir(ORIENTATION_DIR, 0755);

    char path[MAX_BUFFER_SIZE];
    marker_path(module, path, sizeof(path));

    write_text_to_file(path, "w", INT, 1);
}

int orientation_showing(void) {
    return showing;
}

void orientation_clear_showing(void) {
    showing = 0;
}

void orientation_dismiss(void) {
    showing = 0;

    store(ORIENTATION_OFF);
}

int orientation_introduce(const char *module, const char *title, const char *text) {
    if (!orientation_should_show(module) || !text || !text[0]) return 0;

    orientation_mark_shown(module);

    play_sound(snd_info_open);
    show_info_box(title, text, 0);

    return 1;
}

int orientation_handle_skip(void) {
    if (!orientation_showing()) return 0;

    orientation_dismiss();
    handle_msgbox_dismiss();

    return 1;
}
