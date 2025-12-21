#include "muxshare.h"
#include "ui/ui_muxmessage.h"
#include "../lvgl/src/drivers/display/sdl.h"

#define FINISH_FILE   "/tmp/msg_finish"
#define PROGRESS_FILE "/tmp/msg_progress"

char **messages = NULL;
int message_count = 0;

char *parse_newline(const char *input) {
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

void load_messages(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open message file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[MAX_BUFFER_SIZE];
    int count = 0;
    while (fgets(line, sizeof(line), file)) count++;

    if (count == 0) {
        fprintf(stderr, "No messages found in file: %s\n", filename);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    messages = malloc(count * sizeof(char *));
    if (!messages) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    rewind(file);
    int index = 0;
    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        messages[index++] = strdup(line);
    }

    message_count = count;
    fclose(file);
}

int main(int argc, char *argv[]) {
    char *default_message = NULL;
    char *live_file = NULL;

    int progress = -1;
    int is_message_file = 0;
    int delay = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay = safe_atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            live_file = argv[++i];
        } else if (progress == -1) {
            progress = safe_atoi(argv[i]);
        } else if (!default_message) {
            default_message = argv[i];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (progress == -1 || (!default_message && !live_file)) {
        fprintf(stderr, "Usage: %s <progress> <message or message file> [-d <delay>] [-l <live file>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    load_device(&device);
    load_config(&config);

    const char *m = "muxmessage";
    set_process_name(m);
    init_module(m);

    detach_parent_process();

    init_theme(0, 0);
    init_display(1);

    init_muxmessage();
    lv_obj_set_user_data(ui_scrMessage, mux_module);

    if (config.BOOT.FACTORY_RESET) {
        char init_wall[MAX_BUFFER_SIZE];
        snprintf(init_wall, sizeof(init_wall), INTERNAL_THEME "/%simage/wall/%s.png", mux_dimension, mux_module);

        if (!file_exist(init_wall)) {
            snprintf(init_wall, sizeof(init_wall), INTERNAL_THEME "/image/wall/%s.png", mux_module);
        }

        char lv_wall[MAX_BUFFER_SIZE];
        snprintf(lv_wall, sizeof(lv_wall), "M:%s", init_wall);

        lv_img_set_src(ui_imgWall, strdup(lv_wall));
    } else {
        load_wallpaper(ui_scrMessage, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    }

    load_font_text(ui_scrMessage);

    overlay_image = lv_img_create(ui_scrMessage);
    load_overlay_image(ui_scrMessage, overlay_image);

    lv_obj_set_style_bg_opa(ui_barProgress, 0, MU_OBJ_MAIN_DEFAULT);
    refresh_screen(ui_scrMessage);

    char *ext = grab_ext((char *) default_message);
    if (strcasecmp(ext, "txt") == 0 && file_exist((char *) default_message)) is_message_file = 1;
    free(ext);

    if (live_file) {
        int fd = inotify_init();
        if (fd < 0) {
            perror("inotify_init");
            exit(1);
        }

        int wd = inotify_add_watch(fd, live_file, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
        if (wd < 0) {
            perror("inotify_add_watch");
            exit(1);
        }

        while (file_exist(live_file)) {
            union {
                char raw[4096];
                struct inotify_event align;
            } buf;

            ssize_t len = read(fd, buf.raw, sizeof(buf.raw));
            if (len < 0) {
                perror("read");
                break;
            }

            const struct inotify_event *ev;
            for (char *p = buf.raw; p < buf.raw + len;) {
                ev = (const struct inotify_event *) p;

                if (ev->mask & IN_MODIFY) {
                    lv_label_set_text_fmt(ui_lblMessage, "%s",
                                          parse_newline(read_line_char_from(live_file, 1))
                    );

                    if (file_exist(PROGRESS_FILE)) {
                        lv_bar_set_value(ui_barProgress, read_line_int_from(PROGRESS_FILE, 1), LV_ANIM_OFF);
                    }

                    refresh_screen(ui_scrMessage);
                }

                if (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) goto done;

                p += sizeof(struct inotify_event) + ev->len;
            }
        }
    } else if (is_message_file && delay > 0) {
        load_messages(default_message);
        srandom((unsigned int) (time(NULL)));

        while (!file_exist(FINISH_FILE)) {
            int index = (int) (1 + (random() % (message_count - 1)));
            lv_label_set_text_fmt(ui_lblMessage, "%s\n\n%s", messages[0], messages[index]);

            if (file_exist(PROGRESS_FILE)) {
                lv_bar_set_value(ui_barProgress, read_line_int_from(PROGRESS_FILE, 1), LV_ANIM_OFF);
            }

            refresh_screen(ui_scrMessage);
            sleep(delay);
        }

        for (int i = 0; i < message_count; i++) free(messages[i]);
        free(messages);
    } else {
        lv_bar_set_value(ui_barProgress, progress, LV_ANIM_OFF);
        lv_label_set_text(ui_lblMessage, parse_newline(default_message));

        refresh_screen(ui_scrMessage);
    }

    done:
    sdl_cleanup();
    return 0;
}
