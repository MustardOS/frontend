#include "../lvgl/lvgl.h"
#include "ui/ui_muxtester.h"
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
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

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void handle_input(mux_input_type type, mux_input_action action) {
    const char *glyph[MUX_INPUT_COUNT] = {
            // Gamepad buttons:
            [MUX_INPUT_A] = "↦⇓",
            [MUX_INPUT_B] = "↧⇒",
            [MUX_INPUT_C] = "⇫",
            [MUX_INPUT_X] = "↥⇐",
            [MUX_INPUT_Y] = "↤⇑",
            [MUX_INPUT_Z] = "⇬",
            [MUX_INPUT_L1] = "↰",
            [MUX_INPUT_L2] = "↲",
            [MUX_INPUT_L3] = "↺",
            [MUX_INPUT_R1] = "↱",
            [MUX_INPUT_R2] = "↳",
            [MUX_INPUT_R3] = "↻",
            [MUX_INPUT_SELECT] = "⇷",
            [MUX_INPUT_START] = "⇸",

            // D-pad:
            [MUX_INPUT_DPAD_UP] = "↟",
            [MUX_INPUT_DPAD_DOWN] = "↡",
            [MUX_INPUT_DPAD_LEFT] = "↞",
            [MUX_INPUT_DPAD_RIGHT] = "↠",

            // Left stick:
            [MUX_INPUT_LS_UP] = "↾",
            [MUX_INPUT_LS_DOWN] = "⇂",
            [MUX_INPUT_LS_LEFT] = "↼",
            [MUX_INPUT_LS_RIGHT] = "⇀",

            // Right stick:
            [MUX_INPUT_RS_UP] = "↿",
            [MUX_INPUT_RS_DOWN] = "⇃",
            [MUX_INPUT_RS_LEFT] = "↽",
            [MUX_INPUT_RS_RIGHT] = "⇁",

            // Volume buttons:
            [MUX_INPUT_VOL_UP] = "⇾",
            [MUX_INPUT_VOL_DOWN] = "⇽",

            // Function buttons:
            [MUX_INPUT_MENU_SHORT] = "⇹",
            [MUX_INPUT_MENU_LONG] = "⇹",
    };

    switch (action) {
        case MUX_INPUT_PRESS:
            if (glyph[type]) {
                lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_lblButton, glyph[type]);
            }
            break;
        case MUX_INPUT_RELEASE:
            lv_label_set_text(ui_lblButton, " ");
            break;
        case MUX_INPUT_HOLD:
            break;
    }
}

void handle_power() {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "tester");
    mux_input_stop();
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);
}

void init_elements() {
    lv_label_set_text(ui_lblMessage, lang.MUXTESTER.POWER);
    lv_obj_set_y(ui_pnlMessage, -5);
    lv_obj_set_height(ui_pnlFooter, 0);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(ui_pnlMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTESTER.TITLE);
    init_mux(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblFirst, lang.MUXTESTER.ANY);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);

    init_fonts();
    init_navigation_sound(&nav_sound, mux_module);

    init_input(&js_fd, &js_fd_sys);
    init_timer(NULL, NULL);

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .press_handler = {
                    [MUX_INPUT_POWER_SHORT] = handle_power,
            },
            .input_handler = handle_input,
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
