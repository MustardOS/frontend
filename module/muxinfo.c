#include "muxshare.h"
#include "ui/ui_muxinfo.h"

#define INFO(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(INFO_ELEMENTS) };
#undef INFO

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define INFO(NAME, UDATA) {UDATA, lang.muxinfo.help.NAME},
        INFO_ELEMENTS
#undef INFO
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int visible_network_opt(void) {
    return device.board.has_network;
}

static int visible_chrony_opt(void) {
    return 0;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_STATIC_ITEM(-1, info, news, lang.muxinfo.news, "news", 0);
    INIT_STATIC_ITEM(-1, info, activity, lang.muxinfo.activity, "activity", 0);
    INIT_STATIC_ITEM(-1, info, screenshot, lang.muxinfo.screenshot, "screenshot", 0);
    INIT_STATIC_ITEM(-1, info, space, lang.muxinfo.space, "space", 0);
    INIT_STATIC_ITEM(-1, info, tester, lang.muxinfo.tester, "tester", 0);
    INIT_STATIC_ITEM(-1, info, sys_info, lang.muxinfo.sysinfo, "sysinfo", 0);
    INIT_STATIC_ITEM(-1, info, bat_info, lang.muxinfo.batinfo, "batinfo", 0);
    INIT_STATIC_ITEM(-1, info, net_info, lang.muxinfo.netinfo, "netinfo", 0);
    INIT_STATIC_ITEM(-1, info, chrony, lang.muxinfo.chrony, "chrony", 0);
    INIT_STATIC_ITEM(-1, info, credit, lang.muxinfo.credit, "credit", 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);

    if (!visible_network_opt()) {
        HIDE_STATIC_ITEM(info, news);
        HIDE_STATIC_ITEM(info, net_info);
    }

    // Hide until further notice or future development
    HIDE_STATIC_ITEM(info, chrony);

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 1, 0, 1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    typedef enum {
        menu_general = 0,
        menu_news,
        menu_credits,
    } menu_action;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        menu_action action;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"news", menu_news, visible_network_opt},
        {"activity", menu_general, NULL},
        {"screenshot", menu_general, NULL},
        {"space", menu_general, NULL},
        {"tester", menu_general, NULL},
        {"sysinfo", menu_general, NULL},
        {"batinfo", menu_general, NULL},
        {"netinfo", menu_general, visible_network_opt},
        {"chrony", menu_general, visible_chrony_opt},
        {"credits", menu_credits, NULL},
    };

    const menu_entry *visible_entries[ui_count_dynamic];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    switch (entry->action) {
        case menu_general:
            play_sound(snd_confirm);
            load_mux(entry->mux_name);

            mux_input_stop();
            break;
        case menu_news:
            if (is_network_connected()) {
                play_sound(snd_confirm);
                load_mux(entry->mux_name);

                mux_input_stop();
            } else {
                play_sound(snd_error);
                toast_message(lang.generic.need_connect, tst_wait_m);
            }
            break;
        case menu_credits:
            play_sound(snd_confirm);

            fade_out_screen();
            load_mux(entry->mux_name);

            mux_input_stop();
            break;
        default:
            break;
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "info");

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

#define INFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_info, UDATA);
    INFO_ELEMENTS
#undef INFO

    overlay_display();
}

int muxinfo_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxinfo.title);
    init_muxinfo(ui_pnl_content);
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
