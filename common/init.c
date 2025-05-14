#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/display/sdl.h"
#include "../lvgl/src/drivers/input/evdev.h"
#include "init.h"
#include "input.h"
#include "common.h"
#include "ui_common.h"
#include "language.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "theme.h"

__thread uint64_t start_ms = 0;
static struct dt_task_param dt_par;
static struct bat_task_param bat_par;
static lv_indev_t *indev = NULL;
static lv_indev_drv_t indev_drv; // must be declared here to prevent LVGL deadlocks
int current_capacity = -1;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

lv_timer_t *timer_ui_refresh;
lv_timer_t *timer_datetime;
lv_timer_t *timer_capacity;
lv_timer_t *timer_status;
lv_timer_t *timer_bluetooth;
lv_timer_t *timer_network;
lv_timer_t *timer_battery;
lv_timer_t *timer_update_system_info;

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    if (!start_ms) start_ms = now_ms;

    return (uint32_t) (now_ms - start_ms);
}

void setup_background_process() {
    pid_t pid = fork();

    if (pid == -1) {
        perror(lang.SYSTEM.FAIL_FORK);
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

void refresh_screen(lv_obj_t *screen) {
    for (int r = 0; r < 3; r++) { /* three is the magic number... */
        lv_obj_invalidate(screen);
        lv_refr_now(NULL);
        lv_task_handler();
    }
}

void safe_quit(int exit_status) {
    write_text_to_file(SAFE_QUIT, "w", INT, exit_status);
}

void close_input() {
    close(joy_general);
    close(joy_power);
    close(joy_volume);
    close(joy_extra);
}

void init_module(char *module) {
    snprintf(mux_module, sizeof(mux_module), "%s", module);
    load_lang(&lang);
}

void init_display() {
    lv_init();
    sdl_init();

    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;

    uint32_t disp_buf_size = device.MUX.WIDTH * device.MUX.HEIGHT;
    lv_color_t *disp_buf_s1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *disp_buf_s2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, disp_buf_s1, disp_buf_s2, disp_buf_size);
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = display_flush;
    disp_drv.hor_res = device.MUX.WIDTH;
    disp_drv.ver_res = device.MUX.HEIGHT;
    disp_drv.physical_hor_res = -1;
    disp_drv.physical_ver_res = -1;
    disp_drv.offset_x = 0;
    disp_drv.offset_y = 0;
    disp_drv.full_refresh = 1;
    disp_drv.direct_mode = 1;
    disp_drv.antialiasing = theme.MISC.ANTIALIASING;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);

    lv_disp_drv_register(&disp_drv);
    lv_disp_flush_ready(&disp_drv);
}

int open_input(const char *path, const char *error_message) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(error_message);
        exit(EXIT_FAILURE);
    }
    return fd;
}

