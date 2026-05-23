#include "muxshare.h"
#include "ui/ui_muxthemeopt.h"

#define THEMEOPT(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(THEMEOPT_ELEMENTS)
};
#undef THEMEOPT

#define THEMEOPT(NAME, ENUM, UDATA) static int NAME##_original;
THEMEOPT_ELEMENTS
#undef THEMEOPT

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    struct help_msg help_messages[] = {
#define THEMEOPT(NAME, ENUM, UDATA) { UDATA, lang.MUXTHEMEOPT.HELP.ENUM },
            THEMEOPT_ELEMENTS
#undef THEMEOPT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define THEMEOPT(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_themeopt);
    THEMEOPT_ELEMENTS
#undef THEMEOPT
}

static void restore_themeopt_options(void) {
    lv_dropdown_set_selected(ui_droHeaderHeight_themeopt, (uint32_t) (config.SETTINGS.THEMEOPT.HEADER_HEIGHT + 1));
    lv_dropdown_set_selected(ui_droFooterHeight_themeopt, (uint32_t) (config.SETTINGS.THEMEOPT.FOOTER_HEIGHT + 1));
    lv_dropdown_set_selected(ui_droContentItemCount_themeopt, (uint32_t) config.SETTINGS.THEMEOPT.CONTENT_ITEM_COUNT);
}

static void save_themeopt_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(themeopt, HeaderHeight, "settings/theme/header_height", INT, -1);
    CHECK_AND_SAVE_STD(themeopt, FooterHeight, "settings/theme/footer_height", INT, -1);
    CHECK_AND_SAVE_STD(themeopt, ContentItemCount, "settings/theme/content_item_count", INT, 0);

    if (is_modified > 0) run_tweak_script(lang.GENERIC.SAVING);
}

static void init_navigation_group(void) {
    char *height_options = generate_number_string(0, 64, 1, lang.MUXTHEMEOPT.SIZE_DEFAULT, NULL, NULL, 0);
    char *count_options = generate_number_string(1, 64, 1, lang.MUXTHEMEOPT.SIZE_DEFAULT, NULL, NULL, 0);

    INIT_OPTION_ITEM(-1, themeopt, HeaderHeight, lang.MUXTHEMEOPT.HEADER_HEIGHT, "headerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, FooterHeight, lang.MUXTHEMEOPT.FOOTER_HEIGHT, "footerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, ContentItemCount, lang.MUXTHEMEOPT.CONTENT_ITEM_COUNT, "count", NULL, 0);

    apply_theme_list_drop_down(&theme, ui_droHeaderHeight_themeopt, height_options);
    apply_theme_list_drop_down(&theme, ui_droFooterHeight_themeopt, height_options);
    apply_theme_list_drop_down(&theme, ui_droContentItemCount_themeopt, count_options);

    free(height_options);
    free(count_options);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);
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

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_x(void) {
    if (msgbox_active) return;

    if (!hold_call) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.HOLD_RESET, SHORT);
        return;
    }

    lv_dropdown_set_selected(ui_droHeaderHeight_themeopt, 0);
    lv_dropdown_set_selected(ui_droFooterHeight_themeopt, 0);
    lv_dropdown_set_selected(ui_droContentItemCount_themeopt, 0);

    play_sound(SND_MUOS);
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

    save_themeopt_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themeopt");

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

#define THEMEOPT(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_themeopt, UDATA);
    THEMEOPT_ELEMENTS
#undef THEMEOPT

    overlay_display();
}

int muxthemeopt_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTHEMEOPT.TITLE);
    init_muxthemeopt(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_elements();
    init_navigation_group();

    restore_themeopt_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A]          = handle_a,
                    [MUX_INPUT_B]          = handle_b,
                    [MUX_INPUT_X]          = handle_x,
                    [MUX_INPUT_DPAD_LEFT]  = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP]    = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN]  = handle_list_nav_down,
                    [MUX_INPUT_L1]         = handle_list_nav_page_up,
                    [MUX_INPUT_R1]         = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2]   = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT]  = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP]    = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN]  = handle_list_nav_down_hold,
                    [MUX_INPUT_L1]         = handle_list_nav_page_up,
                    [MUX_INPUT_L2]         = hold_call_set,
                    [MUX_INPUT_R1]         = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
