#include "../lvgl/lvgl.h"
#include "ui/ui_muxwebserv.h"
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

int sshd_original, sftpgo_original, ttyd_original, syncthing_original,
        rslsync_original, ntp_original, tailscaled_original;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 7
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblSSHD,       lang.MUXWEBSERV.HELP.SHELL},
            {ui_lblSFTPGo,     lang.MUXWEBSERV.HELP.SFTP},
            {ui_lblTTYD,       lang.MUXWEBSERV.HELP.TERMINAL},
            {ui_lblSyncthing,  lang.MUXWEBSERV.HELP.SYNCTHING},
            {ui_lblRSLSync,    lang.MUXWEBSERV.HELP.RESILIO},
            {ui_lblNTP,        lang.MUXWEBSERV.HELP.NTP},
            {ui_lblTailscaled, lang.MUXWEBSERV.HELP.TAILSCALE},
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

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

void init_element_events() {
    lv_obj_t *dropdowns[] = {
            ui_droSSHD,
            ui_droSFTPGo,
            ui_droTTYD,
            ui_droSyncthing,
            ui_droRSLSync,
            ui_droNTP,
            ui_droTailscaled
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

void init_dropdown_settings() {
    sshd_original = lv_dropdown_get_selected(ui_droSSHD);
    sftpgo_original = lv_dropdown_get_selected(ui_droSFTPGo);
    ttyd_original = lv_dropdown_get_selected(ui_droTTYD);
    syncthing_original = lv_dropdown_get_selected(ui_droSyncthing);
    rslsync_original = lv_dropdown_get_selected(ui_droRSLSync);
    ntp_original = lv_dropdown_get_selected(ui_droNTP);
    tailscaled_original = lv_dropdown_get_selected(ui_droTailscaled);
}

void restore_web_options() {
    lv_dropdown_set_selected(ui_droSSHD, config.WEB.SSHD);
    lv_dropdown_set_selected(ui_droSFTPGo, config.WEB.SFTPGO);
    lv_dropdown_set_selected(ui_droTTYD, config.WEB.TTYD);
    lv_dropdown_set_selected(ui_droSyncthing, config.WEB.SYNCTHING);
    lv_dropdown_set_selected(ui_droRSLSync, config.WEB.RSLSYNC);
    lv_dropdown_set_selected(ui_droNTP, config.WEB.NTP);
    lv_dropdown_set_selected(ui_droTailscaled, config.WEB.TAILSCALED);
}

void save_web_options() {
    int idx_sshd = lv_dropdown_get_selected(ui_droSSHD);
    int idx_sftpgo = lv_dropdown_get_selected(ui_droSFTPGo);
    int idx_ttyd = lv_dropdown_get_selected(ui_droTTYD);
    int idx_syncthing = lv_dropdown_get_selected(ui_droSyncthing);
    int idx_rslsync = lv_dropdown_get_selected(ui_droRSLSync);
    int idx_ntp = lv_dropdown_get_selected(ui_droNTP);
    int idx_tailscaled = lv_dropdown_get_selected(ui_droTailscaled);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droSSHD) != sshd_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/sshd"), "w", INT, idx_sshd);
    }

    if (lv_dropdown_get_selected(ui_droSFTPGo) != sftpgo_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/sftpgo"), "w", INT, idx_sftpgo);
    }

    if (lv_dropdown_get_selected(ui_droTTYD) != ttyd_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/ttyd"), "w", INT, idx_ttyd);
    }

    if (lv_dropdown_get_selected(ui_droSyncthing) != syncthing_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/syncthing"), "w", INT, idx_syncthing);
    }

    if (lv_dropdown_get_selected(ui_droRSLSync) != rslsync_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/rslsync"), "w", INT, idx_rslsync);
    }

    if (lv_dropdown_get_selected(ui_droNTP) != ntp_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/ntp"), "w", INT, idx_ntp);
    }

    if (lv_dropdown_get_selected(ui_droTailscaled) != tailscaled_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "web/tailscaled"), "w", INT, idx_tailscaled);
    }

    if (is_modified > 0) run_exec((const char *[]) {(char *) INTERNAL_PATH "script/web/service.sh", NULL});
}

