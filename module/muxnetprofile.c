#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
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
char *osd_message;

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
                     lang.MUXNETPROFILE.TITLE, lang.MUXNETPROFILE.HELP);
}

int remove_profile(char *name) {
    static char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file),
             (RUN_STORAGE_PATH "network/%s.ini"), name);

    if (file_exist(profile_file)) {
        remove(profile_file);
        return 1;
    }

    return 0;
}

void load_profile(char *name) {
    static char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file),
             (RUN_STORAGE_PATH "network/%s.ini"), name);

    mini_t *net_profile = mini_try_load(profile_file);

    write_text_to_file((RUN_GLOBAL_PATH "network/type"), "w", INT,
                       (strcasecmp(mini_get_string(net_profile, "network", "type", "dhcp"), "static") == 0) ? 1 : 0);

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

int save_profile() {
    const char *p_type = read_text_from_file((RUN_GLOBAL_PATH "network/type"));
    const char *p_ssid = read_text_from_file((RUN_GLOBAL_PATH "network/ssid"));
    const char *p_pass = read_text_from_file((RUN_GLOBAL_PATH "network/pass"));
    const char *p_scan = read_text_from_file((RUN_GLOBAL_PATH "network/scan"));
    const char *p_address = read_text_from_file((RUN_GLOBAL_PATH "network/address"));
    const char *p_subnet = read_text_from_file((RUN_GLOBAL_PATH "network/subnet"));
    const char *p_gateway = read_text_from_file((RUN_GLOBAL_PATH "network/gateway"));
    const char *p_dns = read_text_from_file((RUN_GLOBAL_PATH "network/dns"));

    if (!p_ssid || strlen(p_ssid) == 0) {
        toast_message(lang.MUXNETPROFILE.INVALID_SSID, 1000, 1000);
        return 0;
    }

    int type = safe_atoi(p_type);
    if (type) {
        if (!p_address || strlen(p_address) == 0 ||
            !p_subnet || strlen(p_subnet) == 0 ||
            !p_gateway || strlen(p_gateway) == 0 ||
            !p_dns || strlen(p_dns) == 0) {
            toast_message(lang.MUXNETPROFILE.INVALID_NETWORK, 1000, 1000);
            refresh_screen(device.SCREEN.WAIT);
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

    snprintf(profile_file, sizeof(profile_file),
             (RUN_STORAGE_PATH "network/%s.ini"), p_ssid);

    while (file_exist(profile_file)) {
        snprintf(profile_file, sizeof(profile_file),
                 (RUN_STORAGE_PATH "network/%s - %d.ini"), p_ssid, ++counter);
    }

    mini_t *net_profile = mini_try_load(profile_file);

    mini_set_string(net_profile, "network", "ssid", p_ssid);
    mini_set_string(net_profile, "network", "pass", p_pass);
    mini_set_string(net_profile, "network", "scan", p_scan);
    mini_set_string(net_profile, "network", "type", (type == 0) ? "dhcp" : "static");
    mini_set_string(net_profile, "network", "address", (type == 0) ? "" : p_address);
    mini_set_string(net_profile, "network", "subnet", (type == 0) ? "" : p_subnet);
    mini_set_string(net_profile, "network", "gateway", (type == 0) ? "" : p_gateway);
    mini_set_string(net_profile, "network", "dns", (type == 0) ? "" : p_dns);

    mini_save(net_profile, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net_profile);

    return 1;
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
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
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void create_profile_items() {
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
        if (pd == NULL) continue;

        struct dirent *pf;
        while ((pf = readdir(pd))) {
            if (pf->d_type == DT_REG) {
                char *last_dot = strrchr(pf->d_name, '.');
                if (last_dot != NULL && strcasecmp(last_dot, ".ini") == 0) {
                    char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                    if (temp == NULL) {
                        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                        free(file_names);
                        closedir(pd);
                        return;
                    }
                    file_names = temp;

                    char full_app_name[MAX_BUFFER_SIZE];
                    snprintf(full_app_name, sizeof(full_app_name), "%s%s", profile_dir, pf->d_name);
                    file_names[file_count] = strdup(full_app_name);
                    if (file_names[file_count] == NULL) {
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

    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
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
            apply_theme_list_panel(&theme, &device, ui_pnlProfile);
            lv_obj_set_user_data(ui_pnlProfile, strdup(profile_store));

            lv_obj_t *ui_lblProfileItem = lv_label_create(ui_pnlProfile);
            if (ui_lblProfileItem) apply_theme_list_item(&theme, ui_lblProfileItem, profile_store, true, false);

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

        lv_label_set_text(ui_lblScreenMessage, "");

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
        list_nav_next(0);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXNETPROFILE.NONE);
    }
}

void handle_confirm(void) {
    if (msgbox_active || is_network_connected() || ui_count <= 0) {
        return;
    }

    play_sound("confirm", nav_sound, 0, 1);
    load_profile(lv_label_get_text(lv_group_get_focused(ui_group)));
    mux_input_stop();
}

void handle_back(void) {
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

void handle_save(void) {
    if (msgbox_active) return;

    if (save_profile()) {
        play_sound("confirm", nav_sound, 0, 1);
        load_mux("net_profile");
        mux_input_stop();
    }
}

void handle_remove(void) {
    if (msgbox_active || ui_count <= 0) {
        return;
    }

    if (remove_profile(lv_label_get_text(lv_group_get_focused(ui_group)))) {
        play_sound("confirm", nav_sound, 0, 1);
        load_mux("net_profile");
        mux_input_stop();
    }
}

void handle_help(void) {
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

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

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
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
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

    if (TEST_IMAGE) display_testing_message(ui_screen);

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
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

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t *buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_lang(&lang);

    load_theme(&theme, &config, &device, basename(argv[0]));

    ui_common_screen_init(&theme, &device, &lang, lang.MUXNETPROFILE.TITLE);
    init_elements();

    lv_label_set_text(ui_lblScreenMessage, lang.MUXNETPROFILE.LOAD);
    lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    create_profile_items();

    refresh_screen(device.SCREEN.WAIT);
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
                    [MUX_INPUT_X] = handle_save,
                    [MUX_INPUT_Y] = handle_remove,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .combo = {
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
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
