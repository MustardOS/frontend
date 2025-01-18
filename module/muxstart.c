#include "../lvgl/lvgl.h"
#include "ui/ui_muxstart.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"

char *mux_module;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void setup_background_process() {
    pid_t pid = fork();

    if (pid == -1) {
        perror(lang.SYSTEM.FAIL_FORK);
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <progress> <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    mux_module = basename(argv[0]);
    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    setup_background_process();

    init_display();
    init_theme(0, 0);

    init_mux();

    lv_obj_set_user_data(ui_scrStart, mux_module);

    if (config.BOOT.FACTORY_RESET) {
        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));

        char init_wall[MAX_BUFFER_SIZE];
        snprintf(init_wall, sizeof(init_wall), "M:%s/%simage/wall/muxstart.png",
                 INTERNAL_THEME, mux_dimension);
        lv_img_set_src(ui_imgWall, strdup(init_wall));
    } else {
        load_wallpaper(ui_scrStart, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    }

    load_font_text(basename(argv[0]), ui_scrStart);

#if TEST_IMAGE
    display_testing_message(ui_scrStart);
#endif

    overlay_image = lv_img_create(ui_scrStart);
    load_overlay_image(ui_scrStart, overlay_image, theme.MISC.IMAGE_OVERLAY);

    int progress = safe_atoi(argv[1]);
    if (!progress) lv_obj_set_style_bg_opa(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_bar_set_value(ui_barProgress, progress, LV_ANIM_OFF);
    lv_label_set_text(ui_lblMessage, argv[2]);

    lv_refr_now(NULL);
    lv_task_handler();

    return 0;
}
