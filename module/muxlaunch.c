#include "../lvgl/lvgl.h"
#include "ui/ui_muxlaunch.h"
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

char *mux_module;

int msgbox_active = 0;
int nav_sound;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 8
lv_obj_t *ui_objects_panel[UI_COUNT];
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_icons[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

void show_help(lv_obj_t *element_focused) {
    char *help_messages[UI_COUNT] = {
            lang.MUXLAUNCH.HELP.EXPLORE,
            lang.MUXLAUNCH.HELP.COLLECTION,
            lang.MUXLAUNCH.HELP.HISTORY,
            lang.MUXLAUNCH.HELP.APP,
            lang.MUXLAUNCH.HELP.INFO,
            lang.MUXLAUNCH.HELP.CONFIG,
            lang.MUXLAUNCH.HELP.REBOOT,
            lang.MUXLAUNCH.HELP.SHUTDOWN
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);
    if (current_item_index < num_messages) message = help_messages[current_item_index];

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void init_navigation_group_grid(char *item_labels[], char *glyph_names[]) {
    init_grid_info(UI_COUNT, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, UI_COUNT);
    load_font_section(FONT_PANEL_FOLDER, ui_pnlGrid);
    load_font_section(FONT_PANEL_FOLDER, ui_lblGridCurrentItem);
    for (int i = 0; i < UI_COUNT; i++) {
        uint8_t col = i % theme.GRID.COLUMN_COUNT;
        uint8_t row = i / theme.GRID.COLUMN_COUNT;

        lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
        lv_obj_set_user_data(cell_panel, strdup(item_labels[i]));
        lv_obj_t *cell_image = lv_img_create(cell_panel);
        lv_obj_t *cell_label = lv_label_create(cell_panel);
        lv_obj_set_user_data(cell_label, glyph_names[i]);
        ui_objects[i] = cell_label;

        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));
        char grid_image[MAX_BUFFER_SIZE];
        load_element_image_specifics(STORAGE_THEME, mux_dimension, mux_module, "grid", glyph_names[i],
                                     "default", "png", grid_image, sizeof(grid_image));

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_names[i]);
        char grid_image_focused[MAX_BUFFER_SIZE];
        load_element_image_specifics(STORAGE_THEME, mux_dimension, mux_module, "grid", glyph_name_focused,
                                     "default_focused", "png", grid_image_focused, sizeof(grid_image_focused));

        create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                         grid_image, grid_image_focused, item_labels[i]);

        lv_group_add_obj(ui_group, cell_label);
        lv_group_add_obj(ui_group_glyph, cell_image);
        lv_group_add_obj(ui_group_panel, cell_panel);
    }
}

