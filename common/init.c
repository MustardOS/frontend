#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "../lvgl/lvgl.h"
#include "display/sdl.h"
#include "init.h"
#include "input.h"
#include "inotify.h"
#include "common.h"
#include "font.h"
#include "log.h"
#include "ui_common.h"
#include "language.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "battery.h"
#include "board.h"
#include "theme.h"

static uint64_t start_ms = 0;
static struct dt_task_param dt_par;
static struct bat_task_param bat_par;
static lv_timer_t *mux_refresh_timer = NULL;

lv_timer_t *timer_ui_refresh;
lv_timer_t *timer_datetime;
lv_timer_t *timer_capacity;
lv_timer_t *timer_status;
lv_timer_t *timer_idle;
lv_timer_t *timer_bluetooth;
lv_timer_t *timer_network;
lv_timer_t *timer_update_system_info;

static lv_timer_t **const timers[] = {
        &timer_ui_refresh,
        &timer_datetime,
        &timer_capacity,
        &timer_status,
        &timer_idle,
        &timer_bluetooth,
        &timer_network,
        &timer_update_system_info,
};

uint64_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    if (!start_ms) start_ms = now_ms;

    return (uint64_t) (now_ms - start_ms);
}

void inotify_init(void) {
    ino_proc = inotify_create();
    if (!ino_proc) return;

    inotify_track(ino_proc, "/run/muos", "idle_state", &idle_state_exists, 0);
    inotify_track(ino_proc, "/run/muos", "safe_quit", &safe_quit_exists, 0);
    inotify_track(ino_proc, "/run/muos", "hdmi_refresh", &hdmi_refresh_exists, 0);
    inotify_track(ino_proc, "/run/muos", "blank", &blank_exists, 0);
}

void detach_parent_process(void) {
    pid_t pid = fork();

    if (pid < 0) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_FORK);
        exit(EXIT_FAILURE);
    }

    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
}

void mux_set_refresh_timer(lv_timer_t *timer) {
    mux_refresh_timer = timer;
}

lv_timer_t *mux_get_refresh_timer(void) {
    return mux_refresh_timer;
}

static void mux_idle_poll(lv_timer_t *timer) {
    LV_UNUSED(timer);

    int idle = read_line_int_from(IDLE_STATE, 1);
    if (idle < 0 || idle == last_idle) return;

    // Screensaver is disabled so we'll just pause the frontend
    if (!config.SETTINGS.POWER.SAVERTYPE) {
        lv_timer_t *display_timer = mux_get_refresh_timer();

        if (idle) {
            lv_timer_set_period(display_timer, IDLE_FZ);
            lv_timer_pause(display_timer);
        } else {
            lv_timer_resume(display_timer);
            lv_timer_set_period(display_timer, IDLE_MS);
            lv_timer_ready(display_timer);
        }
    }

    last_idle = idle;
}

