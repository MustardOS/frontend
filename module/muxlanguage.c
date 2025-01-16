#include "../lvgl/lvgl.h"
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/collection.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_mux_panels[5];

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXLANGUAGE.TITLE, lang.MUXLANGUAGE.HELP);
}

void populate_languages() {
    struct dirent *entry;

    char lang_dir[MAX_BUFFER_SIZE];
    snprintf(lang_dir, sizeof(lang_dir), (RUN_STORAGE_PATH "language"));
    DIR *dir = opendir(lang_dir);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            size_t len = strlen(entry->d_name);
            if (len > 5 && !strcasecmp(entry->d_name + len - 5, ".json")) {
                entry->d_name[len - 5] = '\0';
                add_item(&items, &item_count, entry->d_name, TS(entry->d_name), "", ROM);
            }
        }
    }
    sort_items(items, item_count);
    closedir(dir);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);
        current_item_index = !current_item_index ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    nav_moved = 1;
}

void create_language_items() {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    populate_languages();
    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlLanguage = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlLanguage);

        lv_obj_t *ui_lblLanguageItem = lv_label_create(ui_pnlLanguage);
        apply_theme_list_item(&theme, ui_lblLanguageItem, items[i].display_name);

        lv_obj_t *ui_lblLanguageGlyph = lv_img_create(ui_pnlLanguage);
        apply_theme_list_glyph(&theme, ui_lblLanguageGlyph, mux_module, "language");

        lv_group_add_obj(ui_group, ui_lblLanguageItem);
        lv_group_add_obj(ui_group_glyph, ui_lblLanguageGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlLanguage);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblLanguageItem, ui_lblLanguageGlyph, items[i].name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblLanguageItem, items[i].display_name);
    }
    if (ui_count > 0) {
        list_nav_next(0);
        lv_obj_update_layout(ui_pnlContent);
    } else if (!ui_count) {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXLANGUAGE.NONE);
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }
}

void handle_confirm() {
    if (msgbox_active) return;

    play_sound("confirm", nav_sound, 0, 1);
    toast_message(lang.MUXLANGUAGE.SAVE, 0, 0);

    write_text_to_file((RUN_GLOBAL_PATH "settings/general/language"), "w", CHAR,
                       items[current_item_index].name);

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
    mux_input_stop();
}

void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help();
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
    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXLANGUAGE.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_language_items();
    init_navigation_sound(&nav_sound, mux_module);

    init_input(&js_fd, &js_fd_sys);
    init_timer(ui_refresh_task, NULL);

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "language");

    free_items(items, item_count);
    close(js_fd);
    close(js_fd_sys);

    return 0;
}
