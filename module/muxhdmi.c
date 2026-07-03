#include "muxshare.h"
#include "ui/ui_muxhdmi.h"

#define HDMI(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(HDMI_ELEMENTS) };
#undef HDMI

#define HDMI(NAME, UDATA) static int NAME##_original;
HDMI_ELEMENTS
#undef HDMI

int hdmi_index[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define HDMI(NAME, UDATA) {UDATA, lang.muxhdmi.help.NAME},
        HDMI_ELEMENTS
#undef HDMI
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define HDMI(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_hdmi);
    HDMI_ELEMENTS
#undef HDMI
}

static void restore_hdmi_options(void) {
#define HDMI(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_hdmi, config.settings.hdmi.NAME);
    HDMI_ELEMENTS
#undef HDMI

    map_drop_down_to_index(ui_dro_resolution_hdmi, config.settings.hdmi.resolution, hdmi_index, 11, 0);
}

static void save_hdmi_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_MAP(hdmi, resolution, "settings/hdmi/resolution", hdmi_index, 11, 0);
    CHECK_AND_SAVE_STD(hdmi, space, "settings/hdmi/space", INT, 0);
    CHECK_AND_SAVE_STD(hdmi, depth, "settings/hdmi/depth", INT, 0);
    CHECK_AND_SAVE_STD(hdmi, range, "settings/hdmi/range", INT, 0);
    CHECK_AND_SAVE_STD(hdmi, scan, "settings/hdmi/scan", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *hdmi_resolution[] = {
        "480i",        "576i",       "480p", "576p",
        "720p + 50hz", "720p + 60hz"
        //"1080i + 50hz",  "1080i + 60hz",  "1080p + 24hz", "1080p + 50hz", "1080p + 60hz",
    };

    char *hdmi_space[] = {"RGB", "YUV444", "YUV422", "YUV420"};

    char *hdmi_range[] = {lang.muxhdmi.colour.range.limited, lang.muxhdmi.colour.range.full};

    char *hdmi_scan[] = {lang.muxhdmi.scan_scale.over, lang.muxhdmi.scan_scale.under};

    INIT_OPTION_ITEM(-1, hdmi, resolution, lang.muxhdmi.resolution, "resolution", hdmi_resolution, 6);
    INIT_OPTION_ITEM(-1, hdmi, space, lang.muxhdmi.colour.space, "space", hdmi_space, 4);
    INIT_OPTION_ITEM(-1, hdmi, depth, lang.muxhdmi.colour.depth, "depth", NULL, 0);
    INIT_OPTION_ITEM(-1, hdmi, range, lang.muxhdmi.colour.range.title, "range", hdmi_range, 2);
    INIT_OPTION_ITEM(-1, hdmi, scan, lang.muxhdmi.scan_scale.title, "scan", hdmi_scan, 2);

    char *depth_string = generate_number_string(8, 16, 2, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_lbl_depth_hdmi, ui_dro_depth_hdmi, depth_string);
    free(depth_string);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    save_hdmi_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "hdmi");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.save, 0},
                                  {NULL, NULL, 0}});

#define HDMI(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_hdmi, UDATA);
    HDMI_ELEMENTS
#undef HDMI

    overlay_display();
}

int muxhdmi_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxhdmi.title);
    init_muxhdmi(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_hdmi_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
