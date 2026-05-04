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

#define MODES_JOYPAD (M_BIT(RGB_MODE_OFF) | M_BIT(RGB_MODE_STATIC) |              \
                      M_BIT(RGB_MODE_BREATHING) |                                 \
                      M_BIT(RGB_MODE_THEME_SUPPLIED) |                            \
                      M_BIT(RGB_MODE_COLOUR_CYCLE) | M_BIT(RGB_MODE_RAINBOW) |    \
                      M_BIT(RGB_MODE_STICK_FOLLOW))

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

        {"rg-vita-pro", RGB_ZONE_L | RGB_ZONE_SINGLE,
                RGB_BACKEND_JOYPAD, MODES_JOYPAD},

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

#define MODE_TABLE_MAX 16

static int mode_dropdown_to_enum[MODE_TABLE_MAX];
static int mode_enum_to_dropdown[MODE_TABLE_MAX];
static int mode_dropdown_count = 0;

static int mode_enum_from_dropdown(int dropdown_idx);

static int mode_dropdown_from_enum(int enum_val);

static int colourl_palette_idx(void);

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
        case RGB_BACKEND_JOYPAD:
            return "joypad";
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
        case RGB_MODE_RAINBOW:
            return 5;
        case RGB_MODE_STICK_FOLLOW:
            return 6;
        case RGB_MODE_COLOUR_CYCLE:
            return 7;
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

static int current_mode_enum(void) {
    int selected = (int) lv_dropdown_get_selected(ui_droMode_rgb);

    return mode_enum_from_dropdown(selected);
}

