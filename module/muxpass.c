#include "muxshare.h"
#include "muxpass.h"
#include "ui/ui_muxpass.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/passcode.h"

static int exit_status_muxpass = 0;

struct mux_passcode passcode;
static char *p_code;
static char *p_msg;

#define UI_COUNT 6
static lv_obj_t *ui_objects[UI_COUNT];

static lv_obj_t *ui_mux_panels[2];

static void init_navigation_group() {
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

static void handle_confirm(void) {
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
        exit_status_muxpass = 1;
        close_input();
        mux_input_stop();
    }
}

static void handle_back(void) {
    play_sound("back", nav_sound, 0, 0);

    exit_status_muxpass = 2;
    close_input();
    mux_input_stop();
}

static void handle_up(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0, 0);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) - 1,
                           LV_ANIM_ON);
}

static void handle_down(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0, 0);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) + 1,
                           LV_ANIM_ON);
}

static void handle_left(void) {
    play_sound("navigate", nav_sound, 0, 0);
    nav_prev(ui_group, 1);
}

static void handle_right(void) {
    play_sound("navigate", nav_sound, 0, 0);
    nav_next(ui_group, 1);
}

static void init_elements() {
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

int muxpass_main(char *p_type) {
    exit_status_muxpass = 0;
    char *cmd_help = "\nmuOS Extras - Passcode\nUsage: %s <-t>\n\nOptions:\n"
                     "\t-t Type of passcode lock <boot|launch|setting>\n\n";

    init_module("muxpass");

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
        fprintf(stderr, cmd_help, p_type);
        return 2;
    }

    if (strcasecmp(p_code, "000000") == 0) {
        return 1;
    }

    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPASS.TITLE);
    init_muxpass(ui_pnlContent);
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

    init_timer(NULL, NULL);

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

    return exit_status_muxpass;
}
