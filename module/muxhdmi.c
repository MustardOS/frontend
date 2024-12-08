#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxhdmi.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_resolution = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

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

int enable_original, resolution_original, space_original, depth_original,
        range_original, scan_original, audio_original;

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
            {ui_lblEnable,     TS("Enable or disable HDMI output.")},
            {ui_lblResolution, TS("Select the resolution for HDMI output, such as 720p or 1080p.")},
            {ui_lblSpace,      TS("Choose the color space, such as RGB or YUV.")},
            {ui_lblDepth,      TS("Set the color depth, such as 8-bit or 10-bit.")},
            {ui_lblRange,      TS("Set the color range of RGB colour space.")},
            {ui_lblScan,       TS("Switch between overscan or underscan to fit the display screen.")},
            {ui_lblAudio,      TS("Switch between device speaker or external monitor audio via HDMI connection.")}
    };

    char *message = TG("No Help Information Found");
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = TG("No Help Information Found");

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

void elements_events_init() {
    lv_obj_t *dropdowns[] = {
            ui_droEnable,
            ui_droResolution,
            ui_droSpace,
            ui_droDepth,
            ui_droRange,
            ui_droScan,
            ui_droAudio
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

void init_dropdown_settings() {
    enable_original = lv_dropdown_get_selected(ui_droEnable);
    resolution_original = lv_dropdown_get_selected(ui_droResolution);
    space_original = lv_dropdown_get_selected(ui_droSpace);
    depth_original = lv_dropdown_get_selected(ui_droDepth);
    range_original = lv_dropdown_get_selected(ui_droRange);
    scan_original = lv_dropdown_get_selected(ui_droScan);
    audio_original = lv_dropdown_get_selected(ui_droAudio);
}

void restore_hdmi_options() {
    lv_dropdown_set_selected(ui_droEnable, config.SETTINGS.HDMI.ENABLED);
    map_drop_down_to_index(ui_droResolution, config.SETTINGS.HDMI.RESOLUTION,
                           (int[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 11, 0);
    lv_dropdown_set_selected(ui_droSpace, config.SETTINGS.HDMI.SPACE);
    lv_dropdown_set_selected(ui_droDepth, config.SETTINGS.HDMI.DEPTH);
    lv_dropdown_set_selected(ui_droRange, config.SETTINGS.HDMI.RANGE);
    lv_dropdown_set_selected(ui_droScan, config.SETTINGS.HDMI.SCAN);
    lv_dropdown_set_selected(ui_droAudio, config.SETTINGS.HDMI.AUDIO);
}

void save_hdmi_options() {
    int idx_enable = lv_dropdown_get_selected(ui_droEnable);
    int idx_resolution = map_drop_down_to_value(lv_dropdown_get_selected(ui_droResolution),
                                                (int[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 12, 0);
    int idx_space = lv_dropdown_get_selected(ui_droSpace);
    int idx_depth = lv_dropdown_get_selected(ui_droDepth);
    int idx_range = lv_dropdown_get_selected(ui_droRange);
    int idx_scan = lv_dropdown_get_selected(ui_droScan);
    int idx_audio = lv_dropdown_get_selected(ui_droAudio);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droEnable) != enable_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/enabled", "w", INT, idx_enable);
    }

    if (lv_dropdown_get_selected(ui_droResolution) != resolution_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/resolution", "w", INT, idx_resolution);
    }

    if (lv_dropdown_get_selected(ui_droSpace) != space_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/space", "w", INT, idx_space);
    }

    if (lv_dropdown_get_selected(ui_droDepth) != depth_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/depth", "w", INT, idx_depth);
    }

    if (lv_dropdown_get_selected(ui_droRange) != range_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/range", "w", INT, idx_range);
    }

    if (lv_dropdown_get_selected(ui_droScan) != scan_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/scan", "w", INT, idx_scan);
    }

    if (lv_dropdown_get_selected(ui_droAudio) != audio_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/hdmi/audio", "w", INT, idx_audio);
    }

    static char hdmi_script[MAX_BUFFER_SIZE];
    if (idx_enable == 0) {
        if (lv_dropdown_get_selected(ui_droEnable) != enable_original) {
            snprintf(hdmi_script, sizeof(hdmi_script),
                     "%s/device/current/script/hdmi_stop.sh", INTERNAL_PATH);
            system(hdmi_script);
        }
    } else {
        if (is_modified > 0) {
            system("killall -q \"hdmi_start.sh\"");
            snprintf(hdmi_script, sizeof(hdmi_script),
                     "%s/device/current/script/hdmi_start.sh", INTERNAL_PATH);
            system(hdmi_script);
        }
    }
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlEnable,
            ui_pnlResolution,
            ui_pnlSpace,
            ui_pnlDepth,
            ui_pnlRange,
            ui_pnlScan,
            ui_pnlAudio
    };

    ui_objects[0] = ui_lblEnable;
    ui_objects[1] = ui_lblResolution;
    ui_objects[2] = ui_lblSpace;
    ui_objects[3] = ui_lblDepth;
    ui_objects[4] = ui_lblRange;
    ui_objects[5] = ui_lblScan;
    ui_objects[6] = ui_lblAudio;

    lv_obj_t *ui_objects_value[] = {
            ui_droEnable,
            ui_droResolution,
            ui_droSpace,
            ui_droDepth,
            ui_droRange,
            ui_droScan,
            ui_droAudio
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoEnable,
            ui_icoResolution,
            ui_icoSpace,
            ui_icoDepth,
            ui_icoRange,
            ui_icoScan,
            ui_icoAudio
    };

    apply_theme_list_panel(&theme, &device, ui_pnlEnable);
    apply_theme_list_panel(&theme, &device, ui_pnlResolution);
    apply_theme_list_panel(&theme, &device, ui_pnlSpace);
    apply_theme_list_panel(&theme, &device, ui_pnlDepth);
    apply_theme_list_panel(&theme, &device, ui_pnlRange);
    apply_theme_list_panel(&theme, &device, ui_pnlScan);
    apply_theme_list_panel(&theme, &device, ui_pnlAudio);

    apply_theme_list_item(&theme, ui_lblEnable, TS("HDMI Active"), false, true);
    apply_theme_list_item(&theme, ui_lblResolution, TS("Resolution"), false, true);
    apply_theme_list_item(&theme, ui_lblSpace, TS("Colour Space"), false, true);
    apply_theme_list_item(&theme, ui_lblDepth, TS("Colour Depth"), false, true);
    apply_theme_list_item(&theme, ui_lblRange, TS("Colour Range"), false, true);
    apply_theme_list_item(&theme, ui_lblScan, TS("Scan Scaling"), false, true);
    apply_theme_list_item(&theme, ui_lblAudio, TS("Audio Output"), false, true);

    apply_theme_list_glyph(&theme, ui_icoEnable, mux_module, "enable");
    apply_theme_list_glyph(&theme, ui_icoResolution, mux_module, "resolution");
    apply_theme_list_glyph(&theme, ui_icoSpace, mux_module, "space");
    apply_theme_list_glyph(&theme, ui_icoDepth, mux_module, "depth");
    apply_theme_list_glyph(&theme, ui_icoRange, mux_module, "range");
    apply_theme_list_glyph(&theme, ui_icoScan, mux_module, "scan");
    apply_theme_list_glyph(&theme, ui_icoAudio, mux_module, "audio");

    apply_theme_list_drop_down(&theme, ui_droEnable, NULL);
    apply_theme_list_drop_down(&theme, ui_droResolution, NULL);
    apply_theme_list_drop_down(&theme, ui_droSpace, NULL);
    apply_theme_list_drop_down(&theme, ui_droDepth, NULL);
    apply_theme_list_drop_down(&theme, ui_droRange, NULL);
    apply_theme_list_drop_down(&theme, ui_droScan, NULL);
    apply_theme_list_drop_down(&theme, ui_droAudio, NULL);

    add_drop_down_options(ui_droEnable, (char *[]) {
            TG("Disabled"), TG("Enabled")}, 2);
    add_drop_down_options(ui_droResolution, (char *[]) {
            TS("480i"), TS("576i"), TS("480p"), TS("576p"), TS("720p + 50hz"), TS("720p + 60hz"),
            TS("1080i + 50hz"), TS("1080i + 60hz"), TS("1080p + 24hz"), TS("1080p + 50hz"), TS("1080p + 60hz")}, 11);
    add_drop_down_options(ui_droSpace, (char *[]) {
            TS("RGB"), TS("YUV444"), TS("YUV422"), TS("YUV420")}, 4);
    add_drop_down_options(ui_droDepth, (char *[]) {
            TS("8"), TS("10"), TS("12"), TS("16")}, 4);
    add_drop_down_options(ui_droRange, (char *[]) {
            TS("Limited"), TS("Full")}, 2);
    add_drop_down_options(ui_droScan, (char *[]) {
            TS("Over"), TS("Under")}, 2);
    add_drop_down_options(ui_droAudio, (char *[]) {
            TS("External"), TS("Internal")}, 2);

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

void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
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

    save_hdmi_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "hdmi");
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

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, TG("Save"));

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
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblEnable, "enable");
    lv_obj_set_user_data(ui_lblResolution, "resolution");
    lv_obj_set_user_data(ui_lblSpace, "space");
    lv_obj_set_user_data(ui_lblDepth, "depth");
    lv_obj_set_user_data(ui_lblRange, "range");
    lv_obj_set_user_data(ui_lblScan, "scan");
    lv_obj_set_user_data(ui_lblAudio, "audio");

    if (TEST_IMAGE) display_testing_message(ui_screen);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);
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

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

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
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, TS("HDMI SETTINGS"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();
    elements_events_init();

    restore_hdmi_options();
    init_dropdown_settings();

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
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
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

    refresh_screen(device.SCREEN.WAIT);
    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