static int effective_backend_from_saved(int saved) {
    if (rgb_caps && rgb_caps->backend != RGB_BACKEND_AUTO) return rgb_caps->backend;

    switch (saved) {
        case RGB_BACKEND_SYSFS:
        case RGB_BACKEND_SERIAL:
        case RGB_BACKEND_JOYPAD:
            return saved;
        case RGB_BACKEND_AUTO:
        default:
            return RGB_BACKEND_AUTO;
    }
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

static void rgb_focus(void) {
    if (!rgb_caps) return;

    int ui_mode = current_mode_enum();
    if (ui_mode != last_applied_mode) {
        apply_mode_visibility(ui_mode);
        last_applied_mode = ui_mode;
    }
}

typedef struct {
    char buf[24][12];
    const char *argv[32];
    size_t n;
} rgb_argv_t;

static void rgb_argv_init(rgb_argv_t *a, int backend, int proto_mode, int proto_bright) {
    a->n = 0;

    a->argv[a->n++] = RGBLED_BIN;
    a->argv[a->n++] = "-b";
    a->argv[a->n++] = backend_flag(backend);

    snprintf(a->buf[0], sizeof a->buf[0], "%d", proto_mode);
    a->argv[a->n++] = a->buf[0];

    snprintf(a->buf[1], sizeof a->buf[1], "%d", proto_bright);
    a->argv[a->n++] = a->buf[1];
}

static void rgb_argv_int(rgb_argv_t *a, int slot, int value) {
    if (slot < 0 || slot >= (int) A_SIZE(a->buf)) return;
    if (a->n >= A_SIZE(a->argv)) return;

    snprintf(a->buf[slot], sizeof a->buf[slot], "%d", value);
    a->argv[a->n++] = a->buf[slot];
}

static int rgb_speed_pct_from_dropdown(int joypad, int value) {
    if (joypad) {
        if (value < 0) return 0;
        if (value > 100) return 100;

        return value;
    }

    switch (value) {
        case RGB_BREATH_FAST:
            return 80;
        case RGB_BREATH_MEDIUM:
            return 50;
        case RGB_BREATH_SLOW:
            return 20;
        default:
            return 50;
    }
}

static void rgb_push_off(rgb_argv_t *a) {
    for (int i = 0; i < 6; i++) {
        rgb_argv_int(a, 2 + i, 0);
    }
}

static void rgb_push_joypad_args(rgb_argv_t *a, int ui_mode, int ui_breath_speed, int is_off, int is_preset_combo, int ui_colour_r,
                                 const rgb_colour_t *col_l, const rgb_colour_t *col_r, const rgb_colour_combo_t *kc) {
    if (is_off) {
        rgb_push_off(a);
        return;
    }

    if (ui_mode == RGB_MODE_STICK_FOLLOW) {
        rgb_argv_int(a, 2, col_l->r);
        rgb_argv_int(a, 3, col_l->g);
        rgb_argv_int(a, 4, col_l->b);
        return;
    }

    if (ui_mode == RGB_MODE_RAINBOW || ui_mode == RGB_MODE_COLOUR_CYCLE) {
        rgb_argv_int(a, 2, rgb_speed_pct_from_dropdown(1, ui_breath_speed));
        return;
    }

    if (ui_mode == RGB_MODE_BREATHING) {
        int sec_r = 0;
        int sec_g = 0;
        int sec_b = 0;

        if (ui_colour_r > 0) {
            sec_r = col_r->r;
            sec_g = col_r->g;
            sec_b = col_r->b;
        }

        rgb_argv_int(a, 2, rgb_speed_pct_from_dropdown(1, ui_breath_speed));
        rgb_argv_int(a, 3, col_l->r);
        rgb_argv_int(a, 4, col_l->g);
        rgb_argv_int(a, 5, col_l->b);
        rgb_argv_int(a, 6, sec_r);
        rgb_argv_int(a, 7, sec_g);
        rgb_argv_int(a, 8, sec_b);
        return;
    }

    if (is_preset_combo) {
        rgb_argv_int(a, 2, kc->a_r);
        rgb_argv_int(a, 3, kc->a_g);
        rgb_argv_int(a, 4, kc->a_b);
        rgb_argv_int(a, 5, kc->b_r);
        rgb_argv_int(a, 6, kc->b_g);
        rgb_argv_int(a, 7, kc->b_b);
        return;
    }

    rgb_argv_int(a, 2, col_l->r);
    rgb_argv_int(a, 3, col_l->g);
    rgb_argv_int(a, 4, col_l->b);
    rgb_argv_int(a, 5, col_r->r);
    rgb_argv_int(a, 6, col_r->g);
    rgb_argv_int(a, 7, col_r->b);
}

static void rgb_push_sysfs_args(rgb_argv_t *a, int ui_mode, int ui_breath_speed, int is_off, int is_preset_combo,
                                const rgb_colour_t *col_l, const rgb_colour_t *col_r, const rgb_colour_t *col_m,
                                const rgb_colour_t *col_f1, const rgb_colour_t *col_f2, const rgb_colour_combo_t *kc) {
    if (is_off) {
        rgb_push_off(a);
        return;
    }

    if (ui_mode == RGB_MODE_RAINBOW || ui_mode == RGB_MODE_COLOUR_CYCLE) {
        rgb_argv_int(a, 2, rgb_speed_pct_from_dropdown(0, ui_breath_speed));
        return;
    }

    if (is_preset_combo) {
        rgb_argv_int(a, 2, kc->a_r);
        rgb_argv_int(a, 3, kc->a_g);
        rgb_argv_int(a, 4, kc->a_b);
        rgb_argv_int(a, 5, kc->b_r);
        rgb_argv_int(a, 6, kc->b_g);
        rgb_argv_int(a, 7, kc->b_b);
        return;
    }

    rgb_argv_int(a, 2, col_l->r);
    rgb_argv_int(a, 3, col_l->g);
    rgb_argv_int(a, 4, col_l->b);
    rgb_argv_int(a, 5, col_r->r);
    rgb_argv_int(a, 6, col_r->g);
    rgb_argv_int(a, 7, col_r->b);

    if (rgb_caps->zones & RGB_ZONE_M) {
        rgb_argv_int(a, 8, col_m->r);
        rgb_argv_int(a, 9, col_m->g);
        rgb_argv_int(a, 10, col_m->b);
    }

    if (rgb_caps->zones & RGB_ZONE_F1) {
        rgb_argv_int(a, 11, col_f1->r);
        rgb_argv_int(a, 12, col_f1->g);
        rgb_argv_int(a, 13, col_f1->b);
    }

    if (rgb_caps->zones & RGB_ZONE_F2) {
        rgb_argv_int(a, 14, col_f2->r);
        rgb_argv_int(a, 15, col_f2->g);
        rgb_argv_int(a, 16, col_f2->b);
    }
}

static void rgb_push_serial_args(rgb_argv_t *a, int ui_mode, int ui_breath_speed, int is_off, int is_preset_combo,
                                 const rgb_colour_t *col_l, const rgb_colour_t *col_r, const rgb_colour_combo_t *kc) {
    if (is_off) {
        rgb_push_off(a);
        return;
    }

    if (ui_mode == RGB_MODE_RAINBOW || ui_mode == RGB_MODE_COLOUR_CYCLE) {
        rgb_argv_int(a, 2, rgb_speed_pct_from_dropdown(0, ui_breath_speed));
        return;
    }

    if (is_preset_combo) {
        rgb_argv_int(a, 2, kc->a_r);
        rgb_argv_int(a, 3, kc->a_g);
        rgb_argv_int(a, 4, kc->a_b);
        rgb_argv_int(a, 5, kc->b_r);
        rgb_argv_int(a, 6, kc->b_g);
        rgb_argv_int(a, 7, kc->b_b);
        return;
    }

    if (ui_mode == RGB_MODE_STATIC) {
        rgb_argv_int(a, 2, col_r->r);
        rgb_argv_int(a, 3, col_r->g);
        rgb_argv_int(a, 4, col_r->b);
        rgb_argv_int(a, 5, col_l->r);
        rgb_argv_int(a, 6, col_l->g);
        rgb_argv_int(a, 7, col_l->b);
        return;
    }

    rgb_argv_int(a, 2, col_l->r);
    rgb_argv_int(a, 3, col_l->g);
    rgb_argv_int(a, 4, col_l->b);
}

static int rgb_protocol_mode_for_backend(int backend, int ui_mode, int breath_speed, int *is_preset_combo, int *is_off) {
    int proto_mode = proto_mode_for(ui_mode, breath_speed, is_preset_combo, is_off);
    if (backend == RGB_BACKEND_JOYPAD && ui_mode == RGB_MODE_BREATHING) return 8;

    return proto_mode;
}

static void rgb_apply(void) {
    const char *argv[2];
    argv[0] = RGBLED_BIN;
    argv[1] = "restore";
    run_exec(argv, 2, 0, 0, NULL, NULL);
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
        case 4:
            new_mode = saved_mode;
            break;
        case 5:
            new_mode = RGB_MODE_OFF;
            break;
        case 6:
            new_mode = RGB_MODE_THEME_SUPPLIED;
            break;
        case 7:
            new_mode = RGB_MODE_COLOUR_CYCLE;
            break;
        case 8:
            new_mode = RGB_MODE_RAINBOW;
            break;
        case 9:
            new_mode = RGB_MODE_STICK_FOLLOW;
            break;
        default:
            new_mode = RGB_MODE_OFF;
            break;
    }

    if (rgb_caps && !(rgb_caps->mode_mask & M_BIT(new_mode))) new_mode = RGB_MODE_OFF;

    lv_dropdown_set_selected(ui_droMode_rgb, mode_dropdown_from_enum(new_mode));
    lv_dropdown_set_selected(ui_droBright_rgb, int_to_pct(config.SETTINGS.RGB.BRIGHT, 0, 255));

    {
        int saved_bs = config.SETTINGS.RGB.BREATH_SPEED;
        int joypad = rgb_caps && rgb_caps->backend == RGB_BACKEND_JOYPAD;
        if (joypad) {
            if (saved_bs >= 0 && saved_bs <= 2) {
                int fms[] = {80, 50, 20};
                saved_bs = fms[saved_bs];
            }
            if (saved_bs < 0) saved_bs = 0;
            if (saved_bs > 100) saved_bs = 100;
        }
        lv_dropdown_set_selected(ui_droBreathSpeed_rgb, saved_bs);
    }

    lv_dropdown_set_selected(ui_droColourL_rgb, config.SETTINGS.RGB.COLOURL);
    lv_dropdown_set_selected(ui_droColourR_rgb, config.SETTINGS.RGB.COLOURR);
    lv_dropdown_set_selected(ui_droColourM_rgb, config.SETTINGS.RGB.COLOURM);
    lv_dropdown_set_selected(ui_droColourF1_rgb, config.SETTINGS.RGB.COLOURF1);
    lv_dropdown_set_selected(ui_droColourF2_rgb, config.SETTINGS.RGB.COLOURF2);
    lv_dropdown_set_selected(ui_droCombo_rgb, config.SETTINGS.RGB.COMBO);
    lv_dropdown_set_selected(ui_droBackend_rgb, effective_backend_from_saved(config.SETTINGS.RGB.BACKEND));
}

