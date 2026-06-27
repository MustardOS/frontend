#include "muxshare.h"
#include "ui/ui_muxpass.h"

#define PASSCODE_LEN 6
#define PASSCODE_NOP "000000"

static int passcode_eq(const char *a, const char *b) {
    unsigned char diff = 0;

    for (size_t i = 0; i < PASSCODE_LEN; i++) {
        diff |= (unsigned char) a[i] ^ (unsigned char) b[i];
    }

    return diff == 0;
}

static int exit_status_muxpass = 0;

struct mux_passcode passcode;

static int p_type;

static char *p_code;
static char *p_msg;

static lv_obj_t *ui_objects[6];

static void init_navigation_group(void) {
    ui_objects[0] = ui_rol_combo_one;
    ui_objects[1] = ui_rol_combo_two;
    ui_objects[2] = ui_rol_combo_three;
    ui_objects[3] = ui_rol_combo_four;
    ui_objects[4] = ui_rol_combo_five;
    ui_objects[5] = ui_rol_combo_six;

    reset_ui_groups();

    for (unsigned int i = 0; i < A_SIZE(ui_objects); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }
}

static void handle_a(void) {
    char b1[2], b2[2], b3[2], b4[2], b5[2], b6[2];
    const uint32_t bs = sizeof(b1);

    lv_roller_get_selected_str(ui_rol_combo_one, b1, bs);
    lv_roller_get_selected_str(ui_rol_combo_two, b2, bs);
    lv_roller_get_selected_str(ui_rol_combo_three, b3, bs);
    lv_roller_get_selected_str(ui_rol_combo_four, b4, bs);
    lv_roller_get_selected_str(ui_rol_combo_five, b5, bs);
    lv_roller_get_selected_str(ui_rol_combo_six, b6, bs);

    char try_code[13];
    snprintf(try_code, sizeof(try_code), "%s%s%s%s%s%s", b1, b2, b3, b4, b5, b6);

    const int code_match = passcode_eq(try_code, p_code);
    const int safety_match =
        strcasecmp(passcode.code.safety, PASSCODE_NOP) != 0 && passcode_eq(try_code, passcode.code.safety);

    if (code_match || safety_match) {
        play_sound(snd_muos);
        exit_status_muxpass = 1;
        mux_input_stop();
    } else {
        play_sound(snd_error);
    }
}

static void handle_b(void) {
    if (p_type != pct_boot) play_sound(snd_back);

    exit_status_muxpass = 2;
    mux_input_stop();
}

static void handle_up(void) {
    play_sound(snd_navigate);

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    lv_roller_set_selected(e_focused, lv_roller_get_selected(e_focused) - 1, LV_ANIM_ON);
}

static void handle_down(void) {
    play_sound(snd_navigate);

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    lv_roller_set_selected(e_focused, lv_roller_get_selected(e_focused) + 1, LV_ANIM_ON);
}

static void handle_left(void) {
    first_open ? (first_open = 0) : play_sound(snd_navigate);
    nav_prev(ui_group, 1);
}

static void handle_right(void) {
    first_open ? (first_open = 0) : play_sound(snd_navigate);
    nav_next(ui_group, 1);
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, NULL});
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.check, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    if (p_type == pct_boot) lv_label_set_text(ui_lbl_nav_b, lang.muxlaunch.shutdown);

    overlay_display();
}

int muxpass_main(const int auth_type) {
    p_type = auth_type;
    exit_status_muxpass = 0;

    init_module(__func__);
    load_passcode(&passcode);

    if (p_type == pct_boot) {
        p_code = passcode.code.boot;
        p_msg = passcode.message.boot;
    } else if (p_type == pct_config) {
        p_code = passcode.code.setting;
        p_msg = passcode.message.setting;
    } else if (p_type == pct_launch) {
        p_code = passcode.code.launch;
        p_msg = passcode.message.launch;
    } else {
        return 2;
    }

    if (strcasecmp(p_code, PASSCODE_NOP) == 0) return 1;

    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxpass.title);
    init_muxpass(ui_pnl_content, load_font_pass_roller());
    init_elements();

    if (strlen(p_msg) > 1) toast_message(p_msg, tst_wait_f);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    apply_pass_theme(
        ui_rol_combo_one, ui_rol_combo_two, ui_rol_combo_three, ui_rol_combo_four, ui_rol_combo_five, ui_rol_combo_six
    );

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_timer(NULL, NULL);
    refresh_screen(ui_screen, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
            },
        .release_handler = {},
        .hold_handler = {
            [mux_input_dpad_up] = handle_up,
            [mux_input_dpad_down] = handle_down,
            [mux_input_dpad_left] = handle_left,
            [mux_input_dpad_right] = handle_right,
        }
    };

    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return exit_status_muxpass;
}
