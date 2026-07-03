#include "muxshare.h"
#include "ui/ui_muxconfig.h"

#define CONFIG(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(CONFIG_ELEMENTS) };
#undef CONFIG

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define CONFIG(NAME, UDATA) {UDATA, lang.muxconfig.help.NAME},
        CONFIG_ELEMENTS
#undef CONFIG
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int storage_available(void) {
    return is_partition_mounted(device.storage.sdcard.mount);
}

static int overlay_available(void) {
    return !hdmi_mode && config.settings.advanced.stage_overlay;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_STATIC_ITEM(-1, config, general, lang.muxconfig.general, "general", 0);
    INIT_STATIC_ITEM(-1, config, connect, lang.muxconfig.connect, "connect", 0);
    INIT_STATIC_ITEM(-1, config, custom, lang.muxconfig.custom, "custom", 0);
    INIT_STATIC_ITEM(-1, config, interface, lang.muxconfig.interface, "interface", 0);
    INIT_STATIC_ITEM(-1, config, overlay, lang.muxconfig.overlay, "overlay", 0);
    INIT_STATIC_ITEM(-1, config, language, lang.muxconfig.language, "language", 0);
    INIT_STATIC_ITEM(-1, config, power, lang.muxconfig.power, "power", 0);
    INIT_STATIC_ITEM(-1, config, storage, lang.muxconfig.storage, "storage", 0);
    INIT_STATIC_ITEM(-1, config, backup, lang.muxconfig.backup, "backup", 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);

    if (!storage_available()) HIDE_STATIC_ITEM(config, storage);
    if (hdmi_mode || !config.settings.advanced.stage_overlay) HIDE_STATIC_ITEM(config, overlay);

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 1, 0, 1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[] = {
        {"tweakgen", &kiosk.setting.general, NULL},
        {"connect", &kiosk.config.connectivity, NULL},
        {"custom", &kiosk.config.customisation, NULL},
        {"visual", &kiosk.setting.visual, NULL},
        {"overlay", &kiosk.setting.overlay, overlay_available},
        {"language", &kiosk.config.language, NULL},
        {"power", &kiosk.setting.power, NULL},
        {"storage", &kiosk.config.storage, storage_available},
        {"backup", &kiosk.config.backup, NULL},
    };

    const menu_entry *visible_entries[ui_count_dynamic];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    play_sound(snd_confirm);
    load_mux(entry->mux_name);

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    config_auth = 0;

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "config");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define CONFIG(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_config, UDATA);
    CONFIG_ELEMENTS
#undef CONFIG

    overlay_display();
}

int muxconfig_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxconfig.title);
    init_muxconfig(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
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
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
