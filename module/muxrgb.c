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

static int colour_palette_idx(void);


static void show_help(void) {
    struct help_msg help_messages[] = {
#define RGB(NAME, ENUM, UDATA) { UDATA, lang.MUXRGB.HELP.ENUM },
            RGB_ELEMENTS
#undef RGB
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
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

static void rgb_focus(void) {
    if (!rgb_caps) return;

    int ui_mode = current_mode_enum();
    if (ui_mode != last_applied_mode) {
        apply_mode_visibility(ui_mode);
        last_applied_mode = ui_mode;
    }
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

static void save_rgb_options(int toast_vis) {
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
        int current_palette_idx = colour_palette_idx();
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
        toast_message(lang.GENERIC.SAVING, toast_vis);
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

static int colour_palette_idx(void) {
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
    int is_serial = rgb_caps && rgb_caps->backend == RGB_BACKEND_SERIAL;
    int is_joypad = rgb_caps && rgb_caps->backend == RGB_BACKEND_JOYPAD;

    int caps_colour_r = (is_sysfs || is_serial) && rgb_caps && !single && (rgb_caps->zones & RGB_ZONE_R);
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

    gen_step_movement(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1, 2, 0, 1);
}


static int save_mode = 0;
static mux_dialogue save_dlg;

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;
    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
}

static int any_rgb_modified(void) {
    if ((int) lv_dropdown_get_selected(ui_droMode_rgb) != Mode_original) return 1;
    if (pct_to_int(lv_dropdown_get_selected(ui_droBright_rgb), 0, 255) != Bright_original) return 1;

    if ((int) lv_dropdown_get_selected(ui_droBreathSpeed_rgb) != BreathSpeed_original) return 1;
    if (colour_palette_idx() != ColourL_original) return 1;

    if ((int) lv_dropdown_get_selected(ui_droColourR_rgb) != ColourR_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droColourM_rgb) != ColourM_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droColourF1_rgb) != ColourF1_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droColourF2_rgb) != ColourF2_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droCombo_rgb) != Combo_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droBackend_rgb) != Backend_original) return 1;

    return 0;
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
    rgb_focus();
}

static void handle_option_next(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
    rgb_focus();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (save_mode) {
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == MUX_UNSAVED_SAVE) save_rgb_options(FOREVER);

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "rgb");

        mux_input_stop();
        return;
    }

    if (msgbox_active || block_input || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (block_input || hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_rgb_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);
    save_rgb_options(FOREVER);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "rgb");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || block_input || hold_call) return;

    save_rgb_options(MEDIUM);
    init_dropdown_settings();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || block_input || hold_call || save_mode) return;

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

    load_wallpaper(ui_screen, NULL, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_rgb_options();
    init_dropdown_settings();

    rgb_focus();
    nav_silent = 1;

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);
    nav_silent = 0;

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
