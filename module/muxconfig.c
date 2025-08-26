#include "muxshare.h"
#include "ui/ui_muxconfig.h"

#define UI_COUNT 8

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblGeneral_config,   lang.MUXCONFIG.HELP.GENERAL},
            {ui_lblConnect_config,   lang.MUXCONFIG.HELP.CONNECTIVITY},
            {ui_lblCustom_config,    lang.MUXCONFIG.HELP.CUSTOM},
            {ui_lblInterface_config, lang.MUXCONFIG.HELP.VISUAL},
            {ui_lblLanguage_config,  lang.MUXCONFIG.HELP.LANGUAGE},
            {ui_lblPower_config,     lang.MUXCONFIG.HELP.POWER},
            {ui_lblStorage_config,   lang.MUXCONFIG.HELP.STORAGE},
            {ui_lblBackup_config,    lang.MUXCONFIG.HELP.BACKUP},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_STATIC_ITEM(-1, config, General, lang.MUXCONFIG.GENERAL, "general", 0);
    INIT_STATIC_ITEM(-1, config, Connect, lang.MUXCONFIG.CONNECTIVITY, "connect", 0);
    INIT_STATIC_ITEM(-1, config, Custom, lang.MUXCONFIG.CUSTOM, "custom", 0);
    INIT_STATIC_ITEM(-1, config, Interface, lang.MUXCONFIG.VISUAL, "interface", 0);
    INIT_STATIC_ITEM(-1, config, Language, lang.MUXCONFIG.LANGUAGE, "language", 0);
    INIT_STATIC_ITEM(-1, config, Power, lang.MUXCONFIG.POWER, "power", 0);
    INIT_STATIC_ITEM(-1, config, Storage, lang.MUXCONFIG.STORAGE, "storage", 0);
    INIT_STATIC_ITEM(-1, config, Backup, lang.MUXCONFIG.BACKUP, "backup", 0);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }

    if (!is_partition_mounted(device.STORAGE.SDCARD.MOUNT)) HIDE_STATIC_ITEM(config, Storage);

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"general",   "tweakgen", &kiosk.SETTING.GENERAL},
            {"connect",   "connect",  &kiosk.CONFIG.CONNECTIVITY},
            {"custom",    "custom",   &kiosk.CONFIG.CUSTOMISATION},
            {"interface", "visual",   &kiosk.SETTING.VISUAL},
            {"language",  "language", &kiosk.CONFIG.LANGUAGE},
            {"power",     "power",    &kiosk.SETTING.POWER},
            {"storage",   "storage",  &kiosk.CONFIG.STORAGE},
            {"backup",    "backup",   &kiosk.CONFIG.BACKUP}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (is_ksk(*elements[i].kiosk_flag)) {
                kiosk_denied();
                return;
            }

            play_sound(SND_CONFIRM);
            load_mux(elements[i].mux_name);

            break;
        }
    }

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
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define CONFIG(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_config, UDATA);
    CONFIG_ELEMENTS
#undef CONFIG

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxconfig_main(void) {
    init_module("muxconfig");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONFIG.TITLE);
    init_muxconfig(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_refresh_task, NULL);

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
