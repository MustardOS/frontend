#include "muxshare.h"
#include "ui/ui_muxrgb.h"

#define RGB(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(RGB_ELEMENTS) };
#undef RGB

#define RGB_ZONE_L      (1u << 0)
#define RGB_ZONE_R      (1u << 1)
#define RGB_ZONE_M      (1u << 2)
#define RGB_ZONE_F1     (1u << 3)
#define RGB_ZONE_F2     (1u << 4)
#define RGB_ZONE_SINGLE (1u << 5)
#define RGB_ZONE_RS1    (1u << 6)
#define RGB_ZONE_RS2    (1u << 7)
#define RGB_ZONE_SHL1   (1u << 8)
#define RGB_ZONE_SHL2   (1u << 9)
#define RGB_ZONE_SHR2   (1u << 10)
#define RGB_ZONE_SHR1   (1u << 11)

typedef struct {
    const char *code;
    uint32_t zones;
} rgb_device_caps_t;

static const rgb_device_caps_t rgb_devices[] = {
    {"gcs-h36s", RGB_ZONE_L | RGB_ZONE_R},

    {"rg40xx-h", RGB_ZONE_L | RGB_ZONE_R},

    {"rg40xx-v", RGB_ZONE_L | RGB_ZONE_SINGLE},

    {"rgcubexx-h", RGB_ZONE_L | RGB_ZONE_R},

    {"rg-vita-pro", RGB_ZONE_L | RGB_ZONE_SINGLE},

    {"tui-brick", RGB_ZONE_M | RGB_ZONE_F1 | RGB_ZONE_F2 | RGB_ZONE_SHL1 | RGB_ZONE_SHL2 | RGB_ZONE_SHR2
                      | RGB_ZONE_SHR1},

    {"tui-brick-pro", RGB_ZONE_L | RGB_ZONE_R | RGB_ZONE_M | RGB_ZONE_F1 | RGB_ZONE_F2 | RGB_ZONE_RS1 | RGB_ZONE_RS2
                          | RGB_ZONE_SHL1 | RGB_ZONE_SHL2 | RGB_ZONE_SHR2 | RGB_ZONE_SHR1},

    {"tui-spoon", RGB_ZONE_L | RGB_ZONE_R | RGB_ZONE_M},
};
#define RGB_DEVICE_COUNT A_SIZE(rgb_devices)

static const rgb_device_caps_t *rgb_caps_for(const char *code) {
    if (!code) return NULL;
    for (size_t i = 0; i < RGB_DEVICE_COUNT; i++) {
        if (strcmp(code, rgb_devices[i].code) == 0) return &rgb_devices[i];
    }
    return NULL;
}

static const rgb_device_caps_t *rgb_caps = NULL;

#define RGBZONE_SEL_PATH "/tmp/rgb_zone_sel"

typedef enum {
    zone_l = 0,
    zone_r,
    zone_m,
    zone_f1,
    zone_f2,
    zone_rs1,
    zone_rs2,
    zone_shl1,
    zone_shl2,
    zone_shr2,
    zone_shr1,
    zone_count
} zone_id_t;

typedef struct {
    uint32_t cap_bit;
    const char *code;
    lv_obj_t *zone_pnl, *zone_lbl, *zone_ico;
    const char *label;
} zone_entry_t;

static zone_entry_t zones[zone_count];

static int zone_visible(const zone_entry_t *z) {
    return rgb_caps && (rgb_caps->zones & z->cap_bit);
}

