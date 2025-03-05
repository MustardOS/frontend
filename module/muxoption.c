#include "../lvgl/lvgl.h"
#include "ui/ui_muxoption.h"
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
#include "../common/input/list_nav.h"
#include "../common/log.h"

char *mux_module;

int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

char *rom_name;
char *rom_dir;
char *rom_system;

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

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 3
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_icons[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblSearch,   lang.MUXOPTION.HELP.SEARCH},
            {ui_lblCore,     lang.MUXOPTION.HELP.ASSIGN_CORE},
            {ui_lblGovernor, lang.MUXOPTION.HELP.ASSIGN_GOV},
    };

    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s", lang.GENERIC.NO_HELP);

    if (element_focused == help_messages[0].element) {
        snprintf(message, sizeof(message), "%s", help_messages[0].message);
    } else if (element_focused == help_messages[1].element) {
        snprintf(message, sizeof(message), "%s\n\n%s:\n%s:  %s\n%s:  %s",
                 lang.MUXOPTION.HELP.ASSIGN_CORE,
                 lang.MUXOPTION.CURRENT,
                 lang.MUXOPTION.DIRECTORY, get_directory_core(rom_dir, 1),
                 lang.MUXOPTION.INDIVIDUAL, get_file_core(rom_dir, rom_name));
    } else if (element_focused == help_messages[2].element) {
        snprintf(message, sizeof(message), "%s\n\n%s:\n%s:  %s\n%s:  %s",
                 lang.MUXOPTION.HELP.ASSIGN_GOV,
                 lang.MUXOPTION.CURRENT,
                 lang.MUXOPTION.DIRECTORY, get_directory_governor(rom_dir),
                 lang.MUXOPTION.INDIVIDUAL, get_file_governor(rom_dir, rom_name));
    }

    if (strlen(message) <= 1) snprintf(message, sizeof(message), "%s", lang.GENERIC.NO_HELP);

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void add_info_item(int index, char *item_text, char *glyph_name, bool add_bottom_border) {
    lv_obj_t *ui_pnlInfoItem = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(ui_pnlInfoItem);
    lv_obj_t *ui_lblInfoItem = lv_label_create(ui_pnlInfoItem);
    apply_theme_list_item(&theme, ui_lblInfoItem, item_text);
    lv_obj_t *ui_icoInfoItem = lv_img_create(ui_pnlInfoItem);
    apply_theme_list_glyph(&theme, ui_icoInfoItem, mux_module, glyph_name);

    if (add_bottom_border) {
        lv_obj_set_height(ui_pnlInfoItem, 1);
        lv_obj_set_style_border_width(ui_pnlInfoItem, 1,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_pnlInfoItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnlInfoItem, theme.LIST_DEFAULT.TEXT_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_pnlInfoItem, LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_move_to_index(ui_pnlInfoItem, index);
}

void add_info_items() {
    char game_directory[FILENAME_MAX];
    snprintf(game_directory, sizeof(game_directory), "%s:  %s", lang.MUXOPTION.DIRECTORY,
             get_last_subdir(rom_dir, '/', 4));
    add_info_item(0, game_directory, "folder", false);
    char game_name[FILENAME_MAX];
    snprintf(game_name, sizeof(game_name), "%s:  %s", lang.MUXOPTION.NAME, rom_name);
    add_info_item(1, game_name, "rom", false);
    add_info_item(2, "", "", true);
}

void init_navigation_group() {
    add_info_items();
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlSearch,
            ui_pnlCore,
            ui_pnlGovernor
    };

    ui_objects[0] = ui_lblSearch;
    ui_objects[1] = ui_lblCore;
    ui_objects[2] = ui_lblGovernor;

    ui_icons[0] = ui_icoSearch;
    ui_icons[1] = ui_icoCore;
    ui_icons[2] = ui_icoGovernor;

    apply_theme_list_panel(ui_pnlSearch);
    apply_theme_list_panel(ui_pnlCore);
    apply_theme_list_panel(ui_pnlGovernor);

    apply_theme_list_item(&theme, ui_lblSearch, lang.MUXOPTION.SEARCH);
    apply_theme_list_item(&theme, ui_lblCore, lang.MUXOPTION.ASSIGN_CORE);
    apply_theme_list_item(&theme, ui_lblGovernor, lang.MUXOPTION.ASSIGN_GOV);

    apply_theme_list_glyph(&theme, ui_icoSearch, mux_module, "search");
    apply_theme_list_glyph(&theme, ui_icoCore, mux_module, "core");
    apply_theme_list_glyph(&theme, ui_icoGovernor, mux_module, "governor");

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_obj_set_user_data(ui_objects_panel[i], strdup(lv_label_get_text(ui_objects[i])));
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_icons[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);

        apply_size_to_content(&theme, ui_pnlContent, ui_objects[i], ui_icons[i], lv_label_get_text(ui_objects[i]));
        apply_text_long_dot(&theme, ui_pnlContent, ui_objects[i], lv_label_get_text(ui_objects[i]));
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == 0) ? UI_COUNT - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == UI_COUNT - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void handle_confirm() {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"search",   "search",   &kiosk.CONTENT.SEARCH},
            {"core",     "assign",   &kiosk.CONTENT.ASSIGN_CORE},
            {"governor", "governor", &kiosk.CONTENT.ASSIGN_GOVERNOR}
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

    safe_quit(0);
    mux_input_stop();
}

void handle_back() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    safe_quit(0);
    mux_input_stop();
}

void handle_help() {
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

    lv_obj_set_user_data(ui_lblSearch, "search");
    lv_obj_set_user_data(ui_lblCore, "core");
    lv_obj_set_user_data(ui_lblGovernor, "governor");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
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

    char *cmd_help = "\nmuOS Options\nUsage: %s <-cds>\n\nOptions:\n"
                     "\t-c Name of content file\n"
                     "\t-d Name of content directory\n"
                     "\t-s Name of content system (use 'none' for root)\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "c:d:s:")) != -1) {
        switch (opt) {
            case 'c':
                rom_name = optarg;
                break;
            case 'd':
                rom_dir = optarg;
                break;
            case 's':
                rom_system = optarg;
                break;
            default:
                fprintf(stderr, cmd_help, argv[0]);
                return 1;
        }
    }

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    if (file_exist(OPTION_SKIP)) {
        remove(OPTION_SKIP);
        LOG_INFO(mux_module, "Skipping Options Module - Not Required...")
        safe_quit(0);
        return 0;
    }

    init_theme(1, 0);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXOPTION.TITLE);
    init_mux(ui_pnlContent);
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);

    load_kiosk(&kiosk);
    list_nav_next(direct_to_previous(ui_objects, UI_COUNT, &nav_moved));

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
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
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
