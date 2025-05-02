#include "muxshare.h"
#include "muxnetprofile.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

static void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXNETPROFILE.TITLE, lang.MUXNETPROFILE.HELP);
}

static void sanitise_ssid_name(char *dest, const char *src) {
    size_t j = 0;
    while (*src && j < MAX_BUFFER_SIZE - 1) {
        // replace any slashes with underscores, to prevent some valid SSID names leading to invalid profile file names
        dest[j++] = (*src == '/' || *src == '\\') ? '_' : *src;
        src++;
    }
    dest[j] = '\0';
}

static int remove_profile(char *name) {
    static char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file),
             (RUN_STORAGE_PATH "network/%s.ini"), name);

    if (file_exist(profile_file)) {
        remove(profile_file);
        return 1;
    }

    return 0;
}

static void load_profile(char *name) {
    static char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file),
             (RUN_STORAGE_PATH "network/%s.ini"), name);

    mini_t *net_profile = mini_try_load(profile_file);

    write_text_to_file((RUN_GLOBAL_PATH "network/type"), "w", INT,
                       (!strcasecmp(mini_get_string(net_profile, "network", "type", "dhcp"), "static")) ? 1 : 0);

    write_text_to_file((RUN_GLOBAL_PATH "network/ssid"), "w", CHAR,
                       mini_get_string(net_profile, "network", "ssid", ""));

    write_text_to_file((RUN_GLOBAL_PATH "network/scan"), "w", INT,
                       mini_get_int(net_profile, "network", "scan", 0));

    write_text_to_file((RUN_GLOBAL_PATH "network/pass"), "w", CHAR,
                       mini_get_string(net_profile, "network", "pass", ""));

    write_text_to_file((RUN_GLOBAL_PATH "network/address"), "w", CHAR,
                       mini_get_string(net_profile, "network", "address", ""));

    write_text_to_file((RUN_GLOBAL_PATH "network/subnet"), "w", CHAR,
                       mini_get_string(net_profile, "network", "subnet", ""));

    write_text_to_file((RUN_GLOBAL_PATH "network/gateway"), "w", CHAR,
                       mini_get_string(net_profile, "network", "gateway", ""));

    write_text_to_file((RUN_GLOBAL_PATH "network/dns"), "w", CHAR,
                       mini_get_string(net_profile, "network", "dns", ""));

    mini_free(net_profile);
}

