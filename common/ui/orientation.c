#include <stdio.h>
#include <sys/stat.h>
#include "orientation.h"
#include "../../module/muxshare.h"
#include "nav.h"
#include "modal.h"
#include "task_progress.h"
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

static char pending_module[MAX_BUFFER_SIZE];
static char pending_title[MAX_BUFFER_SIZE];
static char pending_text[MAX_BUFFER_SIZE];
static int pending = 0;

static int intro_blocked(void) {
    return msgbox_active || modal_active() || task_progress_active() || progress_onscreen != -1;
}

static void intro_present(const char *module, const char *title, const char *text) {
    orientation_mark_shown(module);

    play_sound(snd_info_open);
    show_info_box(title, text, 0);
}

int orientation_introduce(const char *module, const char *title, const char *text) {
    if (!orientation_should_show(module) || !text || !text[0]) return 0;

    if (intro_blocked()) {
        snprintf(pending_module, sizeof(pending_module), "%s", module);
        snprintf(pending_title, sizeof(pending_title), "%s", title);
        snprintf(pending_text, sizeof(pending_text), "%s", text);

        pending = 1;

        return 0;
    }

    intro_present(module, title, text);

    return 1;
}

void orientation_tick(void) {
    if (!pending || intro_blocked()) return;

    pending = 0;

    if (!orientation_should_show(pending_module)) return;

    intro_present(pending_module, pending_title, pending_text);
}

void orientation_reset_pending(void) {
    pending = 0;
}

int orientation_handle_skip(void) {
    if (!orientation_showing()) return 0;

    orientation_dismiss();
    handle_msgbox_dismiss();

    return 1;
}
