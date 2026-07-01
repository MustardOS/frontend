#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "../lvgl/lvgl.h"
#include "../lvgl/src/draw/sdl/lv_draw_sdl.h"
#include "display.h"
#include "init.h"
#include "input.h"
#include "inotify.h"
#include "ui/font.h"
#include "log.h"
#include "ui/common.h"
#include "language.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "battery.h"
#include "board.h"
#include "theme.h"
#include "svg.h"
#include "crash.h"
#include "image.h"
#include "fileio.h"
#include "exec.h"
#include "datetime.h"
#include "sysinfo.h"

static uint64_t start_ms = 0;
static lv_timer_t *mux_refresh_timer = NULL;

static void (*status_sysinfo_cb)(const lv_timer_t *) = NULL;

lv_timer_t *timer_ui_refresh;
lv_timer_t *timer_status;
lv_timer_t *timer_idle;

char mux_module[MAX_BUFFER_SIZE];
char mux_dim[15];
int msgbox_active;
lv_obj_t *msgbox_element;
int progress_onscreen = -1;
int block_input;
int page_nav_blocked;
int last_idle = -1;
int current_brightness = 0;
int current_volume = 0;
int is_blank = 0;
int config_auth = 0;
int idle_state_exists = 0;
int safe_quit_exists = 0;
int hdmi_refresh_exists = 0;
int blank_exists = 0;
unsigned idle_state_changes = 0;
unsigned saver_type_changes = 0;
unsigned charging_changes = 0;
int hdmi_mode = 0;
int g350_menu_pressed = 0;

static lv_timer_t **const timers[] = {
    &timer_ui_refresh,
    &timer_status,
    &timer_idle,
};

uint64_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    const uint64_t now_ms = (uint64_t) tv_now.tv_sec * 1000 + tv_now.tv_nsec / 1000000;
    if (!start_ms) start_ms = now_ms;

    return now_ms - start_ms;
}

void inotify_init(void) {
    ino_proc = inotify_create();
    if (!ino_proc) return;

    inotify_track(ino_proc, "/run/muos", "idle_state", &idle_state_exists, &idle_state_changes);
    inotify_track(ino_proc, "/run/muos", "safe_quit", &safe_quit_exists, 0);
    inotify_track(ino_proc, "/run/muos", "hdmi_refresh", &hdmi_refresh_exists, 0);
    inotify_track(ino_proc, "/run/muos", "blank", &blank_exists, 0);

    static int saver_type_exists;
    inotify_track(ino_proc, "/opt/muos/config/settings/power", "saver_type", &saver_type_exists, &saver_type_changes);

    static int charging_exists;
    inotify_track(ino_proc, "/run/muos/battery", "charging", &charging_exists, &charging_changes);
}