static void build_zone_table(void) {
    const int dual_right = rgb_caps && (rgb_caps->zones & RGB_ZONE_RS1);

    zones[zone_l] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_L,
        .code = "l",
        .zone_pnl = ui_pnl_zone_l_rgb,
        .zone_lbl = ui_lbl_zone_l_rgb,
        .zone_ico = ui_ico_zone_l_rgb,
        .label = dual_right ? lang.muxrgb.zone_l_arc1 : lang.muxrgb.zone_l,
    };

    zones[zone_r] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_R,
        .code = "r",
        .zone_pnl = ui_pnl_zone_r_rgb,
        .zone_lbl = ui_lbl_zone_r_rgb,
        .zone_ico = ui_ico_zone_r_rgb,
        .label = dual_right ? lang.muxrgb.zone_l_arc2 : lang.muxrgb.zone_r,
    };

    zones[zone_m] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_M,
        .code = "m",
        .zone_pnl = ui_pnl_zone_m_rgb,
        .zone_lbl = ui_lbl_zone_m_rgb,
        .zone_ico = ui_ico_zone_m_rgb,
        .label = lang.muxrgb.zone_m,
    };

    zones[zone_f1] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_F1,
        .code = "f1",
        .zone_pnl = ui_pnl_zone_f1_rgb,
        .zone_lbl = ui_lbl_zone_f1_rgb,
        .zone_ico = ui_ico_zone_f1_rgb,
        .label = lang.muxrgb.zone_f1,
    };

    zones[zone_f2] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_F2,
        .code = "f2",
        .zone_pnl = ui_pnl_zone_f2_rgb,
        .zone_lbl = ui_lbl_zone_f2_rgb,
        .zone_ico = ui_ico_zone_f2_rgb,
        .label = lang.muxrgb.zone_f2,
    };

    zones[zone_rs1] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_RS1,
        .code = "rs1",
        .zone_pnl = ui_pnl_zone_rs1_rgb,
        .zone_lbl = ui_lbl_zone_rs1_rgb,
        .zone_ico = ui_ico_zone_rs1_rgb,
        .label = lang.muxrgb.zone_rs1,
    };

    zones[zone_rs2] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_RS2,
        .code = "rs2",
        .zone_pnl = ui_pnl_zone_rs2_rgb,
        .zone_lbl = ui_lbl_zone_rs2_rgb,
        .zone_ico = ui_ico_zone_rs2_rgb,
        .label = lang.muxrgb.zone_rs2,
    };

    zones[zone_shl1] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_SHL1,
        .code = "shl1",
        .zone_pnl = ui_pnl_zone_shl1_rgb,
        .zone_lbl = ui_lbl_zone_shl1_rgb,
        .zone_ico = ui_ico_zone_shl1_rgb,
        .label = lang.muxrgb.zone_shl1,
    };

    zones[zone_shl2] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_SHL2,
        .code = "shl2",
        .zone_pnl = ui_pnl_zone_shl2_rgb,
        .zone_lbl = ui_lbl_zone_shl2_rgb,
        .zone_ico = ui_ico_zone_shl2_rgb,
        .label = lang.muxrgb.zone_shl2,
    };

    zones[zone_shr2] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_SHR2,
        .code = "shr2",
        .zone_pnl = ui_pnl_zone_shr2_rgb,
        .zone_lbl = ui_lbl_zone_shr2_rgb,
        .zone_ico = ui_ico_zone_shr2_rgb,
        .label = lang.muxrgb.zone_shr2,
    };

    zones[zone_shr1] = (zone_entry_t) {
        .cap_bit = RGB_ZONE_SHR1,
        .code = "shr1",
        .zone_pnl = ui_pnl_zone_shr1_rgb,
        .zone_lbl = ui_lbl_zone_shr1_rgb,
        .zone_ico = ui_ico_zone_shr1_rgb,
        .label = lang.muxrgb.zone_shr1,
    };
}

