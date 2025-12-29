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
#include "font.h"
#include "log.h"
#include "ui_common.h"
#include "language.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "theme.h"

static uint64_t start_ms = 0;
static struct dt_task_param dt_par;
static struct bat_task_param bat_par;
static lv_indev_t *indev = NULL;
static lv_indev_drv_t indev_drv; // must be declared here to prevent LVGL deadlocks

static int joy_general = -1;
static int joy_power = -1;
static int joy_volume = -1;
static int joy_extra = -1;

lv_timer_t *timer_ui_refresh;
lv_timer_t *timer_datetime;
lv_timer_t *timer_capacity;
lv_timer_t *timer_status;
lv_timer_t *timer_bluetooth;
lv_timer_t *timer_network;
lv_timer_t *timer_update_system_info;

static lv_timer_t **const timers[] = {
        &timer_ui_refresh,
        &timer_datetime,
        &timer_capacity,
        &timer_status,
        &timer_bluetooth,
        &timer_network,
        &timer_update_system_info,
};

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    if (!start_ms) start_ms = now_ms;

    return (uint32_t) (now_ms - start_ms);
}

void detach_parent_process(void) {
    pid_t pid = fork();

    if (pid < 0) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_FORK)
        exit(EXIT_FAILURE);
    }

    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
}

void refresh_screen(lv_obj_t *screen) {
    for (int r = 0; r < LVGL_REFRESH_PASS; r++) {
        lv_obj_invalidate(screen);
        lv_refr_now(NULL);
        lv_task_handler();
    }
}

void safe_quit(int exit_status) {
    write_text_to_file(SAFE_QUIT, "w", INT, exit_status);
}

static void close_fd(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

void close_input(void) {
    close_fd(&joy_general);
    close_fd(&joy_power);
    close_fd(&joy_volume);
    close_fd(&joy_extra);
}

void init_module(const char *module) {
    snprintf(mux_module, sizeof(mux_module), "%s", module_from_func(module));
    set_process_name(mux_module);
    load_lang(&lang);
}

static void clear_cb(lv_disp_drv_t *drv, uint8_t *buf, uint32_t size) {
    LV_UNUSED(drv);
    memset(buf, 0, size);
}

void init_display() {
    lv_init();
    sdl_init();

    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;

    uint32_t buf_lines = device.MUX.HEIGHT / 4;
    uint32_t disp_buf_size = device.MUX.WIDTH * buf_lines;

    LOG_INFO("init", "Draw buffer: %u lines (%lu KB)", buf_lines, (disp_buf_size * sizeof(lv_color_t)) / 1024)

    static lv_color_t *disp_buf_s1 = NULL;
    static lv_color_t *disp_buf_s2 = NULL;
    static uint32_t disp_buf_pixels = 0;

    if (disp_buf_size != disp_buf_pixels) {
        free(disp_buf_s1);
        free(disp_buf_s2);

        disp_buf_s1 = malloc(disp_buf_size * sizeof(lv_color_t));
        disp_buf_s2 = malloc(disp_buf_size * sizeof(lv_color_t));

        disp_buf_pixels = disp_buf_size;
    }

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
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    disp_drv.antialiasing = theme.MISC.ANTIALIASING;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);
    disp_drv.clear_cb = clear_cb;

    lv_disp_drv_register(&disp_drv);
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
    opts->nav = get_sticknav_mask(config.SETTINGS.ADVANCED.STICKNAV);
    opts->remap_to_dpad = true;

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

void dispose_input() {
    if (indev) {
        lv_indev_delete(indev);
        indev = NULL;
    }
}

static lv_timer_t *timer_ensure(lv_timer_t **timer, lv_timer_cb_t cb, uint32_t period, void *user_data) {
    if (*timer == NULL) {
        *timer = lv_timer_create(cb, period, user_data);
        return *timer;
    }

    lv_timer_set_cb(*timer, cb);
    lv_timer_set_period(*timer, period);
    lv_timer_resume(*timer);

    return *timer;
}

static void timer_suspend(lv_timer_t **timer) {
    if (*timer) {
        lv_timer_pause(*timer);
    }
}

static void timer_destroy(lv_timer_t **timer) {
    if (*timer) {
        lv_timer_del(*timer);
        *timer = NULL;
    }
}

void timer_action(int action) {
    for (size_t i = 0; i < A_SIZE(timers); ++i) {
        switch (action) {
            case 0:
                timer_suspend(timers[i]);
                break;
            case 1:
                timer_destroy(timers[i]);
                break;
            default: LOG_WARN(mux_module, "Timer issue warning - No suspend or destroy!")
        }
    }
}

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(lv_timer_t *)) {
    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    timer_ensure(&timer_ui_refresh, ui_refresh_task, TIMER_REFRESH, NULL);
    timer_ensure(&timer_status, status_task, TIMER_STATUS, NULL);

    if (config.VISUAL.CLOCK) {
        timer_ensure(&timer_datetime, datetime_task, TIMER_DATETIME, &dt_par);
    } else {
        timer_suspend(&timer_datetime);
    }

    if (config.VISUAL.BATTERY) {
        timer_ensure(&timer_capacity, capacity_task, TIMER_CAPACITY, &bat_par);
    } else {
        timer_suspend(&timer_capacity);
    }

    if (device.BOARD.HAS_BLUETOOTH && config.VISUAL.BLUETOOTH) {
        timer_ensure(&timer_bluetooth, bluetooth_task, TIMER_BLUETOOTH, NULL);
    } else {
        timer_suspend(&timer_bluetooth);
    }

    if (device.BOARD.HAS_NETWORK && config.VISUAL.NETWORK) {
        timer_ensure(&timer_network, network_task, TIMER_NETWORK, NULL);
    } else {
        timer_suspend(&timer_network);
    }

    if (update_system_info) {
        timer_ensure(&timer_update_system_info, update_system_info, TIMER_SYSINFO, NULL);
    } else {
        timer_suspend(&timer_update_system_info);
    }

    lv_refr_now(NULL);
}

void timer_destroy_all(void) {
    timer_action(1);
}

void timer_suspend_all(void) {
    timer_action(0);
}

void init_fonts(void) {
    int font_context = font_context_changed();
    if (font_context) font_cache_clear();

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

void status_task(lv_timer_t *timer) {
    LV_UNUSED(timer);

    if (progress_onscreen > 0) {
        --progress_onscreen;
        return;
    }

    lv_obj_t *panels[] = {
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume
    };

    for (size_t i = 0; i < A_SIZE(panels); ++i) {
        if (panels[i] && lv_obj_is_valid(panels[i])) {
            lv_obj_add_flag(panels[i], MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }

    if (!msgbox_active) progress_onscreen = -1;
}

void bluetooth_task(lv_timer_t *timer) {
    LV_UNUSED(timer);

    // TODO: Yeah one day...
//    update_bluetooth_status(ui_staBluetooth, &theme);
//    if (!bluetooth_period_set) {
//        bluetooth_period_set = 1;
//        lv_timer_set_period(bluetooth_timer, TIMER_BLUETOOTH);
//    }
}

void network_task(lv_timer_t *timer) {
    LV_UNUSED(timer);

    if (!ui_staNetwork || !lv_obj_is_valid(ui_staNetwork)) return;
    if (strcasecmp(mux_module, "muxnetwork") == 0) return;

    update_network_status(ui_staNetwork, &theme, 0);
}
