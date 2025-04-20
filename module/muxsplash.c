#include "muxsplash.h"
#include "../lvgl/lvgl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
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

int muxsplash_main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <full path to PNG image>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_theme(0, 0);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, "");
    lv_obj_add_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);

    char init_wall[MAX_BUFFER_SIZE];
    snprintf(init_wall, sizeof(init_wall), "M:%s", argv[1]);

    lv_img_set_src(ui_imgWall, init_wall);

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);

    refresh_screen(ui_screen);

    return 0;
}