void detach_parent_process(void) {
    const pid_t pid = fork();

    if (pid < 0) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_fork);
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

    static unsigned last_idle_changes_seen = 0;
    if (ino_proc && idle_state_changes == last_idle_changes_seen) return;
    if (ino_proc) last_idle_changes_seen = idle_state_changes;

    const int idle = read_line_int_from(IDLE_STATE, 1);
    if (idle < 0 || idle == last_idle) return;

    // Screensaver is disabled so we'll just pause the frontend
    if (!config.settings.power.saver_type) {
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

void refresh_screen(const lv_obj_t *screen, const int flush) {
    for (int i = 0, n = flush > 0 ? flush : 1; i < n; i++) {
        lv_obj_invalidate(screen);
        lv_refr_now(NULL);
        lv_task_handler();
    }
}

void safe_quit(const int exit_status) {
    write_text_to_file(SAFE_QUIT, "w", INT, exit_status);
}

void init_module(const char *module) {
    snprintf(mux_module, sizeof(mux_module), "%s", module_from_func(module));
    set_process_name(mux_module);
    load_lang(&lang);
    crash_init(mux_module);
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
            device.mux.width = (int16_t) ext_w;
            device.mux.height = (int16_t) ext_h;
        } else {
            LOG_WARN("video", "Failed to read HDMI external resolution, using default MUX size");
        }
    }

    lv_init();
    img_init();
    svg_init();
    sdl_init();

    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;

    const int double_buffer = config.settings.advanced.double_buffer;
    uint32_t disp_buf_lines = (uint32_t) device.mux.height;

    uint32_t disp_buf_size = (uint32_t) device.mux.width * disp_buf_lines;
    size_t disp_buf_bytes = (size_t) disp_buf_size * sizeof(lv_color_t);

    LOG_INFO(
        "init", "Draw buffer: %s, %u lines (%lu KB)", double_buffer ? "double" : "single", disp_buf_lines,
        disp_buf_bytes / 1024
    );

    static lv_color_t *disp_buf_s1 = NULL;
    static lv_color_t *disp_buf_s2 = NULL;
    static uint32_t disp_buf_pixels = 0;
    static int disp_buf_double = -1;

    if (__builtin_expect(disp_buf_size != disp_buf_pixels || double_buffer != disp_buf_double, 0)) {
        free(disp_buf_s1);
        free(disp_buf_s2);

        disp_buf_s1 = NULL;
        disp_buf_s2 = NULL;

        if (posix_memalign((void **) &disp_buf_s1, 64, disp_buf_bytes) != 0) disp_buf_s1 = NULL;
        if (double_buffer && posix_memalign((void **) &disp_buf_s2, 64, disp_buf_bytes) != 0) disp_buf_s2 = NULL;

        if (!disp_buf_s1 || (double_buffer && !disp_buf_s2)) {
            LOG_ERROR("init", "Failed to allocate display buffer(s)!");
            LOG_ERROR("init", "Requested %zu KB each. Out of memory?", disp_buf_bytes / 1024);

            free(disp_buf_s1);
            disp_buf_s1 = NULL;

            free(disp_buf_s2);
            disp_buf_s2 = NULL;

            exit(EXIT_FAILURE);
        }

        memset(disp_buf_s1, 0, disp_buf_bytes);
        if (disp_buf_s2) memset(disp_buf_s2, 0, disp_buf_bytes);

        disp_buf_pixels = disp_buf_size;
        disp_buf_double = double_buffer;
    }

    lv_disp_draw_buf_init(&disp_buf, disp_buf_s1, disp_buf_s2, disp_buf_size);
    lv_disp_drv_init(&disp_drv);

    static lv_draw_sdl_drv_param_t sdl_drv_param;
    sdl_drv_param.renderer = display_get_renderer();
    disp_drv.user_data = &sdl_drv_param;
    disp_drv.draw_ctx_init = lv_draw_sdl_init_ctx;
    disp_drv.draw_ctx_deinit = lv_draw_sdl_deinit_ctx;
    disp_drv.draw_ctx_size = sizeof(lv_draw_sdl_ctx_t);

    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = display_flush;
    disp_drv.hor_res = device.mux.width;
    disp_drv.ver_res = device.mux.height;
    disp_drv.physical_hor_res = -1;
    disp_drv.physical_ver_res = -1;
    disp_drv.offset_x = 0;
    disp_drv.offset_y = 0;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    disp_drv.antialiasing = theme.misc.antialiasing;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);
    disp_drv.clear_cb = clear_cb;
    disp_drv.screen_transp = 1;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    if (!disp) {
        LOG_ERROR("init", "No display could be drawn to!");
        exit(EXIT_FAILURE);
    }

    {
        lv_draw_sdl_ctx_t *draw_ctx = (lv_draw_sdl_ctx_t *) disp->driver->draw_ctx;
        if (draw_ctx) draw_ctx->shadow_layer = display_get_shadow_layer();
    }

    mux_set_refresh_timer(disp->refr_timer);
}

void init_input(mux_input_options *opts, const int def_combo) {
    if (!opts) return;

    board_init(device.board.name);

    opts->max_idle_ms = IDLE_MS;
    opts->nav = get_sticknav_mask(config.settings.advanced.stick_nav);
    opts->remap_to_dpad = 1;

    if (def_combo) {
        const mux_input_type bright_mods[] = {mux_input_menu, mux_input_switch};

        for (size_t i = 0; i < sizeof(bright_mods) / sizeof(bright_mods[0]); i++) {
            append_combo(
                opts, (mux_input_combo) {.type_mask = BIT(bright_mods[i]) | BIT(mux_input_vol_up),
                                         .press_handler = ui_common_handle_bright_up,
                                         .hold_handler = ui_common_handle_bright_up}
            );

            append_combo(
                opts, (mux_input_combo) {.type_mask = BIT(bright_mods[i]) | BIT(mux_input_vol_down),
                                         .press_handler = ui_common_handle_bright_down,
                                         .hold_handler = ui_common_handle_bright_down}
            );
        }

        append_combo(
            opts, (mux_input_combo) {.type_mask = BIT(mux_input_vol_up),
                                     .press_handler = ui_common_handle_volume_up,
                                     .hold_handler = ui_common_handle_volume_up}
        );

        append_combo(
            opts, (mux_input_combo) {.type_mask = BIT(mux_input_vol_down),
                                     .press_handler = ui_common_handle_volume_down,
                                     .hold_handler = ui_common_handle_volume_down}
        );
    }

    if (opts->idle_handler == NULL) opts->idle_handler = ui_common_handle_idle;
}

