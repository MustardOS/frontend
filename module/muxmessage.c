#include "muxshare.h"
#include "ui/ui_muxmessage.h"
#include <common/tooling/message_args.h>
#include <common/storage/inotify.h>
#include <common/base/message.h>

#define FINISH_FILE   "/tmp/msg_finish"
#define PROGRESS_FILE "/tmp/msg_progress"
#define RELEASED_FILE "/run/muos/loading_released"

static void split_dir_base(const char *path, char *dir, char *base) {
    const char *slash = strrchr(path, '/');

    const size_t ds = 1024;
    const size_t bs = ds;

    if (!slash) {
        snprintf(dir, ds, ".");
        snprintf(base, bs, "%s", path);
        return;
    }

    size_t dl = (size_t) (slash - path);

    if (dl == 0) dl = 1;
    if (dl >= ds) dl = ds - 1;

    memcpy(dir, path, dl);
    dir[dl] = '\0';

    snprintf(base, bs, "%s", slash + 1);
}

static char *parse_newline(const char *input) {
    static char buffer[MAX_BUFFER_SIZE];
    size_t j = 0;

    for (size_t i = 0; input[i] != '\0' && j < sizeof(buffer) - 1; i++) {
        if (input[i] == '\\' && input[i + 1] == 'n') {
            buffer[j++] = '\n';
            i++;
        } else {
            buffer[j++] = input[i];
        }
    }

    buffer[j] = '\0';
    return buffer;
}

int main(const int argc, char *argv[]) {
    muxmessage_args args;
    int is_message_file = 0;

    if (muxmessage_args_parse(argc, argv, &args) != 0) {
        if (args.unknown_option) fprintf(stderr, "Unknown argument: %s\n", args.unknown_option);
        fprintf(stderr, "Usage: %s <progress> <message or message file> [-d <delay>] [-l <live file>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *default_message = args.message;
    char *live_file = args.live_file;
    const int progress = args.progress;
    const int delay = args.delay;

    load_device(&device);
    load_config(&config);

    init_module("muxmessage");

    detach_parent_process();

    init_theme(0, 0);
    init_display();

    init_muxmessage();
    lv_obj_set_user_data(ui_scr_message, mux_module);

    if (config.boot.factory_reset) {
        char init_wall[MAX_BUFFER_SIZE];
        int wall_ready = str_format_checked(
            init_wall, sizeof(init_wall), INTERNAL_THEME "/%simage/wall/%s.png", mux_dim, mux_module
        );

        if (!wall_ready || !file_exist_nocase(init_wall, init_wall, sizeof(init_wall)))
            wall_ready =
                str_format_checked(init_wall, sizeof(init_wall), INTERNAL_THEME "/image/wall/%s.png", mux_module);

        wall_ready = wall_ready && file_exist_nocase(init_wall, init_wall, sizeof(init_wall));

        char lv_wall[MAX_BUFFER_SIZE];
        if (wall_ready && str_format_checked(lv_wall, sizeof(lv_wall), "M:%s", init_wall))
            lv_img_set_src(ui_img_wall, lv_wall);
    } else {
        load_wallpaper(ui_scr_message, NULL, ui_img_wall, wall_general);
    }

    load_font_text(ui_scr_message);

    overlay_image = lv_img_create(ui_scr_message);
    load_overlay_image(ui_scr_message, overlay_image);

    lv_obj_set_style_bg_opa(ui_bar_progress, 0, MU_OBJ_MAIN_DEFAULT);
    refresh_screen(ui_scr_message, 1);

    message_set set = {0};
    char *ext = grab_ext(default_message);
    if (strcasecmp(ext, "json") == 0) is_message_file = message_load(default_message, &set);
    free(ext);

    if (live_file) {
        inotify_status *proc = inotify_create();

        if (!proc) {
            perror("inotify_create");
            exit(1);
        }

        char dir[MAX_BUFFER_SIZE], base[MAX_BUFFER_SIZE];
        split_dir_base(live_file, dir, base);

        int live_exists = 0;
        unsigned live_changed = 0;

        if (inotify_track(proc, dir, base, &live_exists, &live_changed) < 0) {
            perror("inotify_track");
            exit(1);
        }

        unsigned last_time = 0;
        int last_exists = live_exists;

        while (!file_exist(FINISH_FILE)) {
            inotify_check(proc);

            if (last_exists && !live_exists) {
                LOG_INFO(mux_module, "Live file removed... exiting!");
                break;
            }

            last_exists = live_exists;

            if (live_exists && live_changed != last_time) {
                last_time = live_changed;

                char *line = read_line_char_from(live_file, 1);
                if (line && *line) lv_label_set_text_fmt(ui_lbl_message, "%s", parse_newline(line));
                free(line);
                if (file_exist(PROGRESS_FILE))
                    lv_bar_set_value(ui_bar_progress, read_line_int_from(PROGRESS_FILE, 1), LV_ANIM_OFF);

                refresh_screen(ui_scr_message, 1);
            }

            usleep(25 * 1000);
        }
    } else if (is_message_file && delay > 0) {
        srandom((unsigned int) time(NULL));

        while (!file_exist(FINISH_FILE)) {
            const char *line = set.lines[message_index(&set, (int) random(), NULL)];
            if (set.title) {
                lv_label_set_text_fmt(ui_lbl_message, "%s\n\n%s", set.title, line);
            } else {
                lv_label_set_text(ui_lbl_message, line);
            }

            if (file_exist(PROGRESS_FILE))
                lv_bar_set_value(ui_bar_progress, read_line_int_from(PROGRESS_FILE, 1), LV_ANIM_OFF);

            refresh_screen(ui_scr_message, 1);
            sleep(delay);
        }

    } else {
        lv_bar_set_value(ui_bar_progress, progress, LV_ANIM_OFF);
        lv_label_set_text(ui_lbl_message, parse_newline(default_message));

        refresh_screen(ui_scr_message, 1);
    }

    message_free(&set);
    sdl_cleanup();

    FILE *released = fopen(RELEASED_FILE, "w");
    if (released) fclose(released);
    return 0;
}
