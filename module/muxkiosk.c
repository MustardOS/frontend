#include "muxshare.h"
#include "ui/ui_muxkiosk.h"

#define KIOSK(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(KIOSK_ELEMENTS) };
#undef KIOSK

#define KIOSK(NAME, UDATA) static int NAME##_original;
KIOSK_ELEMENTS
#undef KIOSK

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define KIOSK(NAME, UDATA) {UDATA, lang.muxkiosk.help.NAME},
        KIOSK_ELEMENTS
#undef KIOSK
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define KIOSK(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_kiosk);
    KIOSK_ELEMENTS
#undef KIOSK
}

static void restore_kiosk_options(void) {
    lv_dropdown_set_selected(ui_dro_enable_kiosk, kiosk.enable);
    lv_dropdown_set_selected(ui_dro_message_kiosk, kiosk.message);
    lv_dropdown_set_selected(ui_dro_archive_kiosk, kiosk.application.archive);
    lv_dropdown_set_selected(ui_dro_task_kiosk, kiosk.application.task);
    lv_dropdown_set_selected(ui_dro_custom_kiosk, kiosk.config.customisation);
    lv_dropdown_set_selected(ui_dro_language_kiosk, kiosk.config.language);
    lv_dropdown_set_selected(ui_dro_network_kiosk, kiosk.config.network);
    lv_dropdown_set_selected(ui_dro_storage_kiosk, kiosk.config.storage);
    lv_dropdown_set_selected(ui_dro_backup_kiosk, kiosk.config.backup);
    lv_dropdown_set_selected(ui_dro_net_adv_kiosk, kiosk.config.net_settings);
    lv_dropdown_set_selected(ui_dro_web_serv_kiosk, kiosk.config.web_services);
    lv_dropdown_set_selected(ui_dro_core_kiosk, kiosk.content.core);
    lv_dropdown_set_selected(ui_dro_governor_kiosk, kiosk.content.governor);
    lv_dropdown_set_selected(ui_dro_control_kiosk, kiosk.content.control);
    lv_dropdown_set_selected(ui_dro_option_kiosk, kiosk.content.option);
    lv_dropdown_set_selected(ui_dro_retro_arch_kiosk, kiosk.content.retroarch);
    lv_dropdown_set_selected(ui_dro_search_kiosk, kiosk.content.search);
    lv_dropdown_set_selected(ui_dro_tag_kiosk, kiosk.content.tag);
    lv_dropdown_set_selected(ui_dro_col_filter_kiosk, kiosk.content.colfilter);
    lv_dropdown_set_selected(ui_dro_shader_kiosk, kiosk.content.shader);
    lv_dropdown_set_selected(ui_dro_rem_config_kiosk, kiosk.content.remconfig);
    lv_dropdown_set_selected(ui_dro_catalogue_kiosk, kiosk.custom.catalogue);
    lv_dropdown_set_selected(ui_dro_ra_config_kiosk, kiosk.custom.raconfig);
    lv_dropdown_set_selected(ui_dro_theme_kiosk, kiosk.custom.theme);
    lv_dropdown_set_selected(ui_dro_theme_down_kiosk, kiosk.custom.theme_down);
    lv_dropdown_set_selected(ui_dro_clock_kiosk, kiosk.datetime.clock);
    lv_dropdown_set_selected(ui_dro_timezone_kiosk, kiosk.datetime.timezone);
    lv_dropdown_set_selected(ui_dro_apps_kiosk, kiosk.launch.application);
    lv_dropdown_set_selected(ui_dro_config_kiosk, kiosk.launch.configuration);
    lv_dropdown_set_selected(ui_dro_explore_kiosk, kiosk.launch.explore);
    lv_dropdown_set_selected(ui_dro_collect_mod_kiosk, kiosk.launch.collection);
    lv_dropdown_set_selected(ui_dro_collect_add_kiosk, kiosk.collect.add_con);
    lv_dropdown_set_selected(ui_dro_collect_new_kiosk, kiosk.collect.new_dir);
    lv_dropdown_set_selected(ui_dro_collect_rem_kiosk, kiosk.collect.remove);
    lv_dropdown_set_selected(ui_dro_collect_acc_kiosk, kiosk.collect.access);
    lv_dropdown_set_selected(ui_dro_history_mod_kiosk, kiosk.launch.history);
    lv_dropdown_set_selected(ui_dro_history_rem_kiosk, kiosk.content.history);
    lv_dropdown_set_selected(ui_dro_info_kiosk, kiosk.launch.information);
    lv_dropdown_set_selected(ui_dro_rgb_kiosk, kiosk.setting.rgb);
    lv_dropdown_set_selected(ui_dro_advanced_kiosk, kiosk.setting.advanced);
    lv_dropdown_set_selected(ui_dro_general_kiosk, kiosk.setting.general);
    lv_dropdown_set_selected(ui_dro_hdmi_kiosk, kiosk.setting.hdmi);
    lv_dropdown_set_selected(ui_dro_power_kiosk, kiosk.setting.power);
    lv_dropdown_set_selected(ui_dro_visual_kiosk, kiosk.setting.visual);
    lv_dropdown_set_selected(ui_dro_overlay_kiosk, kiosk.setting.overlay);
}

