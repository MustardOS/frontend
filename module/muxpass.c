#include "muxshare.h"
#include "ui/ui_muxpass.h"

#define UI_COUNT 6

static int exit_status_muxpass = 0;

struct mux_passcode passcode;

static int p_type;

static char *p_code;
static char *p_msg;

static lv_obj_t *ui_objects[UI_COUNT];

static void init_navigation_group(void) {
    ui_objects[0] = ui_rolComboOne;
    ui_objects[1] = ui_rolComboTwo;
    ui_objects[2] = ui_rolComboThree;
    ui_objects[3] = ui_rolComboFour;
    ui_objects[4] = ui_rolComboFive;
    ui_objects[5] = ui_rolComboSix;

    reset_ui_groups();

    for (unsigned int i = 0; i < A_SIZE(ui_objects); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }
}

static void handle_a(void) {
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
        play_sound(SND_MUOS);
        exit_status_muxpass = 1;
        close_input();
        mux_input_stop();
    } else {
        play_sound(SND_ERROR);
    }
}

static void handle_b(void) {
    if (!PCT_BOOT) play_sound(SND_BACK);

    exit_status_muxpass = 2;
    close_input();
    mux_input_stop();
}

static void handle_up(void) {
    play_sound(SND_NAVIGATE);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) - 1,
                           LV_ANIM_ON);
}

static void handle_down(void) {
    play_sound(SND_NAVIGATE);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) + 1,
                           LV_ANIM_ON);
}

static void handle_left(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);
    nav_prev(ui_group, 1);
}

static void handle_right(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);
    nav_next(ui_group, 1);
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                 0},
            {ui_lblNavA,      lang.GENERIC.CHECK, 0},
            {ui_lblNavBGlyph, "",                 0},
            {ui_lblNavB,      lang.GENERIC.BACK,  0},
            {NULL, NULL,                          0}
    });

    if (p_type == PCT_BOOT) lv_label_set_text(ui_lblNavB, lang.MUXLAUNCH.SHUTDOWN);

    overlay_display();
}

int muxpass_main(int auth_type) {
    p_type = auth_type;
    exit_status_muxpass = 0;

    init_module(__func__);

    load_passcode(&passcode, &device);

    if (p_type == PCT_BOOT) {
        p_code = passcode.CODE.BOOT;
        p_msg = passcode.MESSAGE.BOOT;
    } else if (p_type == PCT_CONFIG) {
        p_code = passcode.CODE.SETTING;
        p_msg = passcode.MESSAGE.SETTING;
    } else if (p_type == PCT_LAUNCH) {
        p_code = passcode.CODE.LAUNCH;
        p_msg = passcode.MESSAGE.LAUNCH;
    } else {
        return 2;
    }

    if (strcasecmp(p_code, "000000") == 0) return 1;

    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPASS.TITLE);
    init_muxpass(ui_pnlContent);
    init_elements();

    if (strlen(p_msg) > 1) toast_message(p_msg, FOREVER);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    apply_pass_theme(ui_rolComboOne, ui_rolComboTwo, ui_rolComboThree,
                     ui_rolComboFour, ui_rolComboFive, ui_rolComboSix);

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    load_font_text(ui_screen);

    init_fonts();
    init_navigation_group();

    init_timer(NULL, NULL);
    refresh_screen(ui_screen);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L2] = hold_call_set,
            }
    };
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return exit_status_muxpass;
}