static void save_rgb_options(void) {
    int is_modified = 0;

    {
        int current_idx = (int) lv_dropdown_get_selected(ui_droMode_rgb);
        if (current_idx != Mode_original) {
            int current_enum = mode_enum_from_dropdown(current_idx);
            write_text_to_file("/opt/muos/config/settings/rgb/mode", "w", INT, current_enum);
            is_modified++;
        }
    }

    CHECK_AND_SAVE_PCT(rgb, Bright, "settings/rgb/bright", INT, 0, 255);
    CHECK_AND_SAVE_STD(rgb, BreathSpeed, "settings/rgb/breath_speed", INT, 0);

    {
        int current_palette_idx = colourl_palette_idx();
        if (current_palette_idx != ColourL_original) {
            write_text_to_file("/opt/muos/config/settings/rgb/colour_l", "w", INT, current_palette_idx);
            is_modified++;
        }
    }

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

// This is something unique where the stick follow, at least for the
// rg-vita-pro device only has "8" distinct stick follow colours and
// no other custom colours work, "0" or "Off" is the last one!
static const int STICK_FOLLOW_PALETTE_IDX[] = {
        1,   // Red       (255,   0,   0)
        16,  // Yellow    (255, 255,   0)
        19,  // Green     (  0, 255,   0)
        26,  // Neon Cyan (  0, 255, 255)
        33,  // True Blue (  0,   0, 255)
        40,  // Magenta   (255,   0, 255)
        51,  // White     (255, 255, 255)
};
#define STICK_FOLLOW_PALETTE_COUNT (sizeof(STICK_FOLLOW_PALETTE_IDX) / sizeof(STICK_FOLLOW_PALETTE_IDX[0]))

static int stick_palette_idx_from_dropdown(int dropdown_idx) {
    if (dropdown_idx < 0 || dropdown_idx >= (int) STICK_FOLLOW_PALETTE_COUNT) return -1;

    return STICK_FOLLOW_PALETTE_IDX[dropdown_idx];
}

static int stick_dropdown_idx_from_palette(int palette_idx) {
    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        if (STICK_FOLLOW_PALETTE_IDX[i] == palette_idx) return (int) i;
    }

    return 0;
}

