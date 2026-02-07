#include "muxshare.h"
#include "ui/ui_muxcoladjust.h"

#define UI_COUNT 6

#define COLADJUST(NAME, ENUM, UDATA) static int NAME##_original;
COLADJUST_ELEMENTS
#undef COLADJUST

static void show_help(void) {
    struct help_msg help_messages[] = {
#define COLADJUST(NAME, ENUM, UDATA) { lang.MUXCOLADJUST.HELP.ENUM },
            COLADJUST_ELEMENTS
#undef COLADJUST
    };

    gen_help(current_item_index, UI_COUNT, help_messages, ui_group, items);
}

static void init_dropdown_settings(void) {
#define COLADJUST(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_coladjust);
    COLADJUST_ELEMENTS
#undef COLADJUST
}

static void restore_coladjust_options(void) {
#define COLADJUST(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_coladjust, config.SETTINGS.COLOUR.ENUM);
    COLADJUST_ELEMENTS
#undef COLADJUST

    lv_dropdown_set_selected(ui_droTemperature_coladjust, config.SETTINGS.COLOUR.TEMPERATURE + 255);
    lv_dropdown_set_selected(ui_droBrightness_coladjust, config.SETTINGS.COLOUR.BRIGHTNESS + 100);
}

static void save_adjust_options() {
    int is_modified = 0;

    int temperature_mod = lv_dropdown_get_selected(ui_droTemperature_coladjust);
    if (temperature_mod != Temperature_original) set_setting_value("colour", temperature_mod, -255);

    CHECK_AND_SAVE_STD(coladjust, Brightness, "settings/colour/brightness", INT, -100);
    CHECK_AND_SAVE_STD(coladjust, Contrast, "settings/colour/contrast", INT, 0);
    CHECK_AND_SAVE_STD(coladjust, Saturation, "settings/colour/saturation", INT, 0);
    CHECK_AND_SAVE_STD(coladjust, HueShift, "settings/colour/hueshift", INT, 0);
    CHECK_AND_SAVE_STD(coladjust, Gamma, "settings/colour/gamma", INT, 0);

    if (is_modified > 0) refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, coladjust, Temperature, lang.MUXCOLADJUST.TEMPERATURE, "temperature", NULL, 0);
    char *temperature_values = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droTemperature_coladjust, temperature_values);
    free(temperature_values);

    INIT_OPTION_ITEM(-1, coladjust, Brightness, lang.MUXCOLADJUST.BRIGHTNESS, "brightness", NULL, 0);
    char *brightness_values = generate_number_string(-100, 100, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBrightness_coladjust, brightness_values);
    free(brightness_values);

    INIT_OPTION_ITEM(-1, coladjust, Contrast, lang.MUXCOLADJUST.CONTRAST, "contrast", NULL, 0);
    char *contrast_values = generate_number_string(0, 200, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droContrast_coladjust, contrast_values);
    free(contrast_values);

    INIT_OPTION_ITEM(-1, coladjust, Saturation, lang.MUXCOLADJUST.SATURATION, "saturation", NULL, 0);
    char *saturation_values = generate_number_string(0, 300, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSaturation_coladjust, saturation_values);
    free(saturation_values);

    INIT_OPTION_ITEM(-1, coladjust, HueShift, lang.MUXCOLADJUST.HUESHIFT, "hueshift", NULL, 0);
    char *hueshift_values = generate_number_string(0, 300, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droHueShift_coladjust, hueshift_values);
    free(hueshift_values);

    INIT_OPTION_ITEM(-1, coladjust, Gamma, lang.MUXCOLADJUST.GAMMA, "gamma", NULL, 0);
    char *gamma_values = generate_number_string(0, 200, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droGamma_coladjust, gamma_values);
    free(gamma_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_option_prev_multi(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -25);
}

static void handle_option_next_multi(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +25);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    save_adjust_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "colour");
    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active) return;

    if (!hold_call) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.HOLD_RESET, SHORT);
        return;
    }

    set_setting_value("colour", DEFAULT_TEMPERATURE, 0);
    remove_directory_recursive(CONF_CONFIG_PATH "settings/colour");
    refresh_config = 1;

    play_sound(SND_MUOS);
    load_mux("colour");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.RESET,  0},
            {NULL, NULL,                            0}
    });

#define COLADJUST(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_coladjust, UDATA);
    COLADJUST_ELEMENTS
#undef COLADJUST

    overlay_display();
}

int muxcoladjust_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCOLADJUST.TITLE);
    init_muxcoladjust(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_coladjust_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = handle_option_prev_multi,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_option_next_multi,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
