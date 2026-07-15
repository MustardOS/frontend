#include "muxshare.h"
#include "ui/ui_muxoverlay.h"

#define OVERLAY(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(OVERLAY_ELEMENTS) };
#undef OVERLAY

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static int is_dir = 0;
static int is_app = 0;

static int core_is_retroarch = 0;

static mux_dialogue assign_dlg;
static int assign_dialogue_active = 0;

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define OVERLAY(NAME, UDATA) {UDATA, lang.muxoverlay.help.NAME},
        OVERLAY_ELEMENTS
#undef OVERLAY
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int effective_field(char *sys_dir, const char *file_name, const int line, const int compiled_default) {
    const char *val = get_content_line(sys_dir, file_name, "ovl", (size_t) line);
    if (!*val) val = get_content_line(sys_dir, NULL, "ovl", (size_t) line);

    return safe_atoi(val, compiled_default);
}

static void restore_tweak_options(void) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", rom_dir, rom_name);

    char *sys_dir = get_content_path(file_path);
    const char *file_name = get_file_name(file_path);

    const int gen_alpha = effective_field(sys_dir, file_name, 1, config.settings.overlay.gen_alpha);
    const int gen_anchor = effective_field(sys_dir, file_name, 2, config.settings.overlay.gen_anchor);
    const int gen_scale = effective_field(sys_dir, file_name, 3, config.settings.overlay.gen_scale);

    const int bat_alpha = effective_field(sys_dir, file_name, 4, config.settings.overlay.bat_alpha);
    const int bat_anchor = effective_field(sys_dir, file_name, 5, config.settings.overlay.bat_anchor);
    const int bat_scale = effective_field(sys_dir, file_name, 6, config.settings.overlay.bat_scale);

    const int vol_alpha = effective_field(sys_dir, file_name, 7, config.settings.overlay.vol_alpha);
    const int vol_anchor = effective_field(sys_dir, file_name, 8, config.settings.overlay.vol_anchor);
    const int vol_scale = effective_field(sys_dir, file_name, 9, config.settings.overlay.vol_scale);

    const int bri_alpha = effective_field(sys_dir, file_name, 10, config.settings.overlay.bri_alpha);
    const int bri_anchor = effective_field(sys_dir, file_name, 11, config.settings.overlay.bri_anchor);
    const int bri_scale = effective_field(sys_dir, file_name, 12, config.settings.overlay.bri_scale);

    free(sys_dir);

    lv_dropdown_set_selected(ui_dro_gen_alpha_overlay, int_to_pct(gen_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_gen_anchor_overlay, gen_anchor);
    lv_dropdown_set_selected(ui_dro_gen_scale_overlay, gen_scale);

    lv_dropdown_set_selected(ui_dro_bat_alpha_overlay, int_to_pct(bat_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_bat_anchor_overlay, bat_anchor);
    lv_dropdown_set_selected(ui_dro_bat_scale_overlay, bat_scale);

    lv_dropdown_set_selected(ui_dro_vol_alpha_overlay, int_to_pct(vol_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_vol_anchor_overlay, vol_anchor);
    lv_dropdown_set_selected(ui_dro_vol_scale_overlay, vol_scale);

    lv_dropdown_set_selected(ui_dro_bri_alpha_overlay, int_to_pct(bri_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_bri_anchor_overlay, bri_anchor);
    lv_dropdown_set_selected(ui_dro_bri_scale_overlay, bri_scale);
}

static void save_tweak_options(const enum gen_type method) {
    char blob[MAX_BUFFER_SIZE];
    snprintf(
        blob, sizeof(blob), "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d",
        pct_to_int(lv_dropdown_get_selected(ui_dro_gen_alpha_overlay), 0, 255),
        (int) lv_dropdown_get_selected(ui_dro_gen_anchor_overlay),
        (int) lv_dropdown_get_selected(ui_dro_gen_scale_overlay),
        pct_to_int(lv_dropdown_get_selected(ui_dro_bat_alpha_overlay), 0, 255),
        (int) lv_dropdown_get_selected(ui_dro_bat_anchor_overlay),
        (int) lv_dropdown_get_selected(ui_dro_bat_scale_overlay),
        pct_to_int(lv_dropdown_get_selected(ui_dro_vol_alpha_overlay), 0, 255),
        (int) lv_dropdown_get_selected(ui_dro_vol_anchor_overlay),
        (int) lv_dropdown_get_selected(ui_dro_vol_scale_overlay),
        pct_to_int(lv_dropdown_get_selected(ui_dro_bri_alpha_overlay), 0, 255),
        (int) lv_dropdown_get_selected(ui_dro_bri_anchor_overlay),
        (int) lv_dropdown_get_selected(ui_dro_bri_scale_overlay)
    );

    create_marker_assignment("ovl", "Assign Overlay Options", blob, rom_name, rom_dir, is_app, method);
}

static void detect_core_is_retroarch(void) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", rom_dir, rom_name);

    char *sys_dir = get_content_path(file_path);
    const char *file_name = get_file_name(file_path);

    const char *core_file = get_content_line(sys_dir, file_name, "cfg", 2);
    const char *core_dir = get_content_line(sys_dir, NULL, "cfg", 1);

    const char *def_core = get_content_line(sys_dir, file_name, "cfg", 6);
    const char *assign_sys = get_content_line(sys_dir, file_name, "cfg", 3);
    if (!*def_core) {
        def_core = get_content_line(sys_dir, NULL, "cfg", 5);
        assign_sys = get_content_line(sys_dir, NULL, "cfg", 2);
    }

    char assign_dir[MAX_BUFFER_SIZE];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN "/%s", assign_sys);
    const int core_uses_pickles = *def_core && *assign_sys ? core_uses_muxretro(assign_dir, def_core) : 0;

    const char *core_value = *core_file ? core_file : core_dir;
    const char *core_label = *core_value ? format_core_name(core_value, 0, core_uses_pickles) : "";
    core_is_retroarch = core_label && strcasestr(core_label, "RetroArch") ? 1 : 0;

    free(sys_dir);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *anchor_options[] = {
        lang.muxoverlay.anchor.top.left,    lang.muxoverlay.anchor.top.middle,    lang.muxoverlay.anchor.top.right,
        lang.muxoverlay.anchor.centre.left, lang.muxoverlay.anchor.centre.middle, lang.muxoverlay.anchor.centre.right,
        lang.muxoverlay.anchor.bottom.left, lang.muxoverlay.anchor.bottom.middle, lang.muxoverlay.anchor.bottom.right,
    };

    char *scale_options[] = {lang.muxoverlay.scale.original, lang.muxoverlay.scale.fit, lang.muxoverlay.scale.stretch};

    INIT_OPTION_ITEM(-1, overlay, gen_alpha, lang.muxoverlay.genalpha, "gen_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, gen_anchor, lang.muxoverlay.genanchor, "gen_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, gen_scale, lang.muxoverlay.genscale, "gen_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, bat_alpha, lang.muxoverlay.batalpha, "bat_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, bat_anchor, lang.muxoverlay.batanchor, "bat_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, bat_scale, lang.muxoverlay.batscale, "bat_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, vol_alpha, lang.muxoverlay.volalpha, "vol_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, vol_anchor, lang.muxoverlay.volanchor, "vol_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, vol_scale, lang.muxoverlay.volscale, "vol_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, bri_alpha, lang.muxoverlay.brialpha, "bri_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, bri_anchor, lang.muxoverlay.brianchor, "bri_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, bri_scale, lang.muxoverlay.briscale, "bri_scale", scale_options, 3);

    char *alpha_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_lbl_gen_alpha_overlay, ui_dro_gen_alpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_lbl_bat_alpha_overlay, ui_dro_bat_alpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_lbl_vol_alpha_overlay, ui_dro_vol_alpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_lbl_bri_alpha_overlay, ui_dro_bri_alpha_overlay, alpha_values);
    free(alpha_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (core_is_retroarch) {
        HIDE_OPTION_ITEM(overlay, gen_alpha);
        HIDE_OPTION_ITEM(overlay, gen_anchor);
        HIDE_OPTION_ITEM(overlay, gen_scale);
    }
}

static void handle_option_prev(void) {
    if (assign_dialogue_active) {
        dialogue_handle_dpad(&assign_dlg, &theme, -1, 1);
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (assign_dialogue_active) {
        dialogue_handle_dpad(&assign_dlg, &theme, +1, 1);
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (assign_dialogue_active) {
        const int method = assign_dlg.option_data[assign_dlg.selected];
        dialogue_dismiss(&assign_dialogue_active, &assign_dlg);

        if (method < 0) {
            play_sound(snd_back);
            mux_input_stop();
            return;
        }

        LOG_INFO(mux_module, "Overlay Options Assignment Triggered (method %d)", method);
        play_sound(snd_confirm);

        save_tweak_options((enum gen_type) method);

        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    dialogue_open(&assign_dialogue_active, &assign_dlg, &theme);
}

static void handle_b(void) {
    if (hold_call) return;

    if (assign_dialogue_active) {
        play_sound(snd_back);
        dialogue_dismiss(&assign_dialogue_active, &assign_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_down_hold();
}

static void handle_page_up(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_page_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || assign_dialogue_active) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define OVERLAY(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_overlay, UDATA);
    OVERLAY_ELEMENTS
#undef OVERLAY

    overlay_display();
}

void muxoverlay_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
    (void) auto_assign;

    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_dir = dir_exist(rom_dir) && !app;
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    is_app = app;

    init_module(__func__);

    if (is_app) {
        LOG_INFO(mux_module, "Assign Overlay Options APP_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Overlay Options APP_DIR: \"%s\"", rom_dir);
    } else {
        LOG_INFO(mux_module, "Assign Overlay Options ROM_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Overlay Options ROM_DIR: \"%s\"", rom_dir);
        LOG_INFO(mux_module, "Assign Overlay Options ROM_SYS: \"%s\"", rom_system);
    }

    detect_core_is_retroarch();

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxoverlay.title);
    init_muxoverlay(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();

    dialogue_init_assign_scope(
        &assign_dlg, &theme, ui_screen, lang.muxoption.overlay, is_dir, is_app, at_base(rom_dir, MAIN_ROM_DIR),
        lang.generic.select, lang.generic.cancel
    );

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
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_page_up,
            [mux_input_r1] = handle_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);
}