void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlSSHD,
            ui_pnlSFTPGo,
            ui_pnlTTYD,
            ui_pnlSyncthing,
            ui_pnlRSLSync,
            ui_pnlNTP,
            ui_pnlTailscaled,
    };

    ui_objects[0] = ui_lblSSHD;
    ui_objects[1] = ui_lblSFTPGo;
    ui_objects[2] = ui_lblTTYD;
    ui_objects[3] = ui_lblSyncthing;
    ui_objects[4] = ui_lblRSLSync;
    ui_objects[5] = ui_lblNTP;
    ui_objects[6] = ui_lblTailscaled;

    lv_obj_t *ui_objects_value[] = {
            ui_droSSHD,
            ui_droSFTPGo,
            ui_droTTYD,
            ui_droSyncthing,
            ui_droRSLSync,
            ui_droNTP,
            ui_droTailscaled
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoSSHD,
            ui_icoSFTPGo,
            ui_icoTTYD,
            ui_icoSyncthing,
            ui_icoRSLSync,
            ui_icoNTP,
            ui_icoTailscaled
    };

    apply_theme_list_panel(ui_pnlSSHD);
    apply_theme_list_panel(ui_pnlSFTPGo);
    apply_theme_list_panel(ui_pnlTTYD);
    apply_theme_list_panel(ui_pnlSyncthing);
    apply_theme_list_panel(ui_pnlRSLSync);
    apply_theme_list_panel(ui_pnlNTP);
    apply_theme_list_panel(ui_pnlTailscaled);

    apply_theme_list_item(&theme, ui_lblSSHD, lang.MUXWEBSERV.SHELL);
    apply_theme_list_item(&theme, ui_lblSFTPGo, lang.MUXWEBSERV.SFTP);
    apply_theme_list_item(&theme, ui_lblTTYD, lang.MUXWEBSERV.TERMINAL);
    apply_theme_list_item(&theme, ui_lblSyncthing, lang.MUXWEBSERV.SYNCTHING);
    apply_theme_list_item(&theme, ui_lblRSLSync, lang.MUXWEBSERV.RESILIO);
    apply_theme_list_item(&theme, ui_lblNTP, lang.MUXWEBSERV.NTP);
    apply_theme_list_item(&theme, ui_lblTailscaled, lang.MUXWEBSERV.TAILSCALE);

    apply_theme_list_glyph(&theme, ui_icoSSHD, mux_module, "sshd");
    apply_theme_list_glyph(&theme, ui_icoSFTPGo, mux_module, "sftpgo");
    apply_theme_list_glyph(&theme, ui_icoTTYD, mux_module, "ttyd");
    apply_theme_list_glyph(&theme, ui_icoSyncthing, mux_module, "syncthing");
    apply_theme_list_glyph(&theme, ui_icoRSLSync, mux_module, "rslsync");
    apply_theme_list_glyph(&theme, ui_icoNTP, mux_module, "ntp");
    apply_theme_list_glyph(&theme, ui_icoTailscaled, mux_module, "tailscaled");

    char options[MAX_BUFFER_SIZE];
    snprintf(options, sizeof(options), "%s\n%s",
             lang.GENERIC.DISABLED, lang.GENERIC.ENABLED);
    apply_theme_list_drop_down(&theme, ui_droSSHD, options);
    apply_theme_list_drop_down(&theme, ui_droSFTPGo, options);
    apply_theme_list_drop_down(&theme, ui_droTTYD, options);
    apply_theme_list_drop_down(&theme, ui_droSyncthing, options);
    apply_theme_list_drop_down(&theme, ui_droRSLSync, options);
    apply_theme_list_drop_down(&theme, ui_droNTP, options);
    apply_theme_list_drop_down(&theme, ui_droTailscaled, options);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
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

void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    play_sound("back", nav_sound, 0, 1);

    save_web_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "service");
    mux_input_stop();
}

void handle_help(void) {
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu,
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblSSHD, "sshd");
    lv_obj_set_user_data(ui_lblSFTPGo, "sftpgo");
    lv_obj_set_user_data(ui_lblTTYD, "ttyd");
    lv_obj_set_user_data(ui_lblSyncthing, "syncthing");
    lv_obj_set_user_data(ui_lblRSLSync, "rslsync");
    lv_obj_set_user_data(ui_lblNTP, "ntp");
    lv_obj_set_user_data(ui_lblTailscaled, "tailscaled");

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
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXWEBSERV.TITLE);
    init_mux(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_element_events();
    init_navigation_sound(&nav_sound, mux_module);

    restore_web_options();
    init_dropdown_settings();

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);
    init_timer(ui_refresh_task, NULL);

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
                    [MUX_INPUT_A] = handle_option_next,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
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
