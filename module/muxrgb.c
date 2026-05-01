#include "muxshare.h"
#include "ui/ui_muxrgb.h"

#define RGB(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(RGB_ELEMENTS)
};
#undef RGB

#define RGB(NAME, ENUM, UDATA) static int NAME##_original;
RGB_ELEMENTS
#undef RGB

typedef enum {
    RGB_MODE_OFF = 0,
    RGB_MODE_STATIC,
    RGB_MODE_BREATHING,
    RGB_MODE_PRESET_COMBO,
    RGB_MODE_THEME_SUPPLIED,
} rgb_mode_t;

typedef enum {
    RGB_BREATH_FAST = 0,
    RGB_BREATH_MEDIUM,
    RGB_BREATH_SLOW,
} rgb_breath_speed_t;

typedef enum {
    RGB_BACKEND_AUTO = 0,
    RGB_BACKEND_SYSFS,
    RGB_BACKEND_SERIAL,
} rgb_backend_t;

#define RGB_ZONE_L      (1u << 0)
#define RGB_ZONE_R      (1u << 1)
#define RGB_ZONE_M      (1u << 2)
#define RGB_ZONE_F1     (1u << 3)
#define RGB_ZONE_F2     (1u << 4)
#define RGB_ZONE_SINGLE (1u << 5)

#define M_BIT(mode) (1u << (mode))

#define MODES_SERIAL (M_BIT(RGB_MODE_OFF) | M_BIT(RGB_MODE_STATIC) |             \
                      M_BIT(RGB_MODE_BREATHING) | M_BIT(RGB_MODE_PRESET_COMBO) | \
                      M_BIT(RGB_MODE_THEME_SUPPLIED))

#define MODES_SYSFS  (M_BIT(RGB_MODE_OFF) | M_BIT(RGB_MODE_STATIC) |             \
                      M_BIT(RGB_MODE_BREATHING) | M_BIT(RGB_MODE_PRESET_COMBO) | \
                      M_BIT(RGB_MODE_THEME_SUPPLIED))

typedef struct {
    const char *code;
    uint32_t zones;
    rgb_backend_t backend;
    uint16_t mode_mask;
} rgb_device_caps_t;

static const rgb_device_caps_t RGB_DEVICES[] = {
        {"gcs-h36s",    RGB_ZONE_L | RGB_ZONE_R,
                RGB_BACKEND_SERIAL, MODES_SERIAL},

        {"rg40xx-h",    RGB_ZONE_L | RGB_ZONE_R,
                RGB_BACKEND_SERIAL, MODES_SERIAL},

        {"rg40xx-v",    RGB_ZONE_L | RGB_ZONE_SINGLE,
                RGB_BACKEND_SERIAL, MODES_SERIAL},

        {"rgcubexx-h",  RGB_ZONE_L | RGB_ZONE_R,
                RGB_BACKEND_SERIAL, MODES_SERIAL},

        {"rg-vita-pro", RGB_ZONE_L | RGB_ZONE_R,
                RGB_BACKEND_SERIAL, MODES_SERIAL},

        {"tui-brick",   RGB_ZONE_L | RGB_ZONE_R | RGB_ZONE_M | RGB_ZONE_F1 | RGB_ZONE_F2,
                RGB_BACKEND_SYSFS,  MODES_SYSFS},

        {"tui-spoon",   RGB_ZONE_L | RGB_ZONE_R,
                RGB_BACKEND_SYSFS,  MODES_SYSFS},
};
#define RGB_DEVICE_COUNT A_SIZE(RGB_DEVICES)

static const rgb_device_caps_t *rgb_caps_for(const char *code) {
    if (!code) return NULL;
    for (size_t i = 0; i < RGB_DEVICE_COUNT; i++) {
        if (strcmp(code, RGB_DEVICES[i].code) == 0) return &RGB_DEVICES[i];
    }
    return NULL;
}

