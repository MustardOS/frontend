#include "muxshare.h"
#include <common/base/totp.h>
#include <common/platform/sysinfo.h>
#include <common/ui/orientation.h>
#include "ui/ui_muxwebcode.h"

#define WEBCODE_SECRET OPT_PATH "config/system/landing_secret"

/* What the screen has to say, decided once on entry. Only state_code shows a code. */
enum webcode_state { state_code, state_no_service, state_no_auth, state_unavailable };

static enum webcode_state screen_state;
static unsigned char code_secret[TOTP_SECRET_SIZE];
static int64_t shown_window = -1;
static int shown_remaining = -1;

static void dashboard_local_address(char *out, const size_t out_size) {
    out[0] = '\0';
    if (!config.web.mdns) return;

    const char *port = config.web.landing_port[0] ? config.web.landing_port : "80";
    const int standard = strcmp(port, "80") == 0;
    char *effective_name = read_line_char_from(RUN_PATH "mdns_name", 1);
    if (effective_name && *effective_name) {
        if (standard)
            snprintf(out, out_size, "http://%s.local", effective_name);
        else
            snprintf(out, out_size, "http://%s.local:%s", effective_name, port);
    }
    free(effective_name);
}

static int dashboard_ip_address(char *out, const size_t out_size) {
    const char *port = config.web.landing_port[0] ? config.web.landing_port : "80";
    const int standard = strcmp(port, "80") == 0;

    char address[64];
    if (!get_any_ipv4_address(address, sizeof(address)) || !address[0]) {
        out[0] = '\0';
        return 0;
    }

    if (standard)
        snprintf(out, out_size, "http://%s", address);
    else
        snprintf(out, out_size, "http://%s:%s", address, port);
    return 1;
}

/* Spaces the digits apart so the code stays readable from across a room. */
static void format_code(const char *code, char *out, const size_t out_size) {
    size_t written = 0;

    for (size_t i = 0; code[i] && written + 2 < out_size; ++i) {
        if (i && written + 3 < out_size) out[written++] = ' ';
        out[written++] = code[i];
    }
    out[written] = '\0';
}

static void refresh_code(void) {
    if (screen_state != state_code) return;

    const int64_t now = (int64_t) time(NULL);
    const int64_t window = totp_window(now);
    const int remaining = totp_remaining(now);

    if (window != shown_window) {
        char code[16];
        char spaced[32];

        totp_code(code_secret, window, code, sizeof(code));
        format_code(code, spaced, sizeof(spaced));
        lv_label_set_text(ui_lbl_code_webcode, spaced);
        shown_window = window;
    }

    /* Only touch the bar and its caption on a whole second, so the screen is not being
       redrawn sixty times over for a countdown that moves once. */
    if (remaining == shown_remaining) return;
    shown_remaining = remaining;

    lv_bar_set_value(ui_bar_expiry_webcode, remaining, LV_ANIM_OFF);

    char expiry[MAX_BUFFER_SIZE];
    snprintf(expiry, sizeof(expiry), lang.muxwebcode.expires, remaining);
    lv_label_set_text(ui_lbl_expiry_webcode, expiry);
}

static void ui_refresh_task(lv_timer_t *timer) {
    refresh_code();
    ui_gen_refresh_task(timer);
}

static void handle_b(void) {
    if (hold_call) return;

    if (orientation_handle_skip()) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "webcode");

    mux_input_stop();
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_help(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_info_open);
    show_info_box(lang.muxwebcode.title, lang.muxwebcode.overview, 0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});

    overlay_display();
}

/* Works out what to show, and reads the secret only when a code is actually wanted. */
static void resolve_state(void) {
    if (!config.web.landing) {
        screen_state = state_no_service;
        return;
    }
    if (!config.web.landing_auth) {
        screen_state = state_no_auth;
        return;
    }

    screen_state = totp_secret_load(WEBCODE_SECRET, code_secret) ? state_code : state_unavailable;
}

static void apply_state(void) {
    const int showing_code = screen_state == state_code;
    const int showing_service = screen_state != state_no_service;

    lv_obj_t *const code_parts[] = {
        ui_lbl_code_webcode, ui_pnl_expiry_webcode, ui_lbl_expiry_webcode
    };

    for (size_t i = 0; i < A_SIZE(code_parts); ++i) {
        if (showing_code)
            lv_obj_clear_flag(code_parts[i], MU_OBJ_FLAG_HIDE_FLOAT);
        else
            lv_obj_add_flag(code_parts[i], MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (showing_code)
        lv_obj_add_flag(ui_lbl_notice_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
    else
        lv_obj_clear_flag(ui_lbl_notice_webcode, MU_OBJ_FLAG_HIDE_FLOAT);

    if (showing_code) {
        lv_bar_set_range(ui_bar_expiry_webcode, 0, TOTP_STEP);
        refresh_code();
    }

    if (!showing_code) {
        lv_label_set_text(
            ui_lbl_notice_webcode, screen_state == state_no_service ? lang.muxwebcode.no_service
                                   : screen_state == state_no_auth  ? lang.muxwebcode.no_auth
                                                                    : lang.muxwebcode.unavailable
        );
    }

    char local_address[MAX_BUFFER_SIZE];
    char ip_address[MAX_BUFFER_SIZE];
    dashboard_local_address(local_address, sizeof(local_address));
    const int has_ip_address = showing_service && dashboard_ip_address(ip_address, sizeof(ip_address));

    if (showing_service && local_address[0]) {
        lv_label_set_text_fmt(ui_lbl_local_address_webcode, "%s: %s", lang.muxwebcode.local_address, local_address);
        lv_obj_clear_flag(ui_lbl_local_address_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_local_address_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (has_ip_address) {
        lv_label_set_text_fmt(ui_lbl_ip_address_webcode, "%s: %s", lang.muxwebcode.ip_address, ip_address);
        lv_obj_clear_flag(ui_lbl_ip_address_webcode, MU_OBJ_FLAG_HIDE_FLOAT);

        if (lv_qrcode_update(ui_qr_address_webcode, ip_address, strlen(ip_address)) == LV_RES_OK)
            lv_obj_clear_flag(ui_pnl_qr_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
        else
            lv_obj_add_flag(ui_pnl_qr_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_ip_address_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_pnl_qr_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
        if (showing_service && showing_code) {
            lv_label_set_text(ui_lbl_notice_webcode, lang.muxwebcode.no_address);
            lv_obj_clear_flag(ui_lbl_notice_webcode, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }
}

int muxwebcode_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxwebcode.title);
    init_muxwebcode(ui_pnl_content, load_font_pass_roller());

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_elements();

    resolve_state();
    apply_state();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler = {[mux_input_b] = handle_b, [mux_input_x] = handle_x},
        .release_handler = {[mux_input_menu] = handle_help}
    };

    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxwebcode.title, lang.muxwebcode.overview);

    mux_input_task(&input_opts);

    return 0;
}