static char *build_stick_palette_options_string(void) {
    size_t total = 1;
    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        int idx = STICK_FOLLOW_PALETTE_IDX[i];
        if (idx < 0 || idx >= (int) RGB_COLOUR_COUNT) continue;
        total += strlen(RGB_COLOURS[idx].name) + 1;
    }

    char *buf = calloc(total, 1);
    if (!buf) return NULL;

    char *p = buf;
    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        int idx = STICK_FOLLOW_PALETTE_IDX[i];
        if (idx < 0 || idx >= (int) RGB_COLOUR_COUNT) continue;
        const char *name = RGB_COLOURS[idx].name;
        if (i > 0) *p++ = '\n';
        size_t n = strlen(name);
        memcpy(p, name, n);
        p += n;
    }

    *p = '\0';

    return buf;
}

static char *build_full_palette_options_string(void) {
    size_t total = 1;
    for (size_t i = 0; i < RGB_COLOUR_COUNT; i++) total += strlen(RGB_COLOURS[i].name) + 1;

    char *buf = calloc(total, 1);
    if (!buf) return NULL;

    char *p = buf;
    for (size_t i = 0; i < RGB_COLOUR_COUNT; i++) {
        if (i > 0) *p++ = '\n';
        size_t n = strlen(RGB_COLOURS[i].name);
        memcpy(p, RGB_COLOURS[i].name, n);
        p += n;
    }

    *p = '\0';

    return buf;
}

typedef enum {
    COLOURL_DROPDOWN_FULL = 0,
    COLOURL_DROPDOWN_STICK = 1,
} colourl_dropdown_set_t;

static colourl_dropdown_set_t colourl_dropdown_state = COLOURL_DROPDOWN_FULL;

