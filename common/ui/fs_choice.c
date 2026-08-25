#include <unistd.h>
#include "fs_choice.h"
#include "../../module/muxshare.h"
#include "../language.h"
#include "../options.h"
#include "common.h"

typedef enum { fs_vfat = 0, fs_exfat, fs_ext4 } fs_opt;

static const char *fs_name[FS_CHOICE_COUNT] = {"vfat", "exfat", "ext4"};

// The name shown to the user
static const char *fs_label(const fs_opt opt) {
    switch (opt) {
        case fs_exfat:
            return lang.generic.filesystem.exfat;
        case fs_ext4:
            return lang.generic.filesystem.ext4;
        default:
            return lang.generic.filesystem.vfat;
    }
}

// The line describing what the filesystem is good for
static const char *fs_about(const fs_opt opt) {
    switch (opt) {
        case fs_exfat:
            return lang.generic.filesystem.about_exfat;
        case fs_ext4:
            return lang.generic.filesystem.about_ext4;
        default:
            return lang.generic.filesystem.about_vfat;
    }
}

static int fs_offer[FS_CHOICE_COUNT];
static int fs_offered = 0;

static int mkfs_exists(const char *type) {
    if (strncmp(type, "ext", 3) == 0 && access(OPT_PATH "bin/mke2fs", X_OK) == 0) return 1;

    static const char *dirs[] = {"/sbin/", "/usr/sbin/", "/bin/", "/usr/bin/"};

    for (size_t i = 0; i < A_SIZE(dirs); i++) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), "%smkfs.%s", dirs[i], type);

        if (access(path, X_OK) == 0) return 1;
    }

    return 0;
}

static void fs_choice_message(char *out, const size_t len, const fs_choice_text *text, const int selected) {
    const int slot = selected >= 0 && selected < fs_offered ? selected : 0;

    snprintf(out, len, "%s\n\n%s", text->description, fs_about(fs_offer[slot]));
}

const char *fs_choice_name(const int slot) {
    if (slot < 0 || slot >= fs_offered) return NULL;

    return fs_name[fs_offer[slot]];
}

const char *fs_choice_label(const int slot) {
    if (slot < 0 || slot >= fs_offered) return NULL;

    return fs_label(fs_offer[slot]);
}

void fs_choice_describe(mux_dialogue *dlg, const fs_choice_text *text) {
    char message[MAX_BUFFER_SIZE];
    fs_choice_message(message, sizeof(message), text, dlg->selected);

    dialogue_set_description(dlg, message);
}

int fs_choice_open(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const fs_choice_text *text) {
    const char *labels[FS_CHOICE_COUNT];
    fs_offered = 0;

    for (int i = 0; i < FS_CHOICE_COUNT; i++) {
        if (!mkfs_exists(fs_name[i])) continue;

        fs_offer[fs_offered] = i;
        labels[fs_offered] = fs_label(i);
        fs_offered++;
    }

    if (fs_offered == 0) {
        play_sound(snd_error);
        toast_message(lang.generic.filesystem.no_tooling, tst_wait_m);

        return 0;
    }

    int start = 0;
    for (int i = 0; i < fs_offered; i++) {
        if (fs_offer[i] != fs_exfat) continue;

        start = i;
        break;
    }

    char message[MAX_BUFFER_SIZE];
    fs_choice_message(message, sizeof(message), text, start);

    dialogue_init_choice(
        dlg, t, parent, text->title, message, labels, fs_offered, lang.generic.select, lang.generic.cancel
    );

    dialogue_open_at(dlg, t, start);

    return 1;
}