void init_navigation_group() {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    char *item_labels[] = {lang.MUXLAUNCH.EXPLORE, lang.MUXLAUNCH.COLLECTION, lang.MUXLAUNCH.HISTORY,
                           lang.MUXLAUNCH.APP, lang.MUXLAUNCH.INFO, lang.MUXLAUNCH.CONFIG,
                           lang.MUXLAUNCH.REBOOT, lang.MUXLAUNCH.SHUTDOWN};

    char *item_labels_short[] = {lang.MUXLAUNCH.SHORT.EXPLORE, lang.MUXLAUNCH.SHORT.COLLECTION,
                                 lang.MUXLAUNCH.SHORT.HISTORY, lang.MUXLAUNCH.SHORT.APP,
                                 lang.MUXLAUNCH.SHORT.INFO, lang.MUXLAUNCH.SHORT.CONFIG,
                                 lang.MUXLAUNCH.SHORT.REBOOT, lang.MUXLAUNCH.SHORT.SHUTDOWN};

    char *glyph_names[] = {"explore", "collection", "history",
                           "apps", "info", "config",
                           "reboot", "shutdown"};

    if (theme.GRID.ENABLED) {
        init_navigation_group_grid(item_labels_short, glyph_names);
        lv_label_set_text(ui_lblGridCurrentItem, item_labels_short[0]);
        set_label_long_mode(&theme, ui_objects[0], item_labels_short[0]);
    } else {
        lv_obj_t *ui_objects_panel[] = {
                ui_pnlExplore,
                ui_pnlCollection,
                ui_pnlHistory,
                ui_pnlApps,
                ui_pnlInfo,
                ui_pnlConfig,
                ui_pnlReboot,
                ui_pnlShutdown,
        };

        ui_objects[0] = ui_lblContent;
        ui_objects[1] = ui_lblCollection;
        ui_objects[2] = ui_lblHistory;
        ui_objects[3] = ui_lblApps;
        ui_objects[4] = ui_lblInfo;
        ui_objects[5] = ui_lblConfig;
        ui_objects[6] = ui_lblReboot;
        ui_objects[7] = ui_lblShutdown;

        ui_icons[0] = ui_icoContent;
        ui_icons[1] = ui_icoCollection;
        ui_icons[2] = ui_icoHistory;
        ui_icons[3] = ui_icoApps;
        ui_icons[4] = ui_icoInfo;
        ui_icons[5] = ui_icoConfig;
        ui_icons[6] = ui_icoReboot;
        ui_icons[7] = ui_icoShutdown;

        for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
            apply_theme_list_panel(ui_objects_panel[i]);
            apply_theme_list_item(&theme, ui_objects[i], item_labels[i]);
            apply_theme_list_glyph(&theme, ui_icons[i], mux_module, glyph_names[i]);

            lv_group_add_obj(ui_group, ui_objects[i]);
            lv_group_add_obj(ui_group_glyph, ui_icons[i]);
            lv_obj_set_user_data(ui_objects_panel[i], strdup(item_labels[i]));
            lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);

            apply_size_to_content(&theme, ui_pnlContent, ui_objects[i], ui_icons[i], item_labels[i]);
            apply_text_long_dot(&theme, ui_pnlContent, ui_objects[i], item_labels[i]);
        }

        lv_label_set_text(ui_lblGridCurrentItem, item_labels[0]);
        set_label_long_mode(&theme, ui_objects[0], item_labels[0]);
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = !current_item_index ? UI_COUNT - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    if (theme.GRID.ENABLED) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    }
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    lv_label_set_text(ui_lblGridCurrentItem, lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
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
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == UI_COUNT - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    if (theme.GRID.ENABLED) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    }
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    lv_label_set_text(ui_lblGridCurrentItem, lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void handle_a() {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"explore",    "explore",    &kiosk.LAUNCH.EXPLORE},
            {"collection", "collection", &kiosk.LAUNCH.COLLECTION},
            {"history",    "history",    &kiosk.LAUNCH.HISTORY},
            {"apps",       "app",        &kiosk.LAUNCH.APPLICATION},
            {"info",       "info",       &kiosk.LAUNCH.INFORMATION},
            {"config",     "config",     &kiosk.LAUNCH.CONFIGURATION},
            {"reboot",     "reboot",   NULL},
            {"shutdown",   "shutdown", NULL}
    }; /* Leave the reboot and shutdown as null as they should always be available! */

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (!strcasecmp(u_data, elements[i].glyph_name)) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                toast_message(kiosk_nope(), 1000, 1000);
                return;
            }

            const char *sound = !strcmp(elements[i].mux_name, "reboot") ? "reboot" :
                                !strcmp(elements[i].mux_name, "shutdown") ? "shutdown" :
                                "confirm";

            play_sound(sound, nav_sound, 0, 1);
            load_mux(elements[i].mux_name);
            break;
        }
    }

    safe_quit(0);
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

    load_mux("launcher");
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "");

    safe_quit(0);
    mux_input_stop();
}