static void colourl_set_dropdown_set(colourl_dropdown_set_t target) {
    if (target == colourl_dropdown_state) return;

    int current_dropdown_idx = (int) lv_dropdown_get_selected(ui_droColourL_rgb);
    int current_palette_idx;
    if (colourl_dropdown_state == COLOURL_DROPDOWN_STICK) {
        current_palette_idx = stick_palette_idx_from_dropdown(current_dropdown_idx);
        if (current_palette_idx < 0) current_palette_idx = STICK_FOLLOW_PALETTE_IDX[0];
    } else {
        current_palette_idx = current_dropdown_idx;
    }

    char *opts;
    int new_dropdown_idx;
    if (target == COLOURL_DROPDOWN_STICK) {
        opts = build_stick_palette_options_string();
        new_dropdown_idx = stick_dropdown_idx_from_palette(current_palette_idx);
    } else {
        opts = build_full_palette_options_string();
        new_dropdown_idx = current_palette_idx;
        if (new_dropdown_idx < 0 || new_dropdown_idx >= (int) RGB_COLOUR_COUNT) new_dropdown_idx = 0;
    }

    if (opts) {
        lv_dropdown_set_options(ui_droColourL_rgb, opts);
        free(opts);
    }

    lv_dropdown_set_selected(ui_droColourL_rgb, new_dropdown_idx);
    colourl_dropdown_state = target;
}

static int colourl_palette_idx(void) {
    int sel = (int) lv_dropdown_get_selected(ui_droColourL_rgb);

    if (colourl_dropdown_state == COLOURL_DROPDOWN_STICK) {
        int p = stick_palette_idx_from_dropdown(sel);
        return p < 0 ? STICK_FOLLOW_PALETTE_IDX[0] : p;
    }

    if (sel < 0 || sel >= (int) RGB_COLOUR_COUNT) return 0;
    return sel;
}

static char **build_mode_options(int *count) {
    static const struct {
        int mode_enum;
        int gated;
        const char *fallback_name;
    } slots[] = {
            {RGB_MODE_OFF,            0, lang.MUXRGB.MODE_NAME.OFF},
            {RGB_MODE_STATIC,         0, lang.MUXRGB.MODE_NAME.STATIC},
            {RGB_MODE_BREATHING,      0, lang.MUXRGB.MODE_NAME.BREATHING},
            {RGB_MODE_COLOUR_CYCLE,   1, lang.MUXRGB.MODE_NAME.COLOUR_CYCLE},
            {RGB_MODE_RAINBOW,        1, lang.MUXRGB.MODE_NAME.RAINBOW},
            {RGB_MODE_STICK_FOLLOW,   1, lang.MUXRGB.MODE_NAME.STICK_FOLLOW},
            {RGB_MODE_PRESET_COMBO,   1, lang.MUXRGB.MODE_NAME.PRESET_COMBO},
            {RGB_MODE_THEME_SUPPLIED, 0, lang.MUXRGB.MODE_NAME.THEME_SUPPLIED},
    };
    const size_t slot_count = sizeof(slots) / sizeof(slots[0]);

    char **out = calloc(slot_count, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < MODE_TABLE_MAX; i++) {
        mode_dropdown_to_enum[i] = -1;
        mode_enum_to_dropdown[i] = -1;
    }

    int n = 0;
    for (size_t i = 0; i < slot_count; i++) {
        int e = slots[i].mode_enum;
        int show = !slots[i].gated || (rgb_caps && (rgb_caps->mode_mask & M_BIT(e)));
        if (!show) continue;

        const char *label = NULL;
        switch (e) {
            case RGB_MODE_OFF:
                label = lang.MUXRGB.MODE_NAME.OFF;
                break;
            case RGB_MODE_STATIC:
                label = lang.MUXRGB.MODE_NAME.STATIC;
                break;
            case RGB_MODE_BREATHING:
                label = lang.MUXRGB.MODE_NAME.BREATHING;
                break;
            case RGB_MODE_PRESET_COMBO:
                label = lang.MUXRGB.MODE_NAME.PRESET_COMBO;
                break;
            case RGB_MODE_THEME_SUPPLIED:
                label = lang.MUXRGB.MODE_NAME.THEME_SUPPLIED;
                break;
            default:
                label = slots[i].fallback_name;
                break;
        }

        out[n] = strdup(label ? label : slots[i].fallback_name);
        if (e >= 0 && e < MODE_TABLE_MAX) {
            mode_dropdown_to_enum[n] = e;
            mode_enum_to_dropdown[e] = n;
        }

        n++;
    }

    mode_dropdown_count = n;
    *count = n;

    return out;
}

