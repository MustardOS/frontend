#include "../lvgl/lvgl.h"
#include <regex.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <linux/limits.h>
#include "../common/init.h"
#include "../common/img/nothing.h"
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

#define EXPLORE_DIR "/tmp/explore_dir"
#define EXPLORE_NAME "/tmp/explore_name"

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
int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_mux_panels[5];

lv_obj_t *ui_imgScreenshot;
int is_fullscreen = 0;

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(lv_group_get_focused(ui_group))), lang.MUXSHOT.HELP);
}

void image_refresh() {
    // Invalidate the cache for this image path
    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgScreenshot));

    char screenshot_file[PATH_MAX];
    snprintf(screenshot_file, sizeof(screenshot_file), "%s/%s.png",
             STORAGE_SHOTS, lv_label_get_text(lv_group_get_focused(ui_group)));

    if (file_exist(screenshot_file)) {
        char screenshot_image[PATH_MAX];
        snprintf(screenshot_image, sizeof(screenshot_image), "M:%s", screenshot_file);
        lv_img_set_src(ui_imgScreenshot, screenshot_image);
    } else {
        lv_img_set_src(ui_imgScreenshot, &ui_image_Nothing);
    }
}

void create_screenshot_items() {
    DIR *td;
    struct dirent *tf;
    regex_t regex;
    const char *pattern = "^muOS_[0-9]{8}_[0-9]{4}_[0-9]+\\.png$";

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) return;

    td = opendir(STORAGE_SHOTS);
    if (!td) {
        regfree(&regex);
        return;
    }

    while ((tf = readdir(td))) {
        if (regexec(&regex, tf->d_name, 0, NULL, 0) == 0) {
            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot) *last_dot = '\0';

            add_item(&items, &item_count, tf->d_name, tf->d_name, "", ROM);
        }
    }

    closedir(td);
    regfree(&regex);

    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlScreenshot = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlScreenshot);

        lv_obj_t *ui_lblScreenshotItem = lv_label_create(ui_pnlScreenshot);
        apply_theme_list_item(&theme, ui_lblScreenshotItem, items[i].display_name);

        lv_obj_t *ui_lblScreenshotItemGlyph = lv_img_create(ui_pnlScreenshot);
        apply_theme_list_glyph(&theme, ui_lblScreenshotItemGlyph, mux_module, "screenshot");

        lv_group_add_obj(ui_group, ui_lblScreenshotItem);
        lv_group_add_obj(ui_group_glyph, ui_lblScreenshotItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlScreenshot);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblScreenshotItem, ui_lblScreenshotItemGlyph,
                              items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblScreenshotItem, items[i].display_name);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}

void list_nav_prev(int steps) {
    if (ui_count <= 0) return;

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
    image_refresh();
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (ui_count <= 0) return;

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
    image_refresh();
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    nav_moved = 1;
}

void handle_confirm() {
    if (msgbox_active || ui_count <= 0) return;

    play_sound("confirm", nav_sound, 0, 1);

    if (is_fullscreen) {
        is_fullscreen = 0;
        lv_obj_set_style_img_opa(ui_imgScreenshot, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);
    } else {
        is_fullscreen = 1;
        lv_obj_set_style_img_opa(ui_imgScreenshot, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);
    }
}

void handle_back() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    mux_input_stop();
}

void handle_remove() {
    if (msgbox_active || is_fullscreen || !ui_count) return;

    char screenshot_file[PATH_MAX];
    snprintf(screenshot_file, sizeof(screenshot_file), "%s/%s.png",
             STORAGE_SHOTS, lv_label_get_text(lv_group_get_focused(ui_group)));

    if (file_exist(screenshot_file)) {
        remove(screenshot_file);
        load_mux("screenshot");
        mux_input_stop();
    }
}

void handle_help() {
    if (msgbox_active || is_fullscreen) return;

    if (progress_onscreen == -1 && ui_count > 0) {
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
    lv_label_set_text(ui_lblNavX, lang.GENERIC.REMOVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavCGlyph,
            ui_lblNavC,
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

    ui_imgScreenshot = lv_img_create(ui_screen);
    lv_img_set_pivot(ui_imgScreenshot, 0, 0);
    lv_img_set_src(ui_imgScreenshot, &ui_image_Nothing);
    lv_obj_set_style_img_opa(ui_imgScreenshot, 25, LV_PART_MAIN | LV_STATE_DEFAULT);

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (ui_count > 0 && nav_moved) {
        image_refresh();

        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_invalidate(ui_pnlBox);
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

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSHOT.TITLE);

    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_screenshot_items();
    init_navigation_sound(&nav_sound, mux_module);

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);
    init_timer(ui_refresh_task, NULL);

    if (!ui_count) lv_label_set_text(ui_lblScreenMessage, lang.MUXSHOT.NONE);

    load_kiosk(&kiosk);

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
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_remove,
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
    safe_quit(0);

    free_items(items, item_count);

    close(joy_general);
    close(joy_power);
    close(joy_volume);
    close(joy_extra);

    return 0;
}
