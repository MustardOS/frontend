#include "../lvgl/lvgl.h"
#include "ui/ui_muxsplash.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/common.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"

char *mux_module;
int msgbox_active = 0;
int nav_sound = 0;

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
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <full path to PNG image>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    setup_background_process();

    load_device(&device);
    load_config(&config);

    mux_init();
    ui_init();

    char init_wall[MAX_BUFFER_SIZE];
    snprintf(init_wall, sizeof(init_wall), "M:%s", argv[1]);

    lv_obj_t *img = lv_img_create(ui_scrSplash);
    lv_img_set_src(img, init_wall);

#if TEST_IMAGE
    display_testing_message(ui_scrSplash);
#endif

    overlay_image = lv_img_create(ui_scrSplash);
    load_overlay_image(ui_scrSplash, overlay_image, theme.MISC.IMAGE_OVERLAY);

    return 0;
}