static int mode_enum_from_dropdown(int dropdown_idx) {
    static const int fallback[] = {
            RGB_MODE_OFF,
            RGB_MODE_STATIC,
            RGB_MODE_BREATHING,
            RGB_MODE_COLOUR_CYCLE,
            RGB_MODE_RAINBOW,
            RGB_MODE_STICK_FOLLOW,
            RGB_MODE_PRESET_COMBO,
            RGB_MODE_THEME_SUPPLIED,
    };

    if (dropdown_idx < 0) return RGB_MODE_OFF;

    if (mode_dropdown_count > 0 && dropdown_idx < mode_dropdown_count) {
        int e = mode_dropdown_to_enum[dropdown_idx];
        if (e >= 0) return e;
    }

    if (dropdown_idx < (int) A_SIZE(fallback)) return fallback[dropdown_idx];

    return RGB_MODE_OFF;
}

static int mode_dropdown_from_enum(int enum_val) {
    if (enum_val >= 0 && enum_val < MODE_TABLE_MAX && mode_dropdown_count > 0) {
        int idx = mode_enum_to_dropdown[enum_val];
        if (idx >= 0) {
            return idx;
        }
    }

    switch (enum_val) {
        case RGB_MODE_OFF:
            return 0;
        case RGB_MODE_STATIC:
            return 1;
        case RGB_MODE_BREATHING:
            return 2;
        case RGB_MODE_COLOUR_CYCLE:
            return 3;
        case RGB_MODE_RAINBOW:
            return 4;
        case RGB_MODE_STICK_FOLLOW:
            return 5;
        case RGB_MODE_PRESET_COMBO:
            return 6;
        case RGB_MODE_THEME_SUPPLIED:
            return 7;
        default:
            return 0;
    }
}

static void free_string_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) free(arr[i]);
    free(arr);
}