void refresh_screen(lv_obj_t *screen, int flush) {
    for (int i = 0, n = (flush > 0) ? flush : 1; i < n; i++) {
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

void init_module(const char *module) {
    snprintf(mux_module, sizeof(mux_module), "%s", module_from_func(module));
    set_process_name(mux_module);

    load_lang(&lang);
    common_var_init();
}

static void clear_cb(lv_disp_drv_t *drv, uint8_t *buf, uint32_t size) {
    LV_UNUSED(drv);
    memset(buf, 0, size);
}

void init_display(void) {
    hdmi_mode = file_exist(HDMI_MODE);
    LOG_INFO("video", "HDMI in use: %s", hdmi_mode ? "yes" : "no");

    if (hdmi_mode) {
        int ext_w = read_line_int_from("/opt/muos/device/config/screen/external/width", 1);
        int ext_h = read_line_int_from("/opt/muos/device/config/screen/external/height", 1);

        if (ext_w > 0 && ext_h > 0) {
            LOG_INFO("video", "Overriding MUX resolution for HDMI: %dx%d", ext_w, ext_h);
            device.MUX.WIDTH = (int16_t) ext_w;
            device.MUX.HEIGHT = (int16_t) ext_h;
        } else {
            LOG_WARN("video", "Failed to read HDMI external resolution, using default MUX size");
        }
    }

    lv_init();
    sdl_init();

    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;

    uint32_t disp_buf_lines = 64;
    if (disp_buf_lines > (uint32_t) device.MUX.HEIGHT) disp_buf_lines = (uint32_t) device.MUX.HEIGHT;

    uint32_t disp_buf_size = (uint32_t) device.MUX.WIDTH * disp_buf_lines;
    size_t disp_buf_bytes = (size_t) disp_buf_size * sizeof(lv_color_t);

    LOG_INFO("init", "Draw buffer: %u lines (%lu KB)", disp_buf_lines, disp_buf_bytes / 1024);

    static lv_color_t *disp_buf_s1 = NULL;
    static lv_color_t *disp_buf_s2 = NULL;
    static uint32_t disp_buf_pixels = 0;

    if (__builtin_expect(!!(disp_buf_size != disp_buf_pixels), 0)) {
        free(disp_buf_s1);
        free(disp_buf_s2);
        disp_buf_s1 = NULL;
        disp_buf_s2 = NULL;

        if (posix_memalign((void **) &disp_buf_s1, 64, disp_buf_bytes) != 0) disp_buf_s1 = NULL;
        if (posix_memalign((void **) &disp_buf_s2, 64, disp_buf_bytes) != 0) disp_buf_s2 = NULL;

        if (!disp_buf_s1 || !disp_buf_s2) {
            LOG_ERROR("init", "Failed to allocate display buffer(s)!");
            LOG_ERROR("init", "Requested %zu KB each. Out of memory?", disp_buf_bytes / 1024);

            free(disp_buf_s1);
            disp_buf_s1 = NULL;

            free(disp_buf_s2);
            disp_buf_s2 = NULL;

            exit(EXIT_FAILURE);
        }

        memset(disp_buf_s1, 0, disp_buf_bytes);
        memset(disp_buf_s2, 0, disp_buf_bytes);

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

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    if (!disp) {
        LOG_ERROR("init", "No display could be drawn to!");
        exit(EXIT_FAILURE);
    }

    mux_set_refresh_timer(disp->refr_timer);
}

void init_input(mux_input_options *opts, int def_combo) {
    if (!opts) return;

    board_init(device.BOARD.NAME);

    opts->max_idle_ms = IDLE_MS;
    opts->swap_btn = config.SETTINGS.ADVANCED.SWAP;
    opts->nav = get_sticknav_mask(config.SETTINGS.ADVANCED.STICKNAV);
    opts->remap_to_dpad = true;

    if (def_combo) {
        mux_input_type bright_mods[] = {
                MUX_INPUT_MENU,
                MUX_INPUT_SWITCH
        };

        for (size_t i = 0; i < sizeof(bright_mods) / sizeof(bright_mods[0]); i++) {
            append_combo(opts, (mux_input_combo) {
                    .type_mask = BIT(bright_mods[i]) | BIT(MUX_INPUT_VOL_UP),
                    .press_handler = ui_common_handle_bright_up,
                    .hold_handler = ui_common_handle_bright_up
            });

            append_combo(opts, (mux_input_combo) {
                    .type_mask = BIT(bright_mods[i]) | BIT(MUX_INPUT_VOL_DOWN),
                    .press_handler = ui_common_handle_bright_down,
                    .hold_handler = ui_common_handle_bright_down
            });
        }

        append_combo(opts, (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_VOL_UP),
                .press_handler = ui_common_handle_volume_up,
                .hold_handler = ui_common_handle_volume_up
        });

        append_combo(opts, (mux_input_combo) {
                .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                .press_handler = ui_common_handle_volume_down,
                .hold_handler = ui_common_handle_volume_down
        });
    }

    if (opts->idle_handler == NULL) opts->idle_handler = ui_common_handle_idle;
}

static lv_timer_t *timer_ensure(lv_timer_t **timer, lv_timer_cb_t cb, uint32_t period, void *user_data) {
    if (*timer == NULL) {
        *timer = lv_timer_create(cb, period, user_data);
        return *timer;
    }

    if ((*timer)->timer_cb != cb) lv_timer_set_cb(*timer, cb);
    if ((*timer)->period != period) lv_timer_set_period(*timer, period);

    lv_timer_resume(*timer);

    return *timer;
}

static void timer_suspend(lv_timer_t **timer) {
    if (*timer) lv_timer_pause(*timer);
}

static void timer_resume(lv_timer_t **timer) {
    if (*timer) lv_timer_resume(*timer);
}

static void timer_destroy(lv_timer_t **timer) {
    if (*timer) {
        lv_timer_del(*timer);
        *timer = NULL;
    }
}

void timer_action(int action) {
    for (size_t i = 0; i < A_SIZE(timers); ++i) {
        lv_timer_t **t = timers[i];
        if (!t || !*t) continue;

        switch (action) {
            case 0:
                timer_suspend(t);
                break;
            case 1:
                timer_resume(t);
                break;
            case 2:
                timer_destroy(t);
                break;
            default:
                LOG_WARN(mux_module, "Timer issue warning - No suspend, resume, or destroy!");
        }
    }
}

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(lv_timer_t *)) {
    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    timer_ensure(&timer_ui_refresh, ui_refresh_task, TIMER_REFRESH, NULL);
    timer_ensure(&timer_status, status_task, TIMER_STATUS, NULL);
    timer_ensure(&timer_idle, mux_idle_poll, TIMER_IDLE, NULL);

    if (config.VISUAL.CLOCK) {
        timer_ensure(&timer_datetime, datetime_task, TIMER_DATETIME, &dt_par);
    } else {
        timer_suspend(&timer_datetime);
    }

    if (config.VISUAL.BATTERY) {
        timer_ensure(&timer_capacity, battery_capacity_task, TIMER_CAPACITY, &bat_par);
    } else {
        timer_suspend(&timer_capacity);
    }

    if (device.BOARD.HASBLUETOOTH && config.VISUAL.BLUETOOTH) {
        timer_ensure(&timer_bluetooth, bluetooth_task, TIMER_BLUETOOTH, NULL);
    } else {
        timer_suspend(&timer_bluetooth);
    }

    if (device.BOARD.HASNETWORK && config.VISUAL.NETWORK) {
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

void timer_suspend_all(void) {
    timer_action(0);
}

void timer_resume_all(void) {
    timer_action(1);
}

void timer_destroy_all(void) {
    timer_action(2);
}

void init_fonts(void) {
    int font_context = font_context_changed();
    if (font_context) font_cache_clear();

    load_font_text(ui_screen);

    load_font_section(FONT_PANEL_DIR, ui_pnlContent);
    load_font_section(FONT_HEADER_DIR, ui_pnlHeader);
    load_font_section(FONT_FOOTER_DIR, ui_pnlFooter);
}

void init_theme(int panel_init, int long_mode) {
    theme_base = get_theme_base();
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
        lv_obj_t *obj = panels[i];
        if (obj && lv_obj_is_valid(obj)) {
            if (!lv_obj_has_flag(obj, MU_OBJ_FLAG_HIDE_FLOAT)) {
                lv_obj_add_flag(obj, MU_OBJ_FLAG_HIDE_FLOAT);
            }
        }
    }

    if (!msgbox_active) progress_onscreen = -1;
}

void bluetooth_task(lv_timer_t *timer) {
    LV_UNUSED(timer);

//    TODO: Yeah one day...
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
