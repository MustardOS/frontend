#include "muxshare.h"
#include "ui/ui_muxconfig.h"

#define UI_COUNT 9

static void list_nav_move(int steps, int direction);

static void show_help() {
    struct help_msg help_messages[] = {
#define CONFIG(NAME, ENUM, UDATA) { ui_lbl##NAME##_config, lang.MUXCONFIG.HELP.ENUM },
            CONFIG_ELEMENTS
#undef CONFIG
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static int storage_available(void) {
    return is_partition_mounted(device.STORAGE.SDCARD.MOUNT);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_STATIC_ITEM(-1, config, General, lang.MUXCONFIG.GENERAL, "general", 0);
    INIT_STATIC_ITEM(-1, config, Connect, lang.MUXCONFIG.CONNECT, "connect", 0);
    INIT_STATIC_ITEM(-1, config, Custom, lang.MUXCONFIG.CUSTOM, "custom", 0);
    INIT_STATIC_ITEM(-1, config, Interface, lang.MUXCONFIG.INTERFACE, "interface", 0);
    INIT_STATIC_ITEM(-1, config, Overlay, lang.MUXCONFIG.OVERLAY, "overlay", 0);
    INIT_STATIC_ITEM(-1, config, Language, lang.MUXCONFIG.LANGUAGE, "language", 0);
    INIT_STATIC_ITEM(-1, config, Power, lang.MUXCONFIG.POWER, "power", 0);
    INIT_STATIC_ITEM(-1, config, Storage, lang.MUXCONFIG.STORAGE, "storage", 0);
    INIT_STATIC_ITEM(-1, config, Backup, lang.MUXCONFIG.BACKUP, "backup", 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, false);

    if (!storage_available()) HIDE_STATIC_ITEM(config, Storage);

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
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
            {"tweakgen", &kiosk.SETTING.GENERAL,      NULL},
            {"connect",  &kiosk.CONFIG.CONNECTIVITY,  NULL},
            {"custom",   &kiosk.CONFIG.CUSTOMISATION, NULL},
            {"visual",   &kiosk.SETTING.VISUAL,       NULL},
            {"overlay",  &kiosk.SETTING.OVERLAY,      NULL},
            {"language", &kiosk.CONFIG.LANGUAGE,      NULL},
            {"power",    &kiosk.SETTING.POWER,        NULL},
            {"storage",  &kiosk.CONFIG.STORAGE, storage_available},
            {"backup",   &kiosk.CONFIG.BACKUP,        NULL},
    };

    const menu_entry *visible_entries[UI_COUNT];
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

    play_sound(SND_CONFIRM);
    load_mux(entry->mux_name);

    close_input();
    mux_input_stop();
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "config");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define CONFIG(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_config, UDATA);
    CONFIG_ELEMENTS
#undef CONFIG

    overlay_display();
}

int muxconfig_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONFIG.TITLE);
    init_muxconfig(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
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
