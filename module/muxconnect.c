#include "../lvgl/lvgl.h"
#include "ui/ui_muxconnect.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

int turbo_mode = 0;
int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

int bluetooth_original, usbfunction_original;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 4
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblNetwork,      lang.MUXCONNECT.HELP.WIFI},
            {ui_lblUSBFunction,  lang.MUXCONNECT.HELP.USB},
            {ui_lblBluetooth,    lang.MUXCONNECT.HELP.BLUETOOTH},
            {ui_lblServices,     lang.MUXCONNECT.HELP.WEB},
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

void init_dropdown_settings() {
    usbfunction_original = lv_dropdown_get_selected(ui_droUSBFunction);
    bluetooth_original = lv_dropdown_get_selected(ui_droBluetooth);
}

void restore_options() {
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

void save_options() {
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

    if (is_modified > 0) run_exec((const char *[]) {(char *) INTERNAL_PATH "script/mux/tweak.sh", NULL});
}

void add_item(lv_obj_t *ui_pnl, lv_obj_t *ui_lbl, lv_obj_t *ui_ico, lv_obj_t *ui_dro, char *item_text, char *glyph_name) {
    apply_theme_list_panel(ui_pnl);
    apply_theme_list_drop_down(&theme, ui_dro, "");
    apply_theme_list_item(&theme, ui_lbl, item_text);
    apply_theme_list_glyph(&theme, ui_ico, mux_module, glyph_name);

    lv_obj_set_user_data(ui_pnl, strdup(item_text));
    lv_group_add_obj(ui_group, ui_lbl);
    lv_group_add_obj(ui_group_value, ui_dro);
    lv_group_add_obj(ui_group_glyph, ui_ico);
    lv_group_add_obj(ui_group_panel, ui_pnl);

    apply_size_to_content(&theme, ui_pnlContent, ui_lbl, ui_ico, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lbl, item_text);
}

void init_navigation_group() {
    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();
    
    ui_objects[0] = ui_lblBluetooth;
    ui_objects[1] = ui_lblServices;
    ui_objects[2] = ui_lblUSBFunction;
    ui_objects[3] = ui_lblNetwork;
    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_item(ui_pnlBluetooth, ui_lblBluetooth, ui_icoBluetooth, ui_droBluetooth, lang.MUXCONNECT.BLUETOOTH, "bluetooth");
    add_drop_down_options(ui_droBluetooth, disabled_enabled, 2);
    add_item(ui_pnlServices, ui_lblServices, ui_icoServices, ui_droServices, lang.MUXCONNECT.WEB, "service");
    add_item(ui_pnlUSBFunction, ui_lblUSBFunction, ui_icoUSBFunction, ui_droUSBFunction, lang.MUXCONNECT.USB, "usbfunction");
    add_drop_down_options(ui_droUSBFunction, (char *[]) {lang.GENERIC.DISABLED, "ADB", "MTP"}, 3);
    add_item(ui_pnlNetwork, ui_lblNetwork, ui_icoNetwork, ui_droNetwork, lang.MUXCONNECT.WIFI, "network");

    if (!device.DEVICE.HAS_BLUETOOTH || true) { //TODO: remove true when bluetooth is implemented
        lv_obj_add_flag(ui_pnlBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_droBluetooth, LV_OBJ_FLAG_HIDDEN);
        ui_count -= 1;
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

void handle_a() {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"network",  "network",  &kiosk.CONFIG.NETWORK},
            {"service",  "webserv",  &kiosk.CONFIG.WEB_SERVICES}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                toast_message(kiosk_nope(), 1000, 1000);
                return;
            }

            play_sound("confirm", nav_sound, 0, 1);
            load_mux(elements[i].mux_name);
            break;
        }
    }

    mux_input_stop();
}


void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    save_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "config");
    mux_input_stop();
}

void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void init_elements() {
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
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblBluetooth, "bluetooth");
    lv_obj_set_user_data(ui_lblServices, "service");
    lv_obj_set_user_data(ui_lblUSBFunction, "usbfunction");
    lv_obj_set_user_data(ui_lblNetwork, "network");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONNECT.TITLE);
    init_mux(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);

    restore_options();
    init_dropdown_settings();

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);
    init_timer(ui_refresh_task, NULL);

    load_kiosk(&kiosk);
    int prev_index = direct_to_previous(ui_objects, UI_COUNT, &nav_moved);
    //TODO: remove true when bluetooth is implemented
    if (prev_index > lv_obj_get_index(ui_pnlBluetooth) && (!device.DEVICE.HAS_BLUETOOTH || true)) prev_index--;
    list_nav_next(prev_index);

    mux_input_options input_opts = {
            .general_fd = joy_general,
            .power_fd = joy_power,
            .volume_fd = joy_volume,
            .extra_fd = joy_extra,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
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
            },
            .combo = {
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP)),
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_UP)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_DOWN)),
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);
    safe_quit();

    close(joy_general);
    close(joy_power);

    return 0;
}