static void save_kiosk_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_KSK(kiosk, enable, "enable", INT);
    CHECK_AND_SAVE_KSK(kiosk, message, "message", INT);
    CHECK_AND_SAVE_KSK(kiosk, archive, "application/archive", INT);
    CHECK_AND_SAVE_KSK(kiosk, task, "application/task", INT);
    CHECK_AND_SAVE_KSK(kiosk, custom, "config/custom", INT);
    CHECK_AND_SAVE_KSK(kiosk, language, "config/language", INT);
    CHECK_AND_SAVE_KSK(kiosk, network, "config/network", INT);
    CHECK_AND_SAVE_KSK(kiosk, storage, "config/storage", INT);
    CHECK_AND_SAVE_KSK(kiosk, backup, "config/backup", INT);
    CHECK_AND_SAVE_KSK(kiosk, net_adv, "config/netadv", INT);
    CHECK_AND_SAVE_KSK(kiosk, web_serv, "config/webserv", INT);
    CHECK_AND_SAVE_KSK(kiosk, core, "content/core", INT);
    CHECK_AND_SAVE_KSK(kiosk, governor, "content/governor", INT);
    CHECK_AND_SAVE_KSK(kiosk, control, "content/control", INT);
    CHECK_AND_SAVE_KSK(kiosk, tag, "content/tag", INT);
    CHECK_AND_SAVE_KSK(kiosk, col_filter, "content/colfilter", INT);
    CHECK_AND_SAVE_KSK(kiosk, shader, "content/shader", INT);
    CHECK_AND_SAVE_KSK(kiosk, rem_config, "content/remconfig", INT);
    CHECK_AND_SAVE_KSK(kiosk, option, "content/option", INT);
    CHECK_AND_SAVE_KSK(kiosk, retro_arch, "content/retroarch", INT);
    CHECK_AND_SAVE_KSK(kiosk, search, "content/search", INT);
    CHECK_AND_SAVE_KSK(kiosk, history_rem, "content/history", INT);
    CHECK_AND_SAVE_KSK(kiosk, collect_add, "collect/add_con", INT);
    CHECK_AND_SAVE_KSK(kiosk, collect_new, "collect/new_dir", INT);
    CHECK_AND_SAVE_KSK(kiosk, collect_rem, "collect/remove", INT);
    CHECK_AND_SAVE_KSK(kiosk, collect_acc, "collect/access", INT);
    CHECK_AND_SAVE_KSK(kiosk, catalogue, "custom/catalogue", INT);
    CHECK_AND_SAVE_KSK(kiosk, ra_config, "custom/raconfig", INT);
    CHECK_AND_SAVE_KSK(kiosk, theme, "custom/theme", INT);
    CHECK_AND_SAVE_KSK(kiosk, theme_down, "custom/theme_down", INT);
    CHECK_AND_SAVE_KSK(kiosk, clock, "datetime/clock", INT);
    CHECK_AND_SAVE_KSK(kiosk, timezone, "datetime/timezone", INT);
    CHECK_AND_SAVE_KSK(kiosk, apps, "launch/apps", INT);
    CHECK_AND_SAVE_KSK(kiosk, config, "launch/config", INT);
    CHECK_AND_SAVE_KSK(kiosk, explore, "launch/explore", INT);
    CHECK_AND_SAVE_KSK(kiosk, collect_mod, "launch/collection", INT);
    CHECK_AND_SAVE_KSK(kiosk, history_mod, "launch/history", INT);
    CHECK_AND_SAVE_KSK(kiosk, info, "launch/info", INT);
    CHECK_AND_SAVE_KSK(kiosk, rgb, "setting/rgb", INT);
    CHECK_AND_SAVE_KSK(kiosk, advanced, "setting/advanced", INT);
    CHECK_AND_SAVE_KSK(kiosk, general, "setting/general", INT);
    CHECK_AND_SAVE_KSK(kiosk, hdmi, "setting/hdmi", INT);
    CHECK_AND_SAVE_KSK(kiosk, power, "setting/power", INT);
    CHECK_AND_SAVE_KSK(kiosk, visual, "setting/visual", INT);
    CHECK_AND_SAVE_KSK(kiosk, overlay, "setting/overlay", INT);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);

        if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
        if (file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);

        refresh_kiosk = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_OPTION_ITEM(-1, kiosk, enable, lang.muxkiosk.enable, "enable", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, kiosk, message, lang.muxkiosk.message, "message", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, kiosk, archive, lang.muxkiosk.archive, "archive", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, task, lang.muxkiosk.task, "task", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, custom, lang.muxkiosk.custom, "custom", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, language, lang.muxkiosk.language, "language", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, network, lang.muxkiosk.network, "network", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, storage, lang.muxkiosk.storage, "storage", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, backup, lang.muxkiosk.backup, "backup", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, net_adv, lang.muxkiosk.netadv, "netadv", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, web_serv, lang.muxkiosk.webserv, "webserv", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, core, lang.muxkiosk.core, "core", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, governor, lang.muxkiosk.governor, "governor", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, control, lang.muxkiosk.control, "control", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, option, lang.muxkiosk.option, "option", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, retro_arch, lang.muxkiosk.retroarch, "retroarch", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, search, lang.muxkiosk.search, "search", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, tag, lang.muxkiosk.tag, "tag", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, col_filter, lang.muxkiosk.colfilter, "colfilter", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, shader, lang.muxkiosk.shader, "shader", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, rem_config, lang.muxkiosk.remconfig, "remconfig", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, catalogue, lang.muxkiosk.catalogue, "catalogue", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, ra_config, lang.muxkiosk.raconfig, "raconfig", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, theme, lang.muxkiosk.theme, "theme", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, theme_down, lang.muxkiosk.themedown, "theme_down", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, clock, lang.muxkiosk.clock, "clock", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, timezone, lang.muxkiosk.timezone, "timezone", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, apps, lang.muxkiosk.apps, "apps", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, config, lang.muxkiosk.config, "config", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, explore, lang.muxkiosk.explore, "explore", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, collect_mod, lang.muxkiosk.collection.main, "collectmod", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, collect_add, lang.muxkiosk.collection.add_content, "collectadd", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, collect_new, lang.muxkiosk.collection.new_dir, "collectnew", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, collect_rem, lang.muxkiosk.collection.remove, "collectrem", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, collect_acc, lang.muxkiosk.collection.access, "collectacc", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, history_mod, lang.muxkiosk.history.main, "historymod", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, history_rem, lang.muxkiosk.history.remove, "historyrem", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, info, lang.muxkiosk.info, "info", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, rgb, lang.muxkiosk.rgb, "rgb", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, advanced, lang.muxkiosk.advanced, "advanced", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, general, lang.muxkiosk.general, "general", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, hdmi, lang.muxkiosk.hdmi, "hdmi", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, power, lang.muxkiosk.power, "power", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, visual, lang.muxkiosk.visual, "visual", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, overlay, lang.muxkiosk.overlay, "overlay", allowed_restricted, 2);

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

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }
    play_sound(snd_back);

    save_kiosk_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "launcher");

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

#define KIOSK(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_kiosk, UDATA);
    KIOSK_ELEMENTS
#undef KIOSK

    overlay_display();
}

int muxkiosk_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxkiosk.title);
    init_muxkiosk(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_kiosk_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_option_next,
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