static lv_timer_t *timer_ensure(lv_timer_t **timer, const lv_timer_cb_t cb, const uint32_t period) {
    if (*timer == NULL) {
        *timer = lv_timer_create(cb, period, NULL);
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

void timer_action(const int action) {
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

static void status_tick(lv_timer_t *timer) {
    static unsigned ticks = 0;
    ticks++;

    status_task(timer);
    if (status_sysinfo_cb && ticks % 2u == 1) status_sysinfo_cb(timer);

    if (ticks % 4u == 0 && device.board.has_network && config.visual.network) network_task(timer);
    if (ticks % 4u == 2 && device.board.has_bluetooth && config.visual.bluetooth) bluetooth_task(timer);
    if (ticks % 8u == 3 && config.visual.clock) datetime_task(timer);
    if (ticks % 8u == 7 && config.visual.battery) battery_capacity_task(timer);
}

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(const lv_timer_t *)) {
    status_sysinfo_cb = update_system_info;

    timer_ensure(&timer_ui_refresh, ui_refresh_task, TIMER_REFRESH);
    timer_ensure(&timer_status, status_tick, TIMER_STATUS);
    timer_ensure(&timer_idle, mux_idle_poll, TIMER_IDLE);

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

static char last_theme_name[MAX_BUFFER_SIZE] = "";
static int init_fonts_first_call = 1;

void init_fonts(void) {
    if (strcmp(last_theme_name, config.theme.active) != 0) {
        snprintf(last_theme_name, sizeof(last_theme_name), "%s", config.theme.active);
        if (!init_fonts_first_call) {
            if (theme_has_font()) {
                config.settings.advanced.font = 1;
            } else if (config.settings.advanced.font == 1) {
                config.settings.advanced.font = 2;
            }
        }
        init_fonts_first_call = 0;
    }

    static int prev_shadow = -1;
    const int new_shadow = config.visual.render_shadows;
    lv_shadow_set(
        new_shadow, lv_color_hex(theme.list_default.shadow_colour), (lv_opa_t) theme.list_default.shadow_alpha,
        theme.list_default.shadow_x_offset, theme.list_default.shadow_y_offset,
        lv_color_hex(theme.list_focus.shadow_colour), (lv_opa_t) theme.list_focus.shadow_alpha,
        theme.list_focus.shadow_x_offset, theme.list_focus.shadow_y_offset
    );
    lv_glyph_shadow_set(
        new_shadow, lv_color_hex(theme.list_default.shadow_colour), (lv_opa_t) theme.list_default.shadow_alpha,
        theme.list_default.shadow_x_offset, theme.list_default.shadow_y_offset,
        lv_color_hex(theme.list_focus.shadow_colour), (lv_opa_t) theme.list_focus.shadow_alpha,
        theme.list_focus.shadow_x_offset, theme.list_focus.shadow_y_offset
    );

    const int font_context = font_context_changed();
    if (font_context) font_cache_clear();

    load_font_text(ui_screen);

    load_font_section(FONT_PANEL_DIR, ui_pnl_content);
    load_font_section(FONT_HEADER_DIR, ui_pnl_header);
    load_font_section(FONT_FOOTER_DIR, ui_pnl_footer);

    crash_ui_apply_font(ui_screen);
    power_loss_ui_apply_font(ui_screen);

    if (prev_shadow != new_shadow) lv_obj_invalidate(ui_screen);
    prev_shadow = new_shadow;
}

void init_theme(const int panel_init, const int long_mode) {
    theme_base = get_theme_base();
    load_theme(&theme, &config, &device);

    if (panel_init) {
        init_panel_style(&theme);
        init_item_style(&theme);
        init_glyph_style(&theme);
    }

    if (long_mode && theme.list_default.label_long_mode != LV_LABEL_LONG_WRAP) init_item_animation();
}

void status_task(const lv_timer_t *timer) {
    LV_UNUSED(timer);

    if (progress_onscreen > 0) {
        --progress_onscreen;
        return;
    }

    lv_obj_t *panels[] = {ui_pnl_progress_brightness, ui_pnl_progress_volume};

    for (size_t i = 0; i < A_SIZE(panels); ++i) {
        lv_obj_t *obj = panels[i];
        if (obj && lv_obj_is_valid(obj)) {
            if (!lv_obj_has_flag(obj, MU_OBJ_FLAG_HIDE_FLOAT)) {
                ui_common_progress_fade_out(obj);
            }
        }
    }

    if (!msgbox_active) progress_onscreen = -1;
}

void status_poll(void) {
    if (config.visual.clock) datetime_task(NULL);
    if (config.visual.battery) battery_capacity_task(NULL);
}

void bluetooth_task(const lv_timer_t *timer) {
    LV_UNUSED(timer);

    static int last_ui_connected = -1;

    if (!ui_sta_bluetooth || !lv_obj_is_valid(ui_sta_bluetooth)) return;

    const int connected = device.board.has_bluetooth && is_bluetooth_connected();
    if (connected == last_ui_connected) return;

    last_ui_connected = connected;
    update_bluetooth_status(ui_sta_bluetooth, &theme);
}

void network_task(const lv_timer_t *timer) {
    LV_UNUSED(timer);

    static int last_ui_connected = -1;

    if (!ui_sta_network || !lv_obj_is_valid(ui_sta_network)) return;
    if (strcasecmp(mux_module, "muxnetwork") == 0) return;

    const int connected = device.board.has_network && is_network_connected();
    if (connected == last_ui_connected) return;

    last_ui_connected = connected;
    update_network_status(ui_sta_network, &theme, 0);
}