static int save_profile() {
    const char *p_type = read_text_from_file((RUN_GLOBAL_PATH "network/type"));
    const char *p_ssid = read_text_from_file((RUN_GLOBAL_PATH "network/ssid"));
    const char *p_pass = read_text_from_file((RUN_GLOBAL_PATH "network/pass"));
    const char *p_scan = read_text_from_file((RUN_GLOBAL_PATH "network/scan"));
    const char *p_address = read_text_from_file((RUN_GLOBAL_PATH "network/address"));
    const char *p_subnet = read_text_from_file((RUN_GLOBAL_PATH "network/subnet"));
    const char *p_gateway = read_text_from_file((RUN_GLOBAL_PATH "network/gateway"));
    const char *p_dns = read_text_from_file((RUN_GLOBAL_PATH "network/dns"));

    if (!p_ssid || !strlen(p_ssid)) {
        toast_message(lang.MUXNETPROFILE.INVALID_SSID, 1000, 1000);
        return 0;
    }

    int type = safe_atoi(p_type);
    if (type) {
        if (!p_address || !strlen(p_address) ||
            !p_subnet || !strlen(p_subnet) ||
            !p_gateway || !strlen(p_gateway) ||
            !p_dns || !strlen(p_dns)) {
            toast_message(lang.MUXNETPROFILE.INVALID_NETWORK, 1000, 1000);
            return 0;
        }
    } else {
        p_address = "";
        p_subnet = "";
        p_gateway = "";
        p_dns = "";
    }

    static char profile_file[MAX_BUFFER_SIZE];
    int counter = 1;

    char sanitised_ssid[MAX_BUFFER_SIZE];
    sanitise_ssid_name(sanitised_ssid, p_ssid);

    snprintf(profile_file, sizeof(profile_file),
             (RUN_STORAGE_PATH "network/%s.ini"), sanitised_ssid);

    while (file_exist(profile_file)) {
        snprintf(profile_file, sizeof(profile_file),
                 (RUN_STORAGE_PATH "network/%s - %d.ini"), sanitised_ssid, ++counter);
    }

    mini_t *net_profile = mini_try_load(profile_file);

    mini_set_string(net_profile, "network", "ssid", p_ssid);
    mini_set_string(net_profile, "network", "pass", p_pass);
    mini_set_string(net_profile, "network", "scan", p_scan);
    mini_set_string(net_profile, "network", "type", (!type) ? "dhcp" : "static");
    mini_set_string(net_profile, "network", "address", (!type) ? "" : p_address);
    mini_set_string(net_profile, "network", "subnet", (!type) ? "" : p_subnet);
    mini_set_string(net_profile, "network", "gateway", (!type) ? "" : p_gateway);
    mini_set_string(net_profile, "network", "dns", (!type) ? "" : p_dns);

    mini_save(net_profile, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net_profile);

    return 1;
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
    play_sound(SND_NAVIGATE, nav_sound, 0);

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

static void create_profile_items() {
    char profile_path[MAX_BUFFER_SIZE];
    snprintf(profile_path, sizeof(profile_path), (RUN_STORAGE_PATH "network"));

    const char *profile_directories[] = {
            profile_path
    };
    char profile_dir[MAX_BUFFER_SIZE];

    char **file_names = NULL;
    size_t file_count = 0;

    for (size_t dir_index = 0; dir_index < sizeof(profile_directories) / sizeof(profile_directories[0]); ++dir_index) {
        snprintf(profile_dir, sizeof(profile_dir), "%s/", profile_directories[dir_index]);

        DIR *pd = opendir(profile_dir);
        if (!pd) continue;

        struct dirent *pf;
        while ((pf = readdir(pd))) {
            if (pf->d_type == DT_REG) {
                char *last_dot = strrchr(pf->d_name, '.');
                if (last_dot && !strcasecmp(last_dot, ".ini")) {
                    char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                    if (!temp) {
                        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                        free(file_names);
                        closedir(pd);
                        return;
                    }
                    file_names = temp;

                    char full_app_name[MAX_BUFFER_SIZE];
                    snprintf(full_app_name, sizeof(full_app_name), "%s%s", profile_dir, pf->d_name);
                    file_names[file_count] = strdup(full_app_name);
                    if (!file_names[file_count]) {
                        perror(lang.SYSTEM.FAIL_DUP_STRING);
                        free(file_names);
                        closedir(pd);
                        return;
                    }
                    file_count++;
                }
            }
        }
        closedir(pd);
    }

    if (!file_names) return;
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        if (!file_names[i]) continue;
        char *base_filename = file_names[i];

        static char profile_name[MAX_BUFFER_SIZE];
        snprintf(profile_name, sizeof(profile_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        static char profile_store[MAX_BUFFER_SIZE];
        snprintf(profile_store, sizeof(profile_store), "%s", strip_ext(profile_name));

        ui_count++;

        add_item(&items, &item_count, profile_store, profile_store, "", ROM);

        lv_obj_t *ui_pnlProfile = lv_obj_create(ui_pnlContent);
        if (ui_pnlProfile) {
            apply_theme_list_panel(ui_pnlProfile);
            lv_obj_set_user_data(ui_pnlProfile, strdup(profile_store));

            lv_obj_t *ui_lblProfileItem = lv_label_create(ui_pnlProfile);
            if (ui_lblProfileItem) apply_theme_list_item(&theme, ui_lblProfileItem, profile_store);

            lv_obj_t *ui_lblProfileItemGlyph = lv_img_create(ui_pnlProfile);
            apply_theme_list_glyph(&theme, ui_lblProfileItemGlyph, mux_module, "profile");

            lv_group_add_obj(ui_group, ui_lblProfileItem);
            lv_group_add_obj(ui_group_glyph, ui_lblProfileItemGlyph);
            lv_group_add_obj(ui_group_panel, ui_pnlProfile);

            apply_size_to_content(&theme, ui_pnlContent, ui_lblProfileItem, ui_lblProfileItemGlyph, profile_store);
            apply_text_long_dot(&theme, ui_pnlContent, ui_lblProfileItem, profile_store);
        }

        free(base_filename);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free(file_names);

        if (!is_network_connected()) {
            lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_FLOATING);
            lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_FLOATING);
        }

        lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_FLOATING);
        list_nav_move(0, +1);
    }
}

static void handle_confirm(void) {
    if (msgbox_active || is_network_connected() || ui_count <= 0) {
        return;
    }

    play_sound(SND_CONFIRM, nav_sound, 0);
    load_profile(lv_label_get_text(lv_group_get_focused(ui_group)));

    close_input();
    mux_input_stop();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, nav_sound, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, nav_sound, 0);

    close_input();
    mux_input_stop();
}

static void handle_save(void) {
    if (msgbox_active) return;

    if (save_profile()) {
        play_sound(SND_CONFIRM, nav_sound, 0);
        load_mux("net_profile");

        close_input();
        mux_input_stop();
    }
}

static void handle_remove(void) {
    if (msgbox_active || ui_count <= 0) {
        return;
    }

    if (remove_profile(lv_label_get_text(lv_group_get_focused(ui_group)))) {
        play_sound(SND_CONFIRM, nav_sound, 0);
        load_mux("net_profile");

        close_input();
        mux_input_stop();
    }
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, nav_sound, 0);
        show_help();
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

    lv_label_set_text(ui_lblNavA, lang.GENERIC.LOAD);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.GENERIC.SAVE);
    lv_label_set_text(ui_lblNavY, lang.GENERIC.REMOVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    if (!ui_count) {
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_FLOATING);
    }

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

int muxnetprofile_main() {
    init_module("muxnetprofile");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETPROFILE.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();

    create_profile_items();
    if (!ui_count) lv_label_set_text(ui_lblScreenMessage, lang.MUXNETPROFILE.NONE);

    load_kiosk(&kiosk);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_save,
                    [MUX_INPUT_Y] = handle_remove,
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
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
