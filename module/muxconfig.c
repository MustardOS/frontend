#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxconfig.h"

#define CONFIG(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(CONFIG_ELEMENTS) };
#undef CONFIG

static mux_dialogue warn_dlg;
static char warn_pending[64] = "";

static int on_general_row(void) {
    return lv_group_get_focused(ui_group) == ui_lbl_general_config;
}

static void show_warn_dialog(const char *target) {
    snprintf(warn_pending, sizeof(warn_pending), "%s", target);

    if (warn_dlg.description_label) {
        const char *desc = strcmp(target, "danger") == 0 ? lang.muxtweakgen.warn_danger : lang.muxtweakgen.warn;
        lv_label_set_text(warn_dlg.description_label, desc);
    }

    dialogue_open(&warn_dlg, &theme);
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (msgbox_active || hold_call || dialogue_active(&warn_dlg)) return;
    if (!on_general_row() || is_ksk(kiosk.setting.advanced)) return;

    show_warn_dialog("tweakadv");
}

static void handle_y_hold(void) {
    if (msgbox_active || dialogue_active(&warn_dlg)) return;
    if (!on_general_row() || is_ksk(kiosk.setting.advanced)) return;

    show_warn_dialog("danger");
}

static void check_focus(void) {
    if (on_general_row() && !is_ksk(kiosk.setting.advanced)) {
        lv_label_set_text(ui_lbl_nav_x, lang.muxtweakgen.advanced);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    footer_nav_check_scroll();
}

static void handle_dpad_up(void) {
    if (dialogue_active(&warn_dlg)) {
        dialogue_handle_dpad(&warn_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&warn_dlg)) {
        dialogue_handle_dpad(&warn_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_left(void) {
    if (dialogue_active(&warn_dlg)) dialogue_handle_dpad(&warn_dlg, &theme, -1, swap_axis);
}

static void handle_dpad_right(void) {
    if (dialogue_active(&warn_dlg)) dialogue_handle_dpad(&warn_dlg, &theme, +1, swap_axis);
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&warn_dlg)) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&warn_dlg)) return;

    handle_list_nav_down_hold();
}

static void handle_page_up(void) {
    if (dialogue_active(&warn_dlg)) return;

    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (dialogue_active(&warn_dlg)) return;

    handle_list_nav_page_down();
}

static void list_nav_prev(const int steps) {
    list_nav_cb_prev(steps);
    check_focus();
}

static void list_nav_next(const int steps) {
    list_nav_cb_next(steps);
    check_focus();
}

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

static int connectivity_available(void) {
    return device.board.has_network || device.board.has_bluetooth;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_STATIC_ITEM(-1, config, general, lang.muxconfig.general, "general", 0);
    INIT_STATIC_ITEM(-1, config, custom, lang.muxconfig.custom, "custom", 0);
    INIT_STATIC_ITEM(-1, config, connect, lang.muxconfig.connect, "connect", 0);
    INIT_STATIC_ITEM(-1, config, access, lang.muxconfig.access, "access", 0);
    INIT_STATIC_ITEM(-1, config, power, lang.muxconfig.power, "power", 0);
    INIT_STATIC_ITEM(-1, config, storage, lang.muxconfig.storage, "storage", 0);
    INIT_STATIC_ITEM(-1, config, backup, lang.muxconfig.backup, "backup", 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);

    if (!storage_available()) HIDE_STATIC_ITEM(config, storage);
    if (!connectivity_available()) HIDE_STATIC_ITEM(config, connect);

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 1, 0, 1);
    check_focus();
}

static void handle_a(void) {
    if (dialogue_active(&warn_dlg)) {
        const int idx = warn_dlg.selected;
        char target[64];
        snprintf(target, sizeof(target), "%s", warn_pending);

        dialogue_dismiss(&warn_dlg);
        warn_pending[0] = '\0';

        if (idx != 0) return;

        char c_path[MAX_BUFFER_SIZE];
        snprintf(c_path, sizeof(c_path), CONF_CONFIG_PATH "count/warn_%s", target);
        increment_counter_file(c_path);

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "config");

        load_mux(target);
        mux_input_stop();

        return;
    }

    if (msgbox_active || hold_call) return;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        visible_fn visible;
    } menu_entry;

    static int16_t kiosk_pass = 0;

    static const menu_entry entries[] = {
        {"tweakgen", &kiosk.setting.general, NULL},
        {"custom", &kiosk.config.customisation, NULL},
        {"connect", &kiosk.config.connectivity, connectivity_available},
        {"access", &kiosk_pass, NULL},
        {"power", &kiosk.setting.power, NULL},
        {"storage", &kiosk.config.storage, storage_available},
        {"backup", &kiosk.config.backup, NULL},
    };

    SELECT_VISIBLE_ENTRY(entries, entry);

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

    if (dialogue_active(&warn_dlg)) {
        dialogue_cancel(&warn_dlg);
        warn_pending[0] = '\0';
        return;
    }

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
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.muxtweakgen.advanced, 0},
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

    dialogue_init_warn(&warn_dlg, &theme, ui_screen, lang.muxtweakgen.warn, lang.generic.select, lang.generic.cancel);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_y] = handle_y_hold,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_page_up,
            [mux_input_r1] = handle_page_down,
        }
    };

    orientation_introduce(mux_module, lang.muxconfig.title, lang.muxconfig.overview);

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