static const rgb_device_caps_t *rgb_caps = NULL;

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    struct help_msg help_messages[] = {
#define RGB(NAME, ENUM, UDATA) { UDATA, lang.MUXRGB.HELP.ENUM },
            RGB_ELEMENTS
#undef RGB
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static const char *backend_flag(int idx) {
    switch (idx) {
        case RGB_BACKEND_SYSFS:
            return "sysfs";
        case RGB_BACKEND_SERIAL:
            return "serial";
        case RGB_BACKEND_AUTO:
        default:
            return "auto";
    }
}

static int proto_mode_for(int ui_mode, int breath_speed, int *out_preset_combo, int *out_off) {
    *out_preset_combo = 0;
    *out_off = 0;

    switch (ui_mode) {
        case RGB_MODE_OFF:
            *out_off = 1;
            return 1;
        case RGB_MODE_STATIC:
            return 1;
        case RGB_MODE_PRESET_COMBO:
            *out_preset_combo = 1;
            return 1;
        case RGB_MODE_BREATHING:
            switch (breath_speed) {
                case RGB_BREATH_FAST:
                    return 2;
                case RGB_BREATH_MEDIUM:
                    return 3;
                case RGB_BREATH_SLOW:
                    return 4;
                default:
                    return 3;
            }
        default:
            return 1;
    }
}

static int ui_brightness_to_protocol(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    return pct_to_int(pct, 0, 255);
}

static int dro_selected(lv_obj_t *dro) {
    return (int) lv_dropdown_get_selected(dro);
}

static int last_applied_mode = -1;

static void apply_mode_visibility(int ui_mode);

static int resolve_theme_rgb_script(char *out) {
    const char *base = config.THEME.STORAGE_THEME;
    if (!base || !*base) return 0;

    char active_txt_path[MAX_BUFFER_SIZE];
    snprintf(active_txt_path, sizeof active_txt_path, "%s/active.txt", base);
    if (file_exist(active_txt_path)) {
        char *alt_raw = read_line_char_from(active_txt_path, 1);
        if (alt_raw && *alt_raw) {
            char *alt = str_replace(alt_raw, "\r", "");
            if (alt && *alt) {
                snprintf(out, MAX_BUFFER_SIZE, "%s/alternate/rgb/%s/rgbconf.sh", base, alt);
                if (file_exist(out)) return 1;
            }
        }
    }

    snprintf(out, MAX_BUFFER_SIZE, "%s/rgb/rgbconf.sh", base);
    if (file_exist(out)) return 1;

    out[0] = '\0';
    return 0;
}

static void rgb_apply(void) {
    if (!rgb_caps) return;

    int ui_mode = dro_selected(ui_droMode_rgb);

    if (ui_mode != last_applied_mode) {
        apply_mode_visibility(ui_mode);
        last_applied_mode = ui_mode;
    }

    if (ui_mode == RGB_MODE_THEME_SUPPLIED) {
        char script_path[MAX_BUFFER_SIZE];
        if (!resolve_theme_rgb_script(script_path)) {
            LOG_WARN(mux_module, "Theme Supplied RGB selected but no 'rgbconf.sh' script found under '%s'", config.THEME.STORAGE_THEME);
            return;
        }

        const char *args[] = {script_path, NULL};
        run_exec(args, 1, 0, 0, NULL, NULL);
        return;
    }

    int ui_bright = dro_selected(ui_droBright_rgb);
    int ui_breath_speed = dro_selected(ui_droBreathSpeed_rgb);
    int ui_colour_l = dro_selected(ui_droColourL_rgb);
    int ui_colour_r = dro_selected(ui_droColourR_rgb);
    int ui_colour_m = dro_selected(ui_droColourM_rgb);
    int ui_colour_f1 = dro_selected(ui_droColourF1_rgb);
    int ui_colour_f2 = dro_selected(ui_droColourF2_rgb);
    int ui_combo = dro_selected(ui_droCombo_rgb);
    int ui_backend = dro_selected(ui_droBackend_rgb);

    int is_preset_combo = 0, is_off = 0;
    int proto_mode = proto_mode_for(ui_mode, ui_breath_speed, &is_preset_combo, &is_off);

    int proto_bright = is_off ? 0 : ui_brightness_to_protocol(ui_bright);

    const rgb_colour_t *col_l = rgb_colour_at(ui_colour_l);
    const rgb_colour_t *col_r = rgb_colour_or_fallback(ui_colour_r, col_l);
    const rgb_colour_t *col_m = rgb_colour_or_fallback(ui_colour_m, col_l);
    const rgb_colour_t *col_f1 = rgb_colour_or_fallback(ui_colour_f1, col_l);
    const rgb_colour_t *col_f2 = rgb_colour_or_fallback(ui_colour_f2, col_r);

    const rgb_colour_combo_t *kc = rgb_colour_combo_at(ui_combo);

    char buf[20][8];
    const char *argv[24];

    size_t n = 0;

#define ARG_INT(i, v) do { snprintf(buf[i], sizeof buf[i], "%d", (v)); argv[n++] = buf[i]; } while (0)

    argv[n++] = RGBLED_BIN;
    argv[n++] = "-b";
    argv[n++] = backend_flag(ui_backend);

    ARG_INT(0, proto_mode);
    ARG_INT(1, proto_bright);

    if (is_off) {
        for (int i = 0; i < 6; i++) ARG_INT(2 + i, 0);
    } else if (is_preset_combo) {
        ARG_INT(2, kc->a_r);
        ARG_INT(3, kc->a_g);
        ARG_INT(4, kc->a_b);
        ARG_INT(5, kc->b_r);
        ARG_INT(6, kc->b_g);
        ARG_INT(7, kc->b_b);
    } else if (rgb_caps->backend == RGB_BACKEND_SYSFS) {
        ARG_INT(2, col_l->r);
        ARG_INT(3, col_l->g);
        ARG_INT(4, col_l->b);
        ARG_INT(5, col_r->r);
        ARG_INT(6, col_r->g);
        ARG_INT(7, col_r->b);
        if (rgb_caps->zones & RGB_ZONE_M) {
            ARG_INT(8, col_m->r);
            ARG_INT(9, col_m->g);
            ARG_INT(10, col_m->b);
        }
        if (rgb_caps->zones & RGB_ZONE_F1) {
            ARG_INT(11, col_f1->r);
            ARG_INT(12, col_f1->g);
            ARG_INT(13, col_f1->b);
        }
        if (rgb_caps->zones & RGB_ZONE_F2) {
            ARG_INT(14, col_f2->r);
            ARG_INT(15, col_f2->g);
            ARG_INT(16, col_f2->b);
        }
    } else if (ui_mode == RGB_MODE_STATIC) {
        ARG_INT(2, col_r->r);
        ARG_INT(3, col_r->g);
        ARG_INT(4, col_r->b);
        ARG_INT(5, col_l->r);
        ARG_INT(6, col_l->g);
        ARG_INT(7, col_l->b);
    } else {
        ARG_INT(2, col_l->r);
        ARG_INT(3, col_l->g);
        ARG_INT(4, col_l->b);
    }

#undef ARG_INT

    run_exec(argv, n, 0, 0, NULL, NULL);
}

static void init_dropdown_settings(void) {
#define RGB(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_rgb);
    RGB_ELEMENTS
#undef RGB

    Bright_original = pct_to_int(lv_dropdown_get_selected(ui_droBright_rgb), 0, 255);
}

static void restore_rgb_options(void) {
    int saved_mode = config.SETTINGS.RGB.MODE;
    int new_mode;

    switch (saved_mode) {
        case 0:
        case 1:
        case 2:
        case 3:
            new_mode = saved_mode;
            break;
        case 4:
        case 5:
            new_mode = RGB_MODE_OFF;
            break;
        case 6:
            new_mode = RGB_MODE_THEME_SUPPLIED;
            break;
        default:
            new_mode = RGB_MODE_OFF;
            break;
    }

    lv_dropdown_set_selected(ui_droMode_rgb, new_mode);
    lv_dropdown_set_selected(ui_droBright_rgb, int_to_pct(config.SETTINGS.RGB.BRIGHT, 0, 255));
    lv_dropdown_set_selected(ui_droBreathSpeed_rgb, config.SETTINGS.RGB.BREATH_SPEED);
    lv_dropdown_set_selected(ui_droColourL_rgb, config.SETTINGS.RGB.COLOURL);
    lv_dropdown_set_selected(ui_droColourR_rgb, config.SETTINGS.RGB.COLOURR);
    lv_dropdown_set_selected(ui_droColourM_rgb, config.SETTINGS.RGB.COLOURM);
    lv_dropdown_set_selected(ui_droColourF1_rgb, config.SETTINGS.RGB.COLOURF1);
    lv_dropdown_set_selected(ui_droColourF2_rgb, config.SETTINGS.RGB.COLOURF2);
    lv_dropdown_set_selected(ui_droCombo_rgb, config.SETTINGS.RGB.COMBO);
    lv_dropdown_set_selected(ui_droBackend_rgb, config.SETTINGS.RGB.BACKEND);
}

static void save_rgb_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(rgb, Mode, "settings/rgb/mode", INT, 0);
    CHECK_AND_SAVE_PCT(rgb, Bright, "settings/rgb/bright", INT, 0, 255);
    CHECK_AND_SAVE_STD(rgb, BreathSpeed, "settings/rgb/breath_speed", INT, 0);
    CHECK_AND_SAVE_STD(rgb, ColourL, "settings/rgb/colour_l", INT, 0);
    CHECK_AND_SAVE_STD(rgb, ColourR, "settings/rgb/colour_r", INT, 0);
    CHECK_AND_SAVE_STD(rgb, ColourM, "settings/rgb/colour_m", INT, 0);
    CHECK_AND_SAVE_STD(rgb, ColourF1, "settings/rgb/colour_f1", INT, 0);
    CHECK_AND_SAVE_STD(rgb, ColourF2, "settings/rgb/colour_f2", INT, 0);
    CHECK_AND_SAVE_STD(rgb, Combo, "settings/rgb/combo", INT, 0);
    CHECK_AND_SAVE_STD(rgb, Backend, "settings/rgb/backend", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_config = 1;
    }

    rgb_apply();
}