void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void handle_up() {
    if (msgbox_active) return;

    // Grid mode.  Wrap on Row.
    if (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 && !get_grid_column_index(current_item_index)) {
        list_nav_next(get_grid_row_index(current_item_index) == grid_info.last_row_index ?
                      grid_info.last_row_item_count - 1 : grid_info.column_count - 1);
        // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    } else if (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 &&
               (!current_item_index || current_item_index == 4)) {
        list_nav_next(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && !current_item_index) {
        list_nav_next(2);
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 3) {
        list_nav_next(4);
        // Regular Navigation
    } else {
        list_nav_prev(1);
    }
}

void handle_down() {
    if (msgbox_active) return;

    // Grid Navigation.  Wrap on Row.
    if (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 &&
        get_grid_column_index(current_item_index) == get_grid_row_item_count(current_item_index) - 1) {
        list_nav_prev(get_grid_row_item_count(current_item_index) - 1);
        // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    } else if (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 &&
               (current_item_index == 3 || current_item_index == 7)) {
        list_nav_prev(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 2) {
        list_nav_prev(2);
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 7) {
        list_nav_prev(4);
        // Regular Navigation
    } else {
        list_nav_next(1);
    }
}

void handle_up_hold(void) {//prev
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if ((theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 && get_grid_column_index(current_item_index) > 0) ||
        (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE < 4 && current_item_index > 0) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index > 0 && current_item_index <= 3) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index > 4) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index > 0 && current_item_index <= 2) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index > 3) ||
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index > 0)) {
        handle_up();
    }
}

void handle_down_hold(void) {//next
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if ((theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 &&
         get_grid_column_index(current_item_index) < get_grid_row_item_count(current_item_index) - 1) ||
        (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE < 4 && current_item_index < UI_COUNT - 1) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index < UI_COUNT - 1 &&
         current_item_index > 3) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index < 3) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index < UI_COUNT - 1 && current_item_index > 2) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index < 2) ||
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index < UI_COUNT - 1)) {
        handle_down();
    }
}

void handle_left() {
    if (msgbox_active) return;

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.GRID.ENABLED && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4)) {
        int column_index = current_item_index % grid_info.column_count;

        if (current_item_index < grid_info.column_count) {
            list_nav_prev(LV_MAX(column_index + 1, grid_info.last_row_item_count));
        } else {
            list_nav_prev(grid_info.column_count);
        }
    } else if (!theme.GRID.ENABLED && (theme.MISC.NAVIGATION_TYPE == 2 || theme.MISC.NAVIGATION_TYPE == 4)) {
        list_nav_prev(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.MISC.NAVIGATION_TYPE == 3 || theme.MISC.NAVIGATION_TYPE == 5) {
        switch (current_item_index) {
            case 3:
            case 4:
                list_nav_prev(3);
                break;
            case 5:
                list_nav_prev(4);
                break;
            case 6:
            case 7:
                list_nav_prev(5);
                break;
        }
    }
}

void handle_right() {
    if (msgbox_active) return;

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.GRID.ENABLED && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4)) {
        uint8_t row_index = current_item_index / grid_info.column_count;

        //when on 2nd to last row do not go past last item
        if (row_index == grid_info.last_row_index - 1) {
            int new_item_index = LV_MIN(current_item_index + grid_info.column_count, UI_COUNT - 1);
            list_nav_next(new_item_index - current_item_index);
            //when on the last row only move based on amount of items in that row
        } else if (row_index == grid_info.last_row_index) {
            list_nav_next(grid_info.last_row_item_count);
        } else {
            list_nav_next(grid_info.column_count);
        }
    } else if (!theme.GRID.ENABLED && (theme.MISC.NAVIGATION_TYPE == 2 || theme.MISC.NAVIGATION_TYPE == 4)) {
        list_nav_next(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.MISC.NAVIGATION_TYPE == 3 || theme.MISC.NAVIGATION_TYPE == 5) {
        switch (current_item_index) {
            case 0:
                list_nav_next(3);
                break;
            case 1:
                list_nav_next(4);
                break;
            case 2:
                list_nav_next(5);
                break;
        }
    }
}

