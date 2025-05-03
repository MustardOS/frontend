#include "muxshare.h"
#include "muxconnect.h"
#include "ui/ui_muxconnect.h"
#include <string.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int bluetooth_original, usbfunction_original;

#define UI_COUNT 4
static lv_obj_t *ui_objects[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblNetwork,     lang.MUXCONNECT.HELP.WIFI},
            {ui_lblServices,    lang.MUXCONNECT.HELP.WEB},
            {ui_lblBluetooth,   lang.MUXCONNECT.HELP.BLUETOOTH},
            {ui_lblUSBFunction, lang.MUXCONNECT.HELP.USB},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void init_dropdown_settings() {
    usbfunction_original = lv_dropdown_get_selected(ui_droUSBFunction);
    bluetooth_original = lv_dropdown_get_selected(ui_droBluetooth);
}

static void restore_options() {
    const char *usb_type = config.SETTINGS.ADVANCED.USBFUNCTION;
    if (!strcasecmp(usb_type, "adb")) {
        lv_dropdown_set_selected(ui_droUSBFunction, 1);
    } else if (!strcasecmp(usb_type, "mtp")) {
        lv_dropdown_set_selected(ui_droUSBFunction, 2);
    } else {
        lv_dropdown_set_selected(ui_droUSBFunction, 0);
    }
    lv_dropdown_set_selected(ui_droBluetooth, config.VISUAL.BLUETOOTH);
}

static void save_options() {
    char *idx_usbfunction;
    switch (lv_dropdown_get_selected(ui_droUSBFunction)) {
        case 1:
            idx_usbfunction = "adb";
            break;
        case 2:
            idx_usbfunction = "mtp";
            break;
        default:
            idx_usbfunction = "none";
            break;
    }

    int is_modified = 0;

    int idx_bluetooth = lv_dropdown_get_selected(ui_droBluetooth);
    if (lv_dropdown_get_selected(ui_droBluetooth) != bluetooth_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/bluetooth"), "w", INT, idx_bluetooth);
    }

    if (lv_dropdown_get_selected(ui_droUSBFunction) != usbfunction_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/usb_function"), "w", CHAR, idx_usbfunction);
    }

    if (is_modified > 0) {
        const char *args[] = {INTERNAL_PATH "script/mux/tweak.sh", NULL};
        run_exec(args, A_SIZE(args), 0);

        refresh_config = 1;
    }
}

static void add_connect_item(lv_obj_t *ui_pnl, lv_obj_t *ui_lbl, lv_obj_t *ui_ico, lv_obj_t *ui_dro, char *item_text,
                             char *glyph_name) {
    apply_theme_list_panel(ui_pnl);
    apply_theme_list_drop_down(&theme, ui_dro, "");
    apply_theme_list_item(&theme, ui_lbl, item_text);
    apply_theme_list_glyph(&theme, ui_ico, mux_module, glyph_name);

    lv_obj_set_user_data(ui_pnl, strdup(item_text));
    lv_group_add_obj(ui_group, ui_lbl);
    lv_group_add_obj(ui_group_value, ui_dro);
    lv_group_add_obj(ui_group_glyph, ui_ico);
    lv_group_add_obj(ui_group_panel, ui_pnl);

    apply_text_long_dot(&theme, ui_pnlContent, ui_lbl, item_text);
}

static void init_navigation_group() {
    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_objects[0] = ui_lblNetwork;
    ui_objects[1] = ui_lblServices;
    ui_objects[2] = ui_lblBluetooth;
    ui_objects[3] = ui_lblUSBFunction;
    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};

    add_connect_item(ui_pnlNetwork, ui_lblNetwork, ui_icoNetwork, ui_droNetwork_connect, lang.MUXCONNECT.WIFI,
                     "network");
    add_connect_item(ui_pnlServices, ui_lblServices, ui_icoServices, ui_droServices, lang.MUXCONNECT.WEB, "service");
    add_connect_item(ui_pnlBluetooth, ui_lblBluetooth, ui_icoBluetooth, ui_droBluetooth, lang.MUXCONNECT.BLUETOOTH,
                     "bluetooth");
    add_drop_down_options(ui_droBluetooth, disabled_enabled, 2);
    add_connect_item(ui_pnlUSBFunction, ui_lblUSBFunction, ui_icoUSBFunction, ui_droUSBFunction, lang.MUXCONNECT.USB,
                     "usbfunction");
    add_drop_down_options(ui_droUSBFunction, (char *[]) {lang.GENERIC.DISABLED, "ADB", "MTP"}, 3);

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlNetwork, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNetwork, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoNetwork, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_droNetwork_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlServices, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblServices, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoServices, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_droServices, LV_OBJ_FLAG_HIDDEN);
        ui_count -= 2;
    }

    if (!device.DEVICE.HAS_BLUETOOTH || true) { //TODO: remove true when bluetooth is implemented
        lv_obj_add_flag(ui_pnlBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_droBluetooth, LV_OBJ_FLAG_HIDDEN);
        ui_count -= 1;
    }
}

static void list_nav_move(int steps, int direction) {
    play_sound(SND_NAVIGATE, 0);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound(SND_NAVIGATE, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound(SND_NAVIGATE, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_a() {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"network", "network", &kiosk.CONFIG.NETWORK},
            {"service", "webserv", &kiosk.CONFIG.WEB_SERVICES}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                play_sound(SND_ERROR, 0);
                toast_message(kiosk_nope(), 1000, 1000);
                refresh_screen(ui_screen);
                return;
            }

            play_sound(SND_CONFIRM, 0);
            load_mux(elements[i].mux_name);

            close_input();
            mux_input_stop();

            break;
        }
    }

    handle_option_next();
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);

    save_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "connect");

    close_input();
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblNetwork, "network");
    lv_obj_set_user_data(ui_lblServices, "service");
    lv_obj_set_user_data(ui_lblBluetooth, "bluetooth");
    lv_obj_set_user_data(ui_lblUSBFunction, "usbfunction");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxconnect_main() {
    init_module("muxconnect");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONNECT.TITLE);
    init_muxconnect(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_options();
    init_dropdown_settings();

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
