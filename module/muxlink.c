#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxlink.h"

#define LINK(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(LINK_ELEMENTS) };
#undef LINK

#define LINK_STATE_FILE RUN_PATH "network/link"
#define LINK_FIELD_MAX  64

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

enum { row_enabled = 0, row_status, row_interface, row_address, row_peer_address, row_peer_mac };

struct link_report {
    char status[LINK_FIELD_MAX];
    char interface[LINK_FIELD_MAX];
    char address[LINK_FIELD_MAX];
    char peer_address[LINK_FIELD_MAX];
    char peer_mac[LINK_FIELD_MAX];
};

static struct link_report report;
static int settings_changed = 0;
static int nav_action_shown = -1;

static mux_dialogue save_dlg;

static void store_field(const char *key, const char *value) {
    struct {
        const char *key;
        char *target;
    } fields[] = {
        {"status", report.status},     {"interface", report.interface},
        {"address", report.address},   {"peer_address", report.peer_address},
        {"peer_mac", report.peer_mac},
    };

    for (size_t i = 0; i < A_SIZE(fields); i++) {
        if (strcmp(key, fields[i].key) != 0) continue;
        snprintf(fields[i].target, LINK_FIELD_MAX, "%s", value);
        return;
    }
}

static void read_link_report(void) {
    memset(&report, 0, sizeof(report));

    FILE *file = fopen(LINK_STATE_FILE, "r");
    if (!file) return;

    char line[160];
    while (fgets(line, sizeof(line), file)) {
        char *separator = strchr(line, '=');
        if (!separator) continue;

        *separator = '\0';
        char *value = separator + 1;
        value[strcspn(value, "\r\n")] = '\0';

        store_field(line, value);
    }

    fclose(file);
}

static const char *status_label(void) {
    if (!config.settings.network.link) return lang.generic.disabled;
    if (strcmp(report.status, "paired") == 0) return lang.muxlink.status_paired;
    if (strcmp(report.status, "waiting") == 0) return lang.muxlink.status_waiting;
    if (strcmp(report.status, "unplugged") == 0) return lang.muxlink.status_unplugged;

    return lang.muxlink.status_absent;
}

static const char *field_or_dash(const char *value) {
    return value[0] && strcmp(value, "-") != 0 ? value : lang.generic.unknown;
}

static void set_value(const int row, const char *text) {
    if (strcmp(lv_label_get_text(ui_objects_value[row]), text) == 0) return;
    lv_label_set_text(ui_objects_value[row], text);
}

static void refresh_values(void) {
    read_link_report();

    const int paired = config.settings.network.link && strcmp(report.status, "paired") == 0;

    set_value(row_enabled, config.settings.network.link ? lang.generic.enabled : lang.generic.disabled);
    set_value(row_status, status_label());
    set_value(row_interface, field_or_dash(report.interface));
    set_value(row_address, paired ? field_or_dash(report.address) : lang.generic.unknown);
    set_value(row_peer_address, paired ? field_or_dash(report.peer_address) : lang.generic.unknown);
    set_value(row_peer_mac, paired ? field_or_dash(report.peer_mac) : lang.generic.unknown);
}

static void refresh_nav(void) {
    const int actionable = current_item_index == row_enabled;

    if (nav_action_shown == actionable) return;
    nav_action_shown = actionable;

    nav_show_a(actionable, lang.generic.change);
}

static void cycle_enabled(void) {
    config.settings.network.link = !config.settings.network.link;
    settings_changed = 1;

    play_sound(snd_navigate);
    refresh_values();
}

static void save_settings(void) {
    write_text_to_file(CONF_CONFIG_PATH "settings/network/link", "w", INT, config.settings.network.link);
    settings_changed = 0;
}

