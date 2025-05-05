#include "muxshare.h"
#include "muxconfig.h"
#include "ui/ui_muxconfig.h"
#include <string.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

#define UI_COUNT 7
static lv_obj_t *ui_objects[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblTweakGeneral, lang.MUXCONFIG.HELP.GENERAL},
            {ui_lblConnect,      lang.MUXCONFIG.HELP.CONNECTIVITY},
            {ui_lblCustom,       lang.MUXCONFIG.HELP.CUSTOM},
            {ui_lblInterface,    lang.MUXCONFIG.HELP.VISUAL},
            {ui_lblLanguage,     lang.MUXCONFIG.HELP.LANGUAGE},
            {ui_lblPower,        lang.MUXCONFIG.HELP.POWER},
            {ui_lblStorage,      lang.MUXCONFIG.HELP.STORAGE},
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

static void add_config_item(lv_obj_t *ui_pnl, lv_obj_t *ui_lbl, lv_obj_t *ui_ico, char *item_text, char *glyph_name) {
    apply_theme_list_panel(ui_pnl);
    apply_theme_list_item(&theme, ui_lbl, item_text);
    apply_theme_list_glyph(&theme, ui_ico, mux_module, glyph_name);

    lv_obj_set_user_data(ui_pnl, strdup(item_text));
    lv_group_add_obj(ui_group, ui_lbl);
    lv_group_add_obj(ui_group_glyph, ui_ico);
    lv_group_add_obj(ui_group_panel, ui_pnl);

    apply_size_to_content(&theme, ui_pnlContent, ui_lbl, ui_ico, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lbl, item_text);
}

static void init_navigation_group() {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_objects[0] = ui_lblTweakGeneral;
    ui_objects[1] = ui_lblConnect;
    ui_objects[2] = ui_lblCustom;
    ui_objects[3] = ui_lblInterface;
    ui_objects[4] = ui_lblLanguage;
    ui_objects[5] = ui_lblPower;
    ui_objects[6] = ui_lblStorage;
    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);

    add_config_item(ui_pnlTweakGeneral, ui_lblTweakGeneral, ui_icoTweakGeneral, lang.MUXCONFIG.GENERAL, "general");
    add_config_item(ui_pnlConnect, ui_lblConnect, ui_icoConnect, lang.MUXCONFIG.CONNECTIVITY, "connect");
    add_config_item(ui_pnlCustom, ui_lblCustom, ui_icoCustom, lang.MUXCONFIG.CUSTOM, "custom");
    add_config_item(ui_pnlInterface, ui_lblInterface, ui_icoInterface, lang.MUXCONFIG.VISUAL, "interface");
    add_config_item(ui_pnlLanguage, ui_lblLanguage, ui_icoLanguage, lang.MUXCONFIG.LANGUAGE, "language");
    add_config_item(ui_pnlPower, ui_lblPower, ui_icoPower, lang.MUXCONFIG.POWER, "power");
    add_config_item(ui_pnlStorage, ui_lblStorage, ui_icoStorage, lang.MUXCONFIG.STORAGE, "storage");

    char SD2[MAX_BUFFER_SIZE];
    snprintf(SD2, sizeof(SD2), "%s/ROMS/", device.STORAGE.SDCARD.MOUNT);
    if (!directory_exist(SD2)) {
        lv_obj_add_flag(ui_pnlStorage, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblStorage, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoStorage, LV_OBJ_FLAG_HIDDEN);
        ui_count -= 1;
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));

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

static void handle_a() {
    if (msgbox_active) return;

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
            {"storage",   "storage",  &kiosk.CONFIG.STORAGE}
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
            break;
        }
    }

    close_input();
    mux_input_stop();
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "config");

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

    lv_obj_set_user_data(ui_lblTweakGeneral, "general");
    lv_obj_set_user_data(ui_lblConnect, "connect");
    lv_obj_set_user_data(ui_lblCustom, "custom");
    lv_obj_set_user_data(ui_lblInterface, "interface");
    lv_obj_set_user_data(ui_lblLanguage, "language");
    lv_obj_set_user_data(ui_lblPower, "power");
    lv_obj_set_user_data(ui_lblStorage, "storage");

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

int muxconfig_main() {
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

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

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
            .hold_handler = {
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