void init_input(mux_input_options *opts, int def_combo) {
    if (!opts) return;

    joy_general = open_input(device.INPUT_EVENT.JOY_GENERAL, lang.SYSTEM.NO_JOY_GENERAL);
    joy_power = open_input(device.INPUT_EVENT.JOY_POWER, lang.SYSTEM.NO_JOY_POWER);
    joy_volume = open_input(device.INPUT_EVENT.JOY_VOLUME, lang.SYSTEM.NO_JOY_VOLUME);
    joy_extra = open_input(device.INPUT_EVENT.JOY_EXTRA, lang.SYSTEM.NO_JOY_EXTRA);

    opts->general_fd = joy_general;
    opts->power_fd = joy_power;
    opts->volume_fd = joy_volume;
    opts->extra_fd = joy_extra;

    if (!indev) {
        lv_indev_drv_init(&indev_drv);

        indev_drv.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv.read_cb = evdev_read;
        indev_drv.user_data = (void *) (intptr_t) opts->general_fd;
        indev_drv.feedback_cb = NULL;

        indev = lv_indev_drv_register(&indev_drv);
    }

    opts->max_idle_ms = IDLE_MS;
    opts->swap_btn = config.SETTINGS.ADVANCED.SWAP;
    opts->stick_nav = true;

    if (def_combo) {
        opts->combo[0] = (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                .press_handler = ui_common_handle_bright_up,
                .hold_handler = ui_common_handle_bright_up
        };
        opts->combo[1] = (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                .press_handler = ui_common_handle_bright_down,
                .hold_handler = ui_common_handle_bright_down
        };
        opts->combo[2] = (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_UP),
                .press_handler = ui_common_handle_bright_up,
                .hold_handler = ui_common_handle_bright_up
        };
        opts->combo[3] = (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_DOWN),
                .press_handler = ui_common_handle_bright_down,
                .hold_handler = ui_common_handle_bright_down
        };
        opts->combo[4] = (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_VOL_UP),
                .press_handler = ui_common_handle_volume_up,
                .hold_handler = ui_common_handle_volume_up
        };
        opts->combo[5] = (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                .press_handler = ui_common_handle_volume_down,
                .hold_handler = ui_common_handle_volume_down
        };
    }

    if (opts->idle_handler == NULL) opts->idle_handler = ui_common_handle_idle;
}

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(lv_timer_t *)) {
    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    timer_ui_refresh = lv_timer_create(ui_refresh_task, TIMER_REFRESH, NULL);
    timer_datetime = lv_timer_create(datetime_task, TIMER_DATETIME, &dt_par);
    timer_capacity = lv_timer_create(capacity_task, TIMER_CAPACITY, &bat_par);
    timer_status = lv_timer_create(status_task, TIMER_STATUS, NULL);

    if (device.DEVICE.HAS_BLUETOOTH && config.VISUAL.BLUETOOTH)
        timer_bluetooth = lv_timer_create(bluetooth_task, TIMER_BLUETOOTH, NULL);
    if (device.DEVICE.HAS_NETWORK && config.VISUAL.NETWORK)
        timer_network = lv_timer_create(network_task, TIMER_NETWORK, NULL);
    if (config.VISUAL.BATTERY) timer_battery = lv_timer_create(battery_task, TIMER_BATTERY, NULL);
    if (update_system_info) timer_update_system_info = lv_timer_create(update_system_info, TIMER_SYSINFO, NULL);
    lv_refr_now(NULL);
}

void init_dispose() {
    if (timer_ui_refresh) {
        lv_timer_del(timer_ui_refresh);
        timer_ui_refresh = NULL;
    }
    if (timer_datetime) {
        lv_timer_del(timer_datetime);
        timer_datetime = NULL;
    }
    if (timer_capacity) {
        lv_timer_del(timer_capacity);
        timer_capacity = NULL;
    }
    if (timer_status) {
        lv_timer_del(timer_status);
        timer_status = NULL;
    }

    if (timer_bluetooth) {
        lv_timer_del(timer_bluetooth);
        timer_bluetooth = NULL;
    }
    if (timer_network) {
        lv_timer_del(timer_network);
        timer_network = NULL;
    }
    if (timer_battery) {
        lv_timer_del(timer_battery);
        timer_battery = NULL;
    }
    if (timer_update_system_info) {
        lv_timer_del(timer_update_system_info);
        timer_update_system_info = NULL;
    }
    if (indev) {
        lv_indev_delete(indev);
        indev = NULL;
    }
}

void init_fonts() {
    load_font_text(ui_screen);
    load_font_section(FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(FONT_FOOTER_FOLDER, ui_pnlFooter);
}

void init_theme(int panel_init, int long_mode) {
    load_theme(&theme, &config, &device);

    if (panel_init) {
        init_panel_style(&theme);
        init_item_style(&theme);
        init_glyph_style(&theme);
    }

    if (long_mode && theme.LIST_DEFAULT.LABEL_LONG_MODE != LV_LABEL_LONG_WRAP) init_item_animation();
}

void status_task() {
    if (progress_onscreen > 0) {
        --progress_onscreen;
    } else {
        lv_obj_t *panels[] = {ui_pnlProgressBrightness, ui_pnlProgressVolume};
        for (size_t i = 0; i < sizeof(panels) / sizeof(panels[0]); ++i) {
            if (!lv_obj_has_flag(panels[i], LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (!msgbox_active) progress_onscreen = -1;
    }
}

void bluetooth_task() {
//    update_bluetooth_status(ui_staBluetooth, &theme);
//    if (!bluetooth_period_set) {
//        bluetooth_period_set = 1;
//        lv_timer_set_period(bluetooth_timer, TIMER_BLUETOOTH);
//    }
}

void network_task() {
    if (!strcasecmp(mux_module, "muxnetwork")) return;
    update_network_status(ui_staNetwork, &theme, 0);
}

void battery_task() {
    int bat_cap = read_line_int_from(device.BATTERY.CHARGER, 1);
    if (bat_cap != current_capacity) {
        current_capacity = bat_cap;
        update_battery_capacity(ui_staCapacity, &theme);
    }
}