static void leave_module(void) {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "link");
    mux_input_stop();
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
        {"enabled", lang.muxlink.help.enabled},          {"status", lang.muxlink.help.status},
        {"interface", lang.muxlink.help.interface},      {"address", lang.muxlink.help.address},
        {"peeraddress", lang.muxlink.help.peer_address}, {"peermac", lang.muxlink.help.peer_mac},
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;
    if (dialogue_active(&save_dlg)) return;
    if (current_item_index == row_enabled) cycle_enabled();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (dialogue_active(&save_dlg)) {
        dialogue_mark_cancelled(&save_dlg);
        dialogue_dismiss(&save_dlg);
        return;
    }

    play_sound(snd_back);

    if (settings_changed) {
        dialogue_open(&save_dlg, &theme);
        return;
    }

    leave_module();
}

static void handle_confirm_dialogue(void) {
    const mux_confirm_opt opt = (mux_confirm_opt) save_dlg.selected;
    dialogue_dismiss(&save_dlg);

    if (opt == mux_confirm_yep) save_settings();
    leave_module();
}

static void handle_left(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }
    if (current_item_index == row_enabled) cycle_enabled();
}

static void handle_right(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }
    if (current_item_index == row_enabled) cycle_enabled();
}

static void handle_up(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }
    handle_list_nav_up();
}

static void handle_down(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
        return;
    }
    handle_list_nav_down();
}

static void handle_up_hold(void) {
    if (!dialogue_active(&save_dlg)) handle_list_nav_up_hold();
}

static void handle_down_hold(void) {
    if (!dialogue_active(&save_dlg)) handle_list_nav_down_hold();
}

static void handle_start(void) {
    if (dialogue_active(&save_dlg)) handle_confirm_dialogue();
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || dialogue_active(&save_dlg)) return;

    play_sound(snd_info_open);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]
    ){ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_progress_brightness, ui_pnl_progress_volume, ui_pnl_message,
      NULL});
}

static void init_navigation_group(void) {
    INIT_VALUE_ITEM(-1, link, enabled, lang.muxlink.enabled, "enabled", "");
    INIT_VALUE_ITEM(-1, link, status, lang.muxlink.status, "status", "");
    INIT_VALUE_ITEM(-1, link, interface, lang.muxlink.interface, "interface", "");
    INIT_VALUE_ITEM(-1, link, address, lang.muxlink.address, "address", "");
    INIT_VALUE_ITEM(-1, link, peer_address, lang.muxlink.peer_address, "peeraddress", "");
    INIT_VALUE_ITEM(-1, link, peer_mac, lang.muxlink.peer_mac, "peermac", "");

    lv_obj_set_user_data(ui_lbl_enabled_link, "enabled");
    lv_obj_set_user_data(ui_lbl_status_link, "status");
    lv_obj_set_user_data(ui_lbl_interface_link, "interface");
    lv_obj_set_user_data(ui_lbl_address_link, "address");
    lv_obj_set_user_data(ui_lbl_peer_address_link, "peeraddress");
    lv_obj_set_user_data(ui_lbl_peer_mac_link, "peermac");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    refresh_values();

    gen_step_movement(0, +1, 2, 0, 0);
    nav_refresh_list_overflow(ui_pnl_content);
    refresh_nav();

    nav_moved = 1;
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();
    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer) {
    ui_gen_refresh_task(timer);

    if (dialogue_active(&save_dlg)) return;

    refresh_values();
    refresh_nav();
}

int muxlink_main(void) {
    settings_changed = 0;
    nav_action_shown = -1;
    save_dlg.active = 0;

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxlink.title);
    init_muxlink(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts =
        {.swap_axis = theme.misc.navigation_type == 1,
         .press_handler =
             {
                 [mux_input_a] = handle_a,
                 [mux_input_b] = handle_b,
                 [mux_input_x] = handle_x,
                 [mux_input_dpad_up] = handle_up,
                 [mux_input_dpad_down] = handle_down,
                 [mux_input_dpad_left] = handle_left,
                 [mux_input_dpad_right] = handle_right,
                 [mux_input_start] = handle_start,
             },
         .release_handler =
             {
                 [mux_input_menu] = handle_help,
             },
         .hold_handler = {
             [mux_input_dpad_up] = handle_up_hold,
             [mux_input_dpad_down] = handle_down_hold,
         }};

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxlink.title, lang.muxlink.overview);

    mux_input_task(&input_opts);
    return 0;
}
