#include "../lvgl/lvgl.h"
#include <string.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"

char *mux_module;

int msgbox_active = 0;
int nav_sound = 0;
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
lv_obj_t *kiosk_image = NULL;

lv_obj_t *ui_imgButton;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void handle_input(mux_input_type type, mux_input_action action) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    char mux_dimension[15];

    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    const char *glyph[MUX_INPUT_COUNT] = {
            // Gamepad buttons:
            [MUX_INPUT_A] = "btn_a",
            [MUX_INPUT_B] = "btn_b",
            [MUX_INPUT_C] = "btn_c",
            [MUX_INPUT_X] = "btn_x",
            [MUX_INPUT_Y] = "btn_y",
            [MUX_INPUT_Z] = "btn_z",
            [MUX_INPUT_L1] = "btn_l1",
            [MUX_INPUT_L2] = "btn_l2",
            [MUX_INPUT_L3] = "btn_l3",
            [MUX_INPUT_R1] = "btn_r1",
            [MUX_INPUT_R2] = "btn_r2",
            [MUX_INPUT_R3] = "btn_r3",
            [MUX_INPUT_SELECT] = "btn_select",
            [MUX_INPUT_START] = "btn_start",
            [MUX_INPUT_MENU_SHORT] = "btn_menu",
            [MUX_INPUT_MENU_LONG] = "btn_menu",

            // D-pad:
            [MUX_INPUT_DPAD_UP] = "dpad_up",
            [MUX_INPUT_DPAD_DOWN] = "dpad_down",
            [MUX_INPUT_DPAD_LEFT] = "dpad_left",
            [MUX_INPUT_DPAD_RIGHT] = "dpad_right",

            // Left stick:
            [MUX_INPUT_LS_UP] = "ls_up",
            [MUX_INPUT_LS_DOWN] = "ls_down",
            [MUX_INPUT_LS_LEFT] = "ls_left",
            [MUX_INPUT_LS_RIGHT] = "ls_right",

            // Right stick:
            [MUX_INPUT_RS_UP] = "rs_up",
            [MUX_INPUT_RS_DOWN] = "rs_down",
            [MUX_INPUT_RS_LEFT] = "rs_left",
            [MUX_INPUT_RS_RIGHT] = "rs_right",

            // Volume buttons:
            [MUX_INPUT_VOL_UP] = "vol_up",
            [MUX_INPUT_VOL_DOWN] = "vol_down",
    };

    switch (action) {
        case MUX_INPUT_PRESS:
            if (glyph[type]) {
                lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
                if (generate_image_embed(STORAGE_THEME, mux_dimension, mux_module, glyph[type], image_path,
                                         sizeof(image_path), image_embed, sizeof(image_embed)) ||
                    generate_image_embed(INTERNAL_THEME, mux_dimension, mux_module, glyph[type], image_path,
                                         sizeof(image_path), image_embed, sizeof(image_embed))) {
                }
                if (file_exist(image_path)) {
                    lv_img_set_src(ui_imgButton, image_embed);
                } else {
                    lv_img_set_src(ui_imgButton, &ui_image_Nothing);
                }
            }
            break;
        case MUX_INPUT_RELEASE:
            lv_img_set_src(ui_imgButton, &ui_image_Nothing);
            break;
        case MUX_INPUT_HOLD:
            break;
    }
}

void handle_power() {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "tester");
    mux_input_stop();
}

void init_elements() {
    ui_imgButton = lv_img_create(ui_screen);
    lv_obj_set_align(ui_imgButton, LV_ALIGN_CENTER);
    lv_img_set_src(ui_imgButton, &ui_image_Nothing);
    lv_obj_set_style_img_recolor(ui_imgButton, lv_color_hex(theme.LIST_DEFAULT.GLYPH_RECOLOUR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_imgButton, theme.LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblMessage, lang.MUXTESTER.POWER);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(ui_pnlMessage, -12);

    lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_theme(0, 0);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTESTER.TITLE);
    init_timer(NULL, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblScreenMessage, lang.MUXTESTER.ANY);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_sound(&nav_sound, mux_module);

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .press_handler = {[MUX_INPUT_POWER_SHORT] = handle_power},
            .input_handler = handle_input,
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    safe_quit(0);
    return 0;
}