static void hide_static_obj(lv_obj_t *pnl, lv_obj_t *lbl, lv_obj_t *ico) {
    if (lv_obj_has_flag(pnl, MU_OBJ_FLAG_HIDE_FLOAT)) return;

    lv_obj_add_flag(pnl, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(lbl, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ico, MU_OBJ_FLAG_HIDE_FLOAT);
    ui_count_static--;
}

static void show_static_obj(lv_obj_t *pnl, lv_obj_t *lbl, lv_obj_t *ico) {
    if (!lv_obj_has_flag(pnl, MU_OBJ_FLAG_HIDE_FLOAT)) return;

    lv_obj_clear_flag(pnl, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(lbl, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ico, MU_OBJ_FLAG_HIDE_FLOAT);

    ui_count_static++;
}

static int zone_at_visible_position(const int pos) {
    int n = 0;
    for (int i = 0; i < zone_count; i++) {
        if (!zone_visible(&zones[i])) continue;
        if (n == pos) return i;
        n++;
    }
    return -1;
}

static void show_zone_list(void) {
    for (int i = 0; i < zone_count; i++) {
        if (zone_visible(&zones[i])) {
            show_static_obj(zones[i].zone_pnl, zones[i].zone_lbl, zones[i].zone_ico);
        } else {
            hide_static_obj(zones[i].zone_pnl, zones[i].zone_lbl, zones[i].zone_ico);
        }
    }
}

static void reset_all_zones(void) {
    char path[MAX_BUFFER_SIZE];

    for (int i = 0; i < zone_count; i++) {
        if (!zone_visible(&zones[i])) continue;

        snprintf(path, sizeof path, CONF_CONFIG_PATH "settings/rgb/colour_%s", zones[i].code);
        write_text_to_file(path, "w", INT, 0);

        snprintf(path, sizeof path, CONF_CONFIG_PATH "settings/rgb/bright_%s", zones[i].code);
        write_text_to_file(path, "w", INT, 255);
    }

    const char *argv[2] = {RGBLED_BIN, "restore"};
    run_exec(argv, 2, 0, 0, NULL, NULL);
}

static void randomise_all_zones(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    char path[MAX_BUFFER_SIZE];

    for (int i = 0; i < zone_count; i++) {
        if (!zone_visible(&zones[i])) continue;

        const int idx = 1 + (int) (random() % (rgb_colour_count - 1));
        snprintf(path, sizeof path, CONF_CONFIG_PATH "settings/rgb/colour_%s", zones[i].code);
        write_text_to_file(path, "w", INT, idx);
    }

    const char *argv[2] = {RGBLED_BIN, "restore"};
    run_exec(argv, 2, 0, 0, NULL, NULL);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    build_zone_table();

    INIT_STATIC_ITEM(-1, rgb, zone_l, zones[zone_l].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_r, zones[zone_r].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_m, zones[zone_m].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_f1, zones[zone_f1].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_f2, zones[zone_f2].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_rs1, zones[zone_rs1].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_rs2, zones[zone_rs2].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_shl1, zones[zone_shl1].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_shl2, zones[zone_shl2].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_shr2, zones[zone_shr2].label, "colour_l", 0);
    INIT_STATIC_ITEM(-1, rgb, zone_shr1, zones[zone_shr1].label, "colour_l", 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!rgb_caps) LOG_WARN(mux_module, "No caps entry for board '%s'; showing all rows", device.board.name);

    show_zone_list();

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 2, 0, 1);
}

static mux_dialogue reset_dlg;
static int reset_dlg_active = 0;

static void handle_dpad_up(void) {
    if (reset_dlg_active) {
        dialogue_handle_dpad(&reset_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (reset_dlg_active) {
        dialogue_handle_dpad(&reset_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (reset_dlg_active) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (reset_dlg_active) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (reset_dlg_active) {
        const mux_confirm_opt opt = (mux_confirm_opt) reset_dlg.selected;
        dialogue_dismiss(&reset_dlg_active, &reset_dlg);

        if (opt == mux_confirm_yep) {
            reset_all_zones();
            play_sound(snd_confirm);
            toast_message(lang.muxrgb.reset_done, tst_wait_m);
        } else {
            play_sound(snd_back);
        }

        return;
    }

    if (msgbox_active || block_input || hold_call) return;

    const int zone_idx = zone_at_visible_position(current_item_index);
    if (zone_idx < 0) return;

    play_sound(snd_confirm);

    write_text_to_file(RGBZONE_SEL_PATH, "w", CHAR, zones[zone_idx].code);
    load_mux("rgbzone");
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, (char *) lv_obj_get_user_data(zones[zone_idx].zone_lbl));

    mux_input_stop();
}

static void handle_b(void) {
    if (reset_dlg_active) {
        dialogue_dismiss(&reset_dlg_active, &reset_dlg);
        play_sound(snd_back);
        return;
    }

    if (msgbox_active || block_input || hold_call) return;

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "rgb");

    mux_input_stop();
}

static void handle_x(void) {
    if (reset_dlg_active || msgbox_active || block_input || hold_call) return;

    play_sound(snd_info_open);
    dialogue_open(&reset_dlg_active, &reset_dlg, &theme);
}

static int y_hold_fired = 0;

static void handle_y_hold(void) {
    if (y_hold_fired || reset_dlg_active || msgbox_active || block_input || hold_call) return;

    y_hold_fired = 1;
    randomise_all_zones();
    play_sound(snd_confirm);
    toast_message(lang.muxrgb.random_toast, tst_wait_m);
}

static void handle_y_release(void) {
    y_hold_fired = 0;
}

static void handle_help(void) {
    if (reset_dlg_active || msgbox_active || progress_onscreen != -1 || !ui_count_static || block_input || hold_call)
        return;

    play_sound(snd_info_open);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.reset, 0},
                                  {NULL, NULL, 0}});

#define RGB(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_rgb, UDATA);
    RGB_ELEMENTS
#undef RGB

    dialogue_init_confirm(
        &reset_dlg, &theme, ui_screen, lang.generic.confirm, lang.muxrgb.reset_confirm, lang.generic.reset,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    overlay_display();
}

int muxrgb_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    rgb_caps = rgb_caps_for(device.board.name);

    init_ui_common_screen(&theme, &device, &lang, lang.muxrgb.title);
    init_muxrgb(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    nav_silent = 1;

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);
    nav_silent = 0;

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
                [mux_input_y] = handle_y_release,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
            [mux_input_y] = handle_y_hold,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
