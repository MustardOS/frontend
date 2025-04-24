#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "muxshare.h"
#include "ui/ui_muxstart.h"
#include "../common/init.h"
#include "../common/common.h"

char **messages = NULL;
int message_count = 0;

void load_messages(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open message file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[MAX_BUFFER_SIZE];
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        count++;
    }

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

void free_messages(void) {
    for (int i = 0; i < message_count; i++) {
        free(messages[i]);
    }
    free(messages);
}

int main(int argc, char *argv[]) {
    int progress = -1;
    const char *default_message = NULL;
    int is_message_file = 0;
    int delay = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay = safe_atoi(argv[++i]);
        } else if (progress == -1) {
            progress = safe_atoi(argv[i]);
        } else if (!default_message) {
            default_message = argv[i];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (progress == -1 || !default_message) {
        fprintf(stderr, "Usage: %s <progress> <message or message file> [-d <delay>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    load_device(&device);
    load_config(&config);

    init_module("muxstart");
    setup_background_process();

    init_theme(0, 0);
    init_display();

    init_muxstart();
    lv_obj_set_user_data(ui_scrStart, mux_module);

    if (config.BOOT.FACTORY_RESET) {
        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));

        char init_wall[MAX_BUFFER_SIZE];
        snprintf(init_wall, sizeof(init_wall), "%s/%simage/wall/muxstart.png",
                 INTERNAL_THEME, mux_dimension);
        if (!file_exist(init_wall)) {
            snprintf(init_wall, sizeof(init_wall), "%s/image/wall/muxstart.png",
                     INTERNAL_THEME);
        }
        char lv_wall[MAX_BUFFER_SIZE];
        snprintf(lv_wall, sizeof(lv_wall), "M:%s", init_wall);
        lv_img_set_src(ui_imgWall, strdup(lv_wall));
    } else {
        load_wallpaper(ui_scrStart, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    }

    load_font_text(ui_scrStart);
    overlay_image = lv_img_create(ui_scrStart);
    load_overlay_image(ui_scrStart, overlay_image);

    lv_obj_set_style_bg_opa(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    refresh_screen(ui_scrStart);

    char *ext = grab_ext((char *) default_message);
    if (!strcasecmp(ext, "txt") && file_exist((char *) default_message)) is_message_file = 1;
    free(ext);

    if (is_message_file && delay > 0) {
        load_messages(default_message);
        srandom((unsigned int) (time(NULL)));

        while (1) {
            if (file_exist("/tmp/msg_finish")) break;

            int index = (int) (1 + (random() % (message_count - 1)));
            char combined[MAX_BUFFER_SIZE * 2];

            snprintf(combined, sizeof(combined), "\n%s\n\n%s", messages[0], messages[index]);
            lv_label_set_text(ui_lblMessage, combined);

            if (file_exist("/tmp/msg_progress")) {
                lv_bar_set_value(ui_barProgress, read_int_from_file("/tmp/msg_progress", 1), LV_ANIM_OFF);
            }

            refresh_screen(ui_scrStart);
            sleep(delay);
        }

        free_messages();
    } else {
        lv_bar_set_value(ui_barProgress, progress, LV_ANIM_OFF);
        lv_label_set_text(ui_lblMessage, default_message);
    }

    return 0;
}
