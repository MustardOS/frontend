#include "muxpass.h"
#include "../lvgl/lvgl.h"
#include "ui/ui_muxpass.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
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
#include "../common/passcode.h"

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
struct mux_passcode passcode;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

char *p_type;
char *p_code;
char *p_msg;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

lv_group_t *ui_group;

#define UI_COUNT 6
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[2];

void init_navigation_group() {
    ui_objects[0] = ui_rolComboOne;
    ui_objects[1] = ui_rolComboTwo;
    ui_objects[2] = ui_rolComboThree;
    ui_objects[3] = ui_rolComboFour;
    ui_objects[4] = ui_rolComboFive;
    ui_objects[5] = ui_rolComboSix;

    ui_group = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }
}

void handle_confirm(void) {
    play_sound("confirm", nav_sound, 0, 0);

    char b1[2], b2[2], b3[2], b4[2], b5[2], b6[2];
    uint32_t bs = sizeof(b1);

    lv_roller_get_selected_str(ui_rolComboOne, b1, bs);
    lv_roller_get_selected_str(ui_rolComboTwo, b2, bs);
    lv_roller_get_selected_str(ui_rolComboThree, b3, bs);
    lv_roller_get_selected_str(ui_rolComboFour, b4, bs);
    lv_roller_get_selected_str(ui_rolComboFive, b5, bs);
    lv_roller_get_selected_str(ui_rolComboSix, b6, bs);

    char try_code[13];
    sprintf(try_code, "%s%s%s%s%s%s", b1, b2, b3, b4, b5, b6);

    if (strcasecmp(try_code, p_code) == 0) {
        safe_quit(1);
        mux_input_stop();
    }
}

void handle_back(void) {
    play_sound("back", nav_sound, 0, 0);

    safe_quit(2);
    mux_input_stop();
}

void handle_up(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0, 0);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) - 1,
                           LV_ANIM_ON);
}

void handle_down(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0, 0);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) + 1,
                           LV_ANIM_ON);
}

void handle_left(void) {
    play_sound("navigate", nav_sound, 0, 0);
    nav_prev(ui_group, 1);
}

void handle_right(void) {
    play_sound("navigate", nav_sound, 0, 0);
    nav_next(ui_group, 1);
}

void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

int muxpass_main(int argc, char *argv[]) {
    char *cmd_help = "\nmuOS Extras - Passcode\nUsage: %s <-t>\n\nOptions:\n"
                     "\t-t Type of passcode lock <boot|launch|setting>\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                p_type = optarg;
                break;
            default:
                fprintf(stderr, cmd_help, argv[0]);
                return 2;
        }
    }

    if (!p_type) {
        fprintf(stderr, cmd_help, argv[0]);
        return 2;
    }

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    load_passcode(&passcode, &device);

    if (strcasecmp(p_type, "boot") == 0) {
        p_code = passcode.CODE.BOOT;
        p_msg = passcode.MESSAGE.BOOT;
    } else if (strcasecmp(p_type, "launch") == 0) {
        p_code = passcode.CODE.LAUNCH;
        p_msg = passcode.MESSAGE.LAUNCH;
    } else if (strcasecmp(p_type, "setting") == 0) {
        p_code = passcode.CODE.SETTING;
        p_msg = passcode.MESSAGE.SETTING;
    } else {
        fprintf(stderr, cmd_help, argv[0]);
        safe_quit(2);
        return 2;
    }

    if (strcasecmp(p_code, "000000") == 0) {
        safe_quit(1);
        return 1;
    }

    init_theme(0, 0);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPASS.TITLE);
    init_mux(ui_pnlContent);
    init_timer(NULL, NULL);
    init_elements();

    if (strlen(p_msg) > 1) toast_message(p_msg, 0, 0);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    apply_pass_theme(ui_rolComboOne, ui_rolComboTwo, ui_rolComboThree,
                     ui_rolComboFour, ui_rolComboFive, ui_rolComboSix);

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    load_font_text(ui_screen);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            }
    };
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 2;
}
