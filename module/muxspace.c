#include "muxshare.h"
#include "muxspace.h"
#include "ui/ui_muxspace.h"
#include <string.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

#define UI_COUNT 4
static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

struct mount {
    lv_obj_t *value_panel;
    lv_obj_t *bar_panel;
    lv_obj_t *value;
    lv_obj_t *bar;
    const char *partition;
};

static void show_help(lv_obj_t *element_focused) {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), lang.MUXSPACE.HELP);
}

static int is_partition_mounted(const char *partition) {
    if (strcmp(partition, "/") == 0) return 1; // this is rootfs so I mean it should always be mounted

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        perror("fopen /proc/mounts");
        return 0;
    }

    char line[MAX_BUFFER_SIZE];
    int mounted = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, partition)) {
            mounted = 1;
            break;
        }
    }

    fclose(fp);
    return mounted;
}

static void get_storage_info(const char *partition, double *total, double *free, double *used) {
    struct statvfs stat;

    if (!is_partition_mounted(partition)) {
        *total = 0.0;
        *free = 0.0;
        *used = 0.0;
        return;
    }

    if (statvfs(partition, &stat) != 0) {
        perror("statvfs");
        *total = 0.0;
        *free = 0.0;
        *used = 0.0;
        return;
    }

    *total = (double) (stat.f_blocks * stat.f_frsize) / (1024 * 1024 * 1024);
    *free = (double) (stat.f_bavail * stat.f_frsize) / (1024 * 1024 * 1024);
    *used = *total - *free;
}

static void update_storage_info() {
    struct mount storage_info[] = {
            {ui_pnlSD1, ui_pnlSD1Bar, ui_lblSD1Value, ui_barSD1, device.STORAGE.ROM.MOUNT},
            {ui_pnlSD2, ui_pnlSD2Bar, ui_lblSD2Value, ui_barSD2, device.STORAGE.SDCARD.MOUNT},
            {ui_pnlUSB, ui_pnlUSBBar, ui_lblUSBValue, ui_barUSB, device.STORAGE.USB.MOUNT},
            {ui_pnlRFS, ui_pnlRFSBar, ui_lblRFSValue, ui_barRFS, device.STORAGE.ROOT.MOUNT}
    };

    for (size_t i = 0; i < sizeof(storage_info) / sizeof(storage_info[0]); i++) {
        double total_space, free_space, used_space;
        get_storage_info(storage_info[i].partition, &total_space, &free_space, &used_space);

        if (total_space > 0) {
            lv_obj_clear_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);

            int percentage = (total_space > 0) ? (int) ((used_space * 100) / total_space) : 0;
            lv_bar_set_value(storage_info[i].bar, percentage, LV_ANIM_ON);

            char space_info[32];
            snprintf(space_info, sizeof(space_info), "%.2f GB / %.2f GB (%d%%)",
                     used_space, total_space, percentage);
            lv_label_set_text(storage_info[i].value, space_info);

            if (percentage >= 90) {
                lv_obj_set_style_bg_color(storage_info[i].bar, lv_color_hex(0xEE3F3F),
                                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(storage_info[i].bar, 255,
                                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        } else {
            lv_obj_add_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlSD1,
            ui_pnlSD2,
            ui_pnlUSB,
            ui_pnlRFS
    };

    ui_objects[0] = ui_lblSD1;
    ui_objects[1] = ui_lblSD2;
    ui_objects[2] = ui_lblUSB;
    ui_objects[3] = ui_lblRFS;

    lv_obj_t *ui_objects_value[] = {
            ui_lblSD1Value,
            ui_lblSD2Value,
            ui_lblUSBValue,
            ui_lblRFSValue
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoSD1,
            ui_icoSD2,
            ui_icoUSB,
            ui_icoRFS
    };

    apply_theme_list_panel(ui_pnlSD1);
    apply_theme_list_panel(ui_pnlSD2);
    apply_theme_list_panel(ui_pnlUSB);
    apply_theme_list_panel(ui_pnlRFS);

    apply_theme_list_item(&theme, ui_lblSD1, "SD1");
    apply_theme_list_item(&theme, ui_lblSD2, "SD2");
    apply_theme_list_item(&theme, ui_lblUSB, "USB");
    apply_theme_list_item(&theme, ui_lblRFS, "ROOTFS");

    apply_theme_list_glyph(&theme, ui_icoSD1, mux_module, "sd1");
    apply_theme_list_glyph(&theme, ui_icoSD2, mux_module, "sd2");
    apply_theme_list_glyph(&theme, ui_icoUSB, mux_module, "usb");
    apply_theme_list_glyph(&theme, ui_icoRFS, mux_module, "rfs");

    apply_theme_list_value(&theme, ui_lblSD1Value, "");
    apply_theme_list_value(&theme, ui_lblSD2Value, "");
    apply_theme_list_value(&theme, ui_lblUSBValue, "");
    apply_theme_list_value(&theme, ui_lblRFSValue, "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (!current_item_index) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "space");

    close_input();
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblSD1, "sd1");
    lv_obj_set_user_data(ui_lblSD2, "sd2");
    lv_obj_set_user_data(ui_lblUSB, "usb");
    lv_obj_set_user_data(ui_lblRFS, "rfs");

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

int muxspace_main() {

    init_module("muxspace");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSPACE.TITLE);
    init_muxspace(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);

    update_storage_info();

    load_kiosk(&kiosk);

    init_timer(ui_refresh_task, update_storage_info);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
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