static void apply_mode_visibility(int ui_mode) {
    int single = rgb_caps && (rgb_caps->zones & RGB_ZONE_SINGLE);
    int is_sysfs = rgb_caps && rgb_caps->backend == RGB_BACKEND_SYSFS;
    int is_joypad = rgb_caps && rgb_caps->backend == RGB_BACKEND_JOYPAD;

    int caps_colour_r = is_sysfs && rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_R);
    int caps_colour_m = is_sysfs && rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_M);
    int caps_colour_f1 = is_sysfs && rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_F1);
    int caps_colour_f2 = is_sysfs && rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_F2);
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
            show_colour_r = (is_sysfs && caps_colour_r) || is_joypad;
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
        case RGB_MODE_COLOUR_CYCLE:
            show_bright = 1;
            show_breath_speed = 1;
            show_backend = 1;
            break;
        case RGB_MODE_RAINBOW: {
            int rainbow_is_joypad = rgb_caps && rgb_caps->backend == RGB_BACKEND_JOYPAD;
            show_bright = 1;
            show_breath_speed = rainbow_is_joypad;
            show_backend = 1;
            break;
        }
        case RGB_MODE_STICK_FOLLOW:
            show_bright = 1;
            show_colour_l = is_joypad;
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

    if (is_joypad) {
        if (ui_mode == RGB_MODE_BREATHING) {
            lv_label_set_text(ui_lblColourL_rgb, lang.MUXRGB.STICK.PRIMARY);
            lv_label_set_text(ui_lblColourR_rgb, lang.MUXRGB.STICK.SECONDARY);
        } else if (ui_mode == RGB_MODE_STICK_FOLLOW) {
            lv_label_set_text(ui_lblColourL_rgb, lang.MUXRGB.STICK.SINGLE);
            lv_label_set_text(ui_lblColourR_rgb, lang.MUXRGB.STICK.SINGLE);
        } else {
            lv_label_set_text(ui_lblColourL_rgb, lang.MUXRGB.STICK.BOTH);
            lv_label_set_text(ui_lblColourR_rgb, lang.MUXRGB.STICK.BOTH);
        }

        colourl_set_dropdown_set(ui_mode == RGB_MODE_STICK_FOLLOW ? COLOURL_DROPDOWN_STICK : COLOURL_DROPDOWN_FULL);
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *breath_speed_options[] = {
            lang.MUXRGB.BREATH_SPEED_NAME.FAST,
            lang.MUXRGB.BREATH_SPEED_NAME.MEDIUM,
            lang.MUXRGB.BREATH_SPEED_NAME.SLOW,
    };

    int joypad_speed_dropdown = rgb_caps && rgb_caps->backend == RGB_BACKEND_JOYPAD;

    char *backend_options[] = {
            lang.MUXRGB.BACKEND_NAME.AUTO,
            lang.MUXRGB.BACKEND_NAME.SYSFS,
            lang.MUXRGB.BACKEND_NAME.SERIAL,
            lang.MUXRGB.BACKEND_NAME.JOYPAD,
    };

    int mode_count = 0;
    char **mode_options = build_mode_options(&mode_count);

    int colour_count = 0;
    int zone_count = 0;
    int combo_count = 0;

    char **colour_options = build_colour_options(&colour_count);
    char **zone_options = build_zone_options(&zone_count);
    char **combo_options = build_combo_options(&combo_count);

    INIT_OPTION_ITEM(-1, rgb, Mode, lang.MUXRGB.MODE, "mode", mode_options, mode_count);
    INIT_OPTION_ITEM(-1, rgb, Bright, lang.MUXRGB.BRIGHT, "bright", NULL, 0);
    if (joypad_speed_dropdown) {
        INIT_OPTION_ITEM(-1, rgb, BreathSpeed, lang.MUXRGB.BREATH_SPEED, "breath_speed", NULL, 0);
    } else {
        INIT_OPTION_ITEM(-1, rgb, BreathSpeed, lang.MUXRGB.BREATH_SPEED, "breath_speed", breath_speed_options, 3);
    }
    INIT_OPTION_ITEM(-1, rgb, ColourL, lang.MUXRGB.COLOURL, "colour_l", colour_options, colour_count);
    INIT_OPTION_ITEM(-1, rgb, ColourR, lang.MUXRGB.COLOURR, "colour_r", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, ColourM, lang.MUXRGB.COLOURM, "colour_m", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, ColourF1, lang.MUXRGB.COLOURF1, "colour_f1", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, ColourF2, lang.MUXRGB.COLOURF2, "colour_f2", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, Combo, lang.MUXRGB.COMBO, "combo", combo_options, combo_count);
    INIT_OPTION_ITEM(-1, rgb, Backend, lang.MUXRGB.BACKEND, "backend", backend_options, 4);

    char *bright_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droBright_rgb, bright_values);
    free(bright_values);

    if (joypad_speed_dropdown) {
        char *speed_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
        apply_theme_list_drop_down(&theme, ui_droBreathSpeed_rgb, speed_values);
        free(speed_values);
    }

    free_string_array(mode_options, mode_count);
    free_string_array(colour_options, colour_count);
    free_string_array(zone_options, zone_count);
    free_string_array(combo_options, combo_count);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!rgb_caps) LOG_WARN(mux_module, "No caps entry for board '%s'; showing all rows", device.BOARD.NAME);

    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        int idx = STICK_FOLLOW_PALETTE_IDX[i];
        if (idx < 0 || idx >= (int) RGB_COLOUR_COUNT) {
            LOG_WARN(mux_module, "STICK_FOLLOW_PALETTE_IDX[%zu]=%d is out of range (palette size %zu)", i, idx, (size_t) RGB_COLOUR_COUNT);
        }
    }

    if (rgb_caps && rgb_caps->backend == RGB_BACKEND_JOYPAD) {
        lv_label_set_text(ui_lblBreathSpeed_rgb, lang.MUXRGB.SPEED);
    }

    int initial_mode = current_mode_enum();
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
    rgb_focus();
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
    rgb_focus();
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

static void handle_x(void) {
    if (msgbox_active || block_input || hold_call) return;

    rgb_apply();
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
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.SET,    0},
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

    rgb_focus();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
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