static char **build_colour_options(int *count) {
    char **out = calloc(RGB_COLOUR_COUNT, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    for (size_t i = 0; i < RGB_COLOUR_COUNT; i++) out[i] = strdup(RGB_COLOURS[i].name);
    *count = (int) RGB_COLOUR_COUNT;

    return out;
}

static char **build_zone_options(int *count) {
    char **out = calloc(RGB_COLOUR_COUNT + 1, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    out[0] = strdup(lang.MUXRGB.SAME_AS_L);

    for (size_t i = 0; i < RGB_COLOUR_COUNT; i++) out[i + 1] = strdup(RGB_COLOURS[i].name);
    *count = (int) (RGB_COLOUR_COUNT + 1);

    return out;
}

static char **build_combo_options(int *count) {
    char **out = calloc(RGB_COLOUR_COMBO_COUNT, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    for (size_t i = 0; i < RGB_COLOUR_COMBO_COUNT; i++) out[i] = strdup(RGB_COLOUR_COMBOS[i].name);
    *count = (int) RGB_COLOUR_COMBO_COUNT;

    return out;
}

static void free_string_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) free(arr[i]);
    free(arr);
}

static void apply_mode_visibility(int ui_mode) {
    int single = rgb_caps && (rgb_caps->zones & RGB_ZONE_SINGLE);
    int is_sysfs = rgb_caps && rgb_caps->backend == RGB_BACKEND_SYSFS;

    int caps_colour_r = rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_R) && (rgb_caps->zones & RGB_ZONE_M);
    int caps_colour_m = rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_M);
    int caps_colour_f1 = rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_F1);
    int caps_colour_f2 = rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_F2);
    int caps_combo = rgb_caps && (rgb_caps->mode_mask & M_BIT(RGB_MODE_PRESET_COMBO));
    int caps_breathing = rgb_caps && (rgb_caps->mode_mask & M_BIT(RGB_MODE_BREATHING));

    int show_bright = 0;
    int show_breath_speed = 0;
    int show_colour_l = 0;
    int show_colour_r = 0;
    int show_colour_m = 0;
    int show_colour_f1 = 0;
    int show_colour_f2 = 0;
    int show_combo = 0;
    int show_backend = 0;

    switch (ui_mode) {
        case RGB_MODE_OFF:
            break;
        case RGB_MODE_STATIC:
            show_bright = 1;
            show_colour_l = 1;
            show_colour_r = caps_colour_r;
            show_colour_m = caps_colour_m;
            show_colour_f1 = caps_colour_f1;
            show_colour_f2 = caps_colour_f2;
            show_backend = 1;
            break;
        case RGB_MODE_BREATHING:
            show_bright = 1;
            show_breath_speed = caps_breathing;
            show_colour_l = 1;
            show_colour_r = is_sysfs && caps_colour_r;
            show_colour_m = is_sysfs && caps_colour_m;
            show_colour_f1 = is_sysfs && caps_colour_f1;
            show_colour_f2 = is_sysfs && caps_colour_f2;
            show_backend = 1;
            break;
        case RGB_MODE_PRESET_COMBO:
            show_bright = 1;
            show_combo = caps_combo;
            show_backend = 1;
            break;
        case RGB_MODE_THEME_SUPPLIED:
        default:
            show_backend = 1;
            break;
    }

    if (show_bright) SHOW_OPTION_ITEM(rgb, Bright); else HIDE_OPTION_ITEM(rgb, Bright);
    if (show_breath_speed) SHOW_OPTION_ITEM(rgb, BreathSpeed); else HIDE_OPTION_ITEM(rgb, BreathSpeed);
    if (show_colour_l) SHOW_OPTION_ITEM(rgb, ColourL); else HIDE_OPTION_ITEM(rgb, ColourL);
    if (show_colour_r) SHOW_OPTION_ITEM(rgb, ColourR); else HIDE_OPTION_ITEM(rgb, ColourR);
    if (show_colour_m) SHOW_OPTION_ITEM(rgb, ColourM); else HIDE_OPTION_ITEM(rgb, ColourM);
    if (show_colour_f1) SHOW_OPTION_ITEM(rgb, ColourF1); else HIDE_OPTION_ITEM(rgb, ColourF1);
    if (show_colour_f2) SHOW_OPTION_ITEM(rgb, ColourF2); else HIDE_OPTION_ITEM(rgb, ColourF2);
    if (show_combo) SHOW_OPTION_ITEM(rgb, Combo); else HIDE_OPTION_ITEM(rgb, Combo);
    if (show_backend) SHOW_OPTION_ITEM(rgb, Backend); else HIDE_OPTION_ITEM(rgb, Backend);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *mode_options[] = {
            lang.MUXRGB.MODE_NAME.OFF,
            lang.MUXRGB.MODE_NAME.STATIC,
            lang.MUXRGB.MODE_NAME.BREATHING,
            lang.MUXRGB.MODE_NAME.PRESET_COMBO,
            lang.MUXRGB.MODE_NAME.THEME_SUPPLIED,
    };

    char *breath_speed_options[] = {
            lang.MUXRGB.BREATH_SPEED_NAME.FAST,
            lang.MUXRGB.BREATH_SPEED_NAME.MEDIUM,
            lang.MUXRGB.BREATH_SPEED_NAME.SLOW,
    };

    char *backend_options[] = {
            lang.MUXRGB.BACKEND_NAME.AUTO,
            lang.MUXRGB.BACKEND_NAME.SYSFS,
            lang.MUXRGB.BACKEND_NAME.SERIAL,
    };

    int colour_count = 0;
    int zone_count = 0;
    int combo_count = 0;

    char **colour_options = build_colour_options(&colour_count);
    char **zone_options = build_zone_options(&zone_count);
    char **combo_options = build_combo_options(&combo_count);

    INIT_OPTION_ITEM(-1, rgb, Mode, lang.MUXRGB.MODE, "mode", mode_options, 5);
    INIT_OPTION_ITEM(-1, rgb, Bright, lang.MUXRGB.BRIGHT, "bright", NULL, 0);
    INIT_OPTION_ITEM(-1, rgb, BreathSpeed, lang.MUXRGB.BREATH_SPEED, "breath_speed", breath_speed_options, 3);
    INIT_OPTION_ITEM(-1, rgb, ColourL, lang.MUXRGB.COLOURL, "colour_l", colour_options, colour_count);
    INIT_OPTION_ITEM(-1, rgb, ColourR, lang.MUXRGB.COLOURR, "colour_r", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, ColourM, lang.MUXRGB.COLOURM, "colour_m", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, ColourF1, lang.MUXRGB.COLOURF1, "colour_f1", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, ColourF2, lang.MUXRGB.COLOURF2, "colour_f2", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, Combo, lang.MUXRGB.COMBO, "combo", combo_options, combo_count);
    INIT_OPTION_ITEM(-1, rgb, Backend, lang.MUXRGB.BACKEND, "backend", backend_options, 3);

    char *bright_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droBright_rgb, bright_values);
    free(bright_values);

    free_string_array(colour_options, colour_count);
    free_string_array(zone_options, zone_count);
    free_string_array(combo_options, combo_count);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!rgb_caps) LOG_WARN(mux_module, "No caps entry for board '%s'; showing all rows", device.BOARD.NAME);

    int initial_mode = (int) lv_dropdown_get_selected(ui_droMode_rgb);
    apply_mode_visibility(initial_mode);
    last_applied_mode = initial_mode;

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
    rgb_apply();
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
    rgb_apply();
}

static void handle_a(void) {
    if (msgbox_active || block_input || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (block_input || hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    save_rgb_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "rgb");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || block_input || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

#define RGB(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_rgb, UDATA);
    RGB_ELEMENTS
#undef RGB

    overlay_display();
}

int muxrgb_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    rgb_caps = rgb_caps_for(device.BOARD.NAME);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXRGB.TITLE);
    init_muxrgb(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_rgb_options();
    init_dropdown_settings();

    rgb_apply();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