void handle_kiosk_purge() {
    if (current_item_index == 6) { /* reboot */
        if (file_exist(KIOSK_CONFIG)) remove(KIOSK_CONFIG);

        const char *kiosk_paths[] = {
                "application/archive",
                "application/task",
                "config/customisation",
                "config/language",
                "config/network",
                "config/storage",
                "config/webserv",
                "content/core",
                "content/governor",
                "content/option",
                "content/retroarch",
                "content/search",
                "custom/catalogue",
                "custom/configuration",
                "custom/theme",
                "datetime/clock",
                "datetime/timezone",
                "launch/application",
                "launch/config",
                "launch/explore",
                "launch/collection",
                "launch/history",
                "launch/info",
                "setting/advanced",
                "setting/general",
                "setting/hdmi",
                "setting/power",
                "setting/visual"
        };

        char path[MAX_BUFFER_SIZE];
        for (size_t i = 0; i < sizeof(kiosk_paths) / sizeof(kiosk_paths[0]); i++) {
            snprintf(path, sizeof(path), (RUN_KIOSK_PATH "%s"), kiosk_paths[i]);
            write_text_to_file(path, "w", INT, 0);
        }

        handle_b();
    }
}

void handle_kiosk_toggle() {
    if (current_item_index == 6) { /* reboot */
        char kiosk_storage[MAX_BUFFER_SIZE];

        int kiosk_cfg = snprintf(kiosk_storage, sizeof(kiosk_storage), "%s/MUOS/kiosk.ini", device.STORAGE.ROM.MOUNT);
        if (kiosk_cfg < 0 || kiosk_cfg >= (int) sizeof(kiosk_storage)) {
            toast_message(lang.MUXLAUNCH.KIOSK.ERROR, 1000, 1000);
            return;
        }

        if (file_exist(KIOSK_CONFIG)) {
            run_exec((const char *[]) {"mv", KIOSK_CONFIG, kiosk_storage, NULL});
            handle_kiosk_purge();
        } else {
            if (file_exist(kiosk_storage)) {
                run_exec((const char *[]) {"mv", kiosk_storage, KIOSK_CONFIG, NULL});
                run_exec((const char *[]) {(INTERNAL_PATH "script/var/init/kiosk.sh"), "init", NULL});

                toast_message(lang.MUXLAUNCH.KIOSK.PROCESS, 1000, 1000);
                sleep(1); /* not really needed but it's a good buffer... */

                handle_b();
            } else {
                toast_message(lang.MUXLAUNCH.KIOSK.ERROR, 1000, 1000);
            }
        }
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

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB,
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

    lv_obj_set_user_data(ui_lblContent, "explore");
    lv_obj_set_user_data(ui_lblCollection, "collection");
    lv_obj_set_user_data(ui_lblHistory, "history");
    lv_obj_set_user_data(ui_lblApps, "apps");
    lv_obj_set_user_data(ui_lblInfo, "info");
    lv_obj_set_user_data(ui_lblConfig, "config");
    lv_obj_set_user_data(ui_lblReboot, "reboot");
    lv_obj_set_user_data(ui_lblShutdown, "shutdown");

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

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_theme(1, 1);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXLAUNCH.TITLE);
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

    if (file_exist("/tmp/hdmi_out")) {
        remove("/tmp/hdmi_out");
        handle_b();
    }

    mux_input_options input_opts = {
            .swap_axis = (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5) ||
                         (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE >= 1 && theme.MISC.NAVIGATION_TYPE <= 5),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_L1) | BIT(MUX_INPUT_R2) | BIT(MUX_INPUT_X),
                            .press_handler = handle_kiosk_purge,
                            .hold_handler = handle_kiosk_purge,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_L1) | BIT(MUX_INPUT_R2) | BIT(MUX_INPUT_Y),
                            .press_handler = handle_kiosk_toggle,
                            .hold_handler = handle_kiosk_toggle,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            }
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    return 0;
}
