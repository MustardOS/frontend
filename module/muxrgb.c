#include "muxshare.h"
#include "ui/ui_muxrgb.h"

#define RGB(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(RGB_ELEMENTS) };
#undef RGB

#define RGB(NAME, UDATA) static int NAME##_original;
RGB_ELEMENTS
#undef RGB

#define RGB_ZONE_L      (1u << 0)
#define RGB_ZONE_R      (1u << 1)
#define RGB_ZONE_M      (1u << 2)
#define RGB_ZONE_F1     (1u << 3)
#define RGB_ZONE_F2     (1u << 4)
#define RGB_ZONE_SINGLE (1u << 5)

#define M_BIT(mode) (1u << (mode))

#define MODES_SERIAL                                                                                                   \
    (M_BIT(RGB_MODE_OFF) | M_BIT(RGB_MODE_STATIC) | M_BIT(RGB_MODE_BREATHING) | M_BIT(RGB_MODE_PRESET_COMBO)           \
     | M_BIT(RGB_MODE_THEME_SUPPLIED))

#define MODES_SYSFS                                                                                                    \
    (M_BIT(RGB_MODE_OFF) | M_BIT(RGB_MODE_STATIC) | M_BIT(RGB_MODE_BREATHING) | M_BIT(RGB_MODE_PRESET_COMBO)           \
     | M_BIT(RGB_MODE_THEME_SUPPLIED))

#define MODES_JOYPAD                                                                                                   \
    (M_BIT(RGB_MODE_OFF) | M_BIT(RGB_MODE_STATIC) | M_BIT(RGB_MODE_BREATHING) | M_BIT(RGB_MODE_THEME_SUPPLIED)         \
     | M_BIT(RGB_MODE_COLOUR_CYCLE) | M_BIT(RGB_MODE_RAINBOW) | M_BIT(RGB_MODE_STICK_FOLLOW))

typedef struct {
    const char *code;
    uint32_t zones;
    rgb_backend_t backend;
    uint16_t mode_mask;
} rgb_device_caps_t;

static const rgb_device_caps_t rgb_devices[] = {
    {"gcs-h36s", RGB_ZONE_L | RGB_ZONE_R, rgb_backend_serial, MODES_SERIAL},

    {"rg40xx-h", RGB_ZONE_L | RGB_ZONE_R, rgb_backend_serial, MODES_SERIAL},

    {"rg40xx-v", RGB_ZONE_L | RGB_ZONE_SINGLE, rgb_backend_serial, MODES_SERIAL},

    {"rgcubexx-h", RGB_ZONE_L | RGB_ZONE_R, rgb_backend_serial, MODES_SERIAL},

    {"rg-vita-pro", RGB_ZONE_L | RGB_ZONE_SINGLE, rgb_backend_joypad, MODES_JOYPAD},

    {"tui-brick", RGB_ZONE_L | RGB_ZONE_R | RGB_ZONE_M | RGB_ZONE_F1 | RGB_ZONE_F2, rgb_backend_sysfs, MODES_SYSFS},

    {"tui-spoon", RGB_ZONE_L | RGB_ZONE_R, rgb_backend_sysfs, MODES_SYSFS},
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

#define MODE_TABLE_MAX 16

static int mode_dropdown_to_enum[MODE_TABLE_MAX];
static int mode_enum_to_dropdown[MODE_TABLE_MAX];
static int mode_dropdown_count = 0;

static int mode_enum_from_dropdown(int dropdown_idx);

static int mode_dropdown_from_enum(int enum_val);

static int colour_palette_idx(void);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define RGB(NAME, UDATA) {UDATA, lang.muxrgb.help.NAME},
        RGB_ELEMENTS
#undef RGB
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int current_mode_enum(void) {
    const int selected = lv_dropdown_get_selected(ui_dro_mode_rgb);

    return mode_enum_from_dropdown(selected);
}

static int effective_backend_from_saved(const int saved) {
    if (rgb_caps && rgb_caps->backend != rgb_backend_auto) return rgb_caps->backend;

    switch (saved) {
        case rgb_backend_sysfs:
        case rgb_backend_serial:
        case rgb_backend_joypad:
            return saved;
        case rgb_backend_auto:
        default:
            return rgb_backend_auto;
    }
}

static int last_applied_mode = -1;

static void apply_mode_visibility(int ui_mode);

static void rgb_focus(void) {
    if (!rgb_caps) return;

    const int ui_mode = current_mode_enum();
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
#define RGB(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_rgb);
    RGB_ELEMENTS
#undef RGB

    bright_original = pct_to_int(lv_dropdown_get_selected(ui_dro_bright_rgb), 0, 255);
}

static void restore_rgb_options(void) {
    const int saved_mode = config.settings.rgb.mode;
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

    lv_dropdown_set_selected(ui_dro_mode_rgb, mode_dropdown_from_enum(new_mode));
    lv_dropdown_set_selected(ui_dro_bright_rgb, int_to_pct(config.settings.rgb.bright, 0, 255));

    {
        int saved_bs = config.settings.rgb.breath_speed;
        const int joypad = rgb_caps && rgb_caps->backend == rgb_backend_joypad;
        if (joypad) {
            if (saved_bs >= 0 && saved_bs <= 2) {
                const int fms[] = {80, 50, 20};
                saved_bs = fms[saved_bs];
            }
            if (saved_bs < 0) saved_bs = 0;
            if (saved_bs > 100) saved_bs = 100;
        }
        lv_dropdown_set_selected(ui_dro_breath_speed_rgb, saved_bs);
    }

    lv_dropdown_set_selected(ui_dro_colour_l_rgb, config.settings.rgb.colour_l);
    lv_dropdown_set_selected(ui_dro_colour_r_rgb, config.settings.rgb.colour_r);
    lv_dropdown_set_selected(ui_dro_colour_m_rgb, config.settings.rgb.colour_m);
    lv_dropdown_set_selected(ui_dro_colour_f1_rgb, config.settings.rgb.colour_f1);
    lv_dropdown_set_selected(ui_dro_colour_f2_rgb, config.settings.rgb.colour_f2);
    lv_dropdown_set_selected(ui_dro_combo_rgb, config.settings.rgb.combo);
    lv_dropdown_set_selected(ui_dro_backend_rgb, effective_backend_from_saved(config.settings.rgb.backend));
}

static void save_rgb_options(const int toast_vis) {
    int is_modified = 0;

    {
        const int current_idx = lv_dropdown_get_selected(ui_dro_mode_rgb);
        if (current_idx != mode_original) {
            const int current_enum = mode_enum_from_dropdown(current_idx);
            write_text_to_file("/opt/muos/config/settings/rgb/mode", "w", INT, current_enum);
            is_modified++;
        }
    }

    CHECK_AND_SAVE_PCT(rgb, bright, "settings/rgb/bright", INT, 0, 255);
    CHECK_AND_SAVE_STD(rgb, breath_speed, "settings/rgb/breath_speed", INT, 0);

    {
        const int current_palette_idx = colour_palette_idx();
        if (current_palette_idx != colour_l_original) {
            write_text_to_file("/opt/muos/config/settings/rgb/colour_l", "w", INT, current_palette_idx);
            is_modified++;
        }
    }

    CHECK_AND_SAVE_STD(rgb, colour_r, "settings/rgb/colour_r", INT, 0);
    CHECK_AND_SAVE_STD(rgb, colour_m, "settings/rgb/colour_m", INT, 0);
    CHECK_AND_SAVE_STD(rgb, colour_f1, "settings/rgb/colour_f1", INT, 0);
    CHECK_AND_SAVE_STD(rgb, colour_f2, "settings/rgb/colour_f2", INT, 0);
    CHECK_AND_SAVE_STD(rgb, combo, "settings/rgb/combo", INT, 0);
    CHECK_AND_SAVE_STD(rgb, backend, "settings/rgb/backend", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, toast_vis);
        refresh_config = 1;
    }

    rgb_apply();
}

static char **build_colour_options(int *count) {
    char **out = calloc(rgb_colour_count, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    for (size_t i = 0; i < rgb_colour_count; i++)
        out[i] = strdup(rgb_colours[i].name);
    *count = (int) rgb_colour_count;

    return out;
}

static char **build_zone_options(int *count) {
    char **out = calloc(rgb_colour_count + 1, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    out[0] = strdup(lang.muxrgb.same_as_l);

    for (size_t i = 0; i < rgb_colour_count; i++)
        out[i + 1] = strdup(rgb_colours[i].name);
    *count = (int) (rgb_colour_count + 1);

    return out;
}

static char **build_combo_options(int *count) {
    char **out = calloc(rgb_colour_combo_count, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    for (size_t i = 0; i < rgb_colour_combo_count; i++)
        out[i] = strdup(rgb_colour_combos[i].name);
    *count = (int) rgb_colour_combo_count;

    return out;
}

// This is something unique where the stick follow, at least for the
// rg-vita-pro device only has "8" distinct stick follow colours and
// no other custom colours work, "0" or "Off" is the last one!
static const int stick_follow_palette_idx[] = {
    1,  // Red       (255,   0,   0)
    16, // Yellow    (255, 255,   0)
    19, // Green     (  0, 255,   0)
    26, // Neon Cyan (  0, 255, 255)
    33, // True Blue (  0,   0, 255)
    40, // Magenta   (255,   0, 255)
    51, // White     (255, 255, 255)
};
#define STICK_FOLLOW_PALETTE_COUNT (sizeof(stick_follow_palette_idx) / sizeof(stick_follow_palette_idx[0]))

static int stick_palette_idx_from_dropdown(const int dropdown_idx) {
    if (dropdown_idx < 0 || dropdown_idx >= (int) STICK_FOLLOW_PALETTE_COUNT) return -1;

    return stick_follow_palette_idx[dropdown_idx];
}

static int stick_dropdown_idx_from_palette(const int palette_idx) {
    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        if (stick_follow_palette_idx[i] == palette_idx) return (int) i;
    }

    return 0;
}

static char *build_stick_palette_options_string(void) {
    size_t total = 1;
    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        const int idx = stick_follow_palette_idx[i];
        if (idx < 0 || idx >= (int) rgb_colour_count) continue;
        total += strlen(rgb_colours[idx].name) + 1;
    }

    char *buf = calloc(total, 1);
    if (!buf) return NULL;

    char *p = buf;
    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        const int idx = stick_follow_palette_idx[i];
        if (idx < 0 || idx >= (int) rgb_colour_count) continue;
        const char *name = rgb_colours[idx].name;
        if (i > 0) *p++ = '\n';
        const size_t n = strlen(name);
        memcpy(p, name, n);
        p += n;
    }

    *p = '\0';

    return buf;
}

static char *build_full_palette_options_string(void) {
    size_t total = 1;
    for (size_t i = 0; i < rgb_colour_count; i++)
        total += strlen(rgb_colours[i].name) + 1;

    char *buf = calloc(total, 1);
    if (!buf) return NULL;

    char *p = buf;
    for (size_t i = 0; i < rgb_colour_count; i++) {
        if (i > 0) *p++ = '\n';
        const size_t n = strlen(rgb_colours[i].name);
        memcpy(p, rgb_colours[i].name, n);
        p += n;
    }

    *p = '\0';

    return buf;
}

typedef enum {
    colourl_dropdown_full = 0,
    colourl_dropdown_stick = 1,
} colourl_dropdown_set_t;

static colourl_dropdown_set_t colourl_dropdown_state = colourl_dropdown_full;

static void colourl_set_dropdown_set(const colourl_dropdown_set_t target) {
    if (target == colourl_dropdown_state) return;

    const int current_dropdown_idx = lv_dropdown_get_selected(ui_dro_colour_l_rgb);
    int current_palette_idx;
    if (colourl_dropdown_state == colourl_dropdown_stick) {
        current_palette_idx = stick_palette_idx_from_dropdown(current_dropdown_idx);
        if (current_palette_idx < 0) current_palette_idx = stick_follow_palette_idx[0];
    } else {
        current_palette_idx = current_dropdown_idx;
    }

    char *opts;
    int new_dropdown_idx;
    if (target == colourl_dropdown_stick) {
        opts = build_stick_palette_options_string();
        new_dropdown_idx = stick_dropdown_idx_from_palette(current_palette_idx);
    } else {
        opts = build_full_palette_options_string();
        new_dropdown_idx = current_palette_idx;
        if (new_dropdown_idx < 0 || new_dropdown_idx >= (int) rgb_colour_count) new_dropdown_idx = 0;
    }

    if (opts) {
        lv_dropdown_set_options(ui_dro_colour_l_rgb, opts);
        free(opts);
    }

    lv_dropdown_set_selected(ui_dro_colour_l_rgb, new_dropdown_idx);
    colourl_dropdown_state = target;
}

static int colour_palette_idx(void) {
    const int sel = lv_dropdown_get_selected(ui_dro_colour_l_rgb);

    if (colourl_dropdown_state == colourl_dropdown_stick) {
        const int p = stick_palette_idx_from_dropdown(sel);
        return p < 0 ? stick_follow_palette_idx[0] : p;
    }

    if (sel < 0 || sel >= (int) rgb_colour_count) return 0;
    return sel;
}

static char **build_mode_options(int *count) {
    static const struct {
        int mode_enum;
        int gated;
        const char *fallback_name;
    } slots[] = {
        {RGB_MODE_OFF, 0, lang.muxrgb.mode_name.off},
        {RGB_MODE_STATIC, 0, lang.muxrgb.mode_name.statc},
        {RGB_MODE_BREATHING, 0, lang.muxrgb.mode_name.breathing},
        {RGB_MODE_COLOUR_CYCLE, 1, lang.muxrgb.mode_name.colour_cycle},
        {RGB_MODE_RAINBOW, 1, lang.muxrgb.mode_name.rainbow},
        {RGB_MODE_STICK_FOLLOW, 1, lang.muxrgb.mode_name.stick_follow},
        {RGB_MODE_PRESET_COMBO, 1, lang.muxrgb.mode_name.preset_combo},
        {RGB_MODE_THEME_SUPPLIED, 0, lang.muxrgb.mode_name.theme_supplied},
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
        const int e = slots[i].mode_enum;
        const int show = !slots[i].gated || (rgb_caps && rgb_caps->mode_mask & M_BIT(e));
        if (!show) continue;

        const char *label = NULL;
        switch (e) {
            case RGB_MODE_OFF:
                label = lang.muxrgb.mode_name.off;
                break;
            case RGB_MODE_STATIC:
                label = lang.muxrgb.mode_name.statc;
                break;
            case RGB_MODE_BREATHING:
                label = lang.muxrgb.mode_name.breathing;
                break;
            case RGB_MODE_PRESET_COMBO:
                label = lang.muxrgb.mode_name.preset_combo;
                break;
            case RGB_MODE_THEME_SUPPLIED:
                label = lang.muxrgb.mode_name.theme_supplied;
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

static int mode_enum_from_dropdown(const int dropdown_idx) {
    static const int fallback[] = {
        RGB_MODE_OFF,     RGB_MODE_STATIC,       RGB_MODE_BREATHING,    RGB_MODE_COLOUR_CYCLE,
        RGB_MODE_RAINBOW, RGB_MODE_STICK_FOLLOW, RGB_MODE_PRESET_COMBO, RGB_MODE_THEME_SUPPLIED,
    };

    if (dropdown_idx < 0) return RGB_MODE_OFF;

    if (mode_dropdown_count > 0 && dropdown_idx < mode_dropdown_count) {
        const int e = mode_dropdown_to_enum[dropdown_idx];
        if (e >= 0) return e;
    }

    if (dropdown_idx < (int) A_SIZE(fallback)) return fallback[dropdown_idx];

    return RGB_MODE_OFF;
}

static int mode_dropdown_from_enum(const int enum_val) {
    if (enum_val >= 0 && enum_val < MODE_TABLE_MAX && mode_dropdown_count > 0) {
        const int idx = mode_enum_to_dropdown[enum_val];
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

static void free_string_array(char **arr, const int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++)
        free(arr[i]);
    free(arr);
}

#define SET_OPTION_VISIBLE(MODULE, NAME, SHOULD_SHOW)                                                                  \
    do {                                                                                                               \
        if (SHOULD_SHOW)                                                                                               \
            SHOW_OPTION_ITEM(MODULE, NAME);                                                                            \
        else                                                                                                           \
            HIDE_OPTION_ITEM(MODULE, NAME);                                                                            \
    } while (0)

static void apply_mode_visibility(const int ui_mode) {
    const int single = rgb_caps && rgb_caps->zones & RGB_ZONE_SINGLE;
    const int is_sysfs = rgb_caps && rgb_caps->backend == rgb_backend_sysfs;
    const int is_serial = rgb_caps && rgb_caps->backend == rgb_backend_serial;
    const int is_joypad = rgb_caps && rgb_caps->backend == rgb_backend_joypad;

    const int caps_colour_r = (is_sysfs || is_serial) && rgb_caps && !single && rgb_caps->zones & RGB_ZONE_R;
    const int caps_colour_m = is_sysfs && rgb_caps && !single && rgb_caps->zones & RGB_ZONE_M;
    const int caps_colour_f1 = is_sysfs && rgb_caps && !single && rgb_caps->zones & RGB_ZONE_F1;
    const int caps_colour_f2 = is_sysfs && rgb_caps && !single && rgb_caps->zones & RGB_ZONE_F2;
    const int caps_combo = rgb_caps && rgb_caps->mode_mask & M_BIT(RGB_MODE_PRESET_COMBO);
    const int caps_breathing = rgb_caps && rgb_caps->mode_mask & M_BIT(RGB_MODE_BREATHING);

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
            const int rainbow_is_joypad = rgb_caps && rgb_caps->backend == rgb_backend_joypad;
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

    SET_OPTION_VISIBLE(rgb, bright, show_bright);
    SET_OPTION_VISIBLE(rgb, breath_speed, show_breath_speed);
    SET_OPTION_VISIBLE(rgb, colour_l, show_colour_l);
    SET_OPTION_VISIBLE(rgb, colour_r, show_colour_r);
    SET_OPTION_VISIBLE(rgb, colour_m, show_colour_m);
    SET_OPTION_VISIBLE(rgb, colour_f1, show_colour_f1);
    SET_OPTION_VISIBLE(rgb, colour_f2, show_colour_f2);
    SET_OPTION_VISIBLE(rgb, combo, show_combo);
    SET_OPTION_VISIBLE(rgb, backend, show_backend);

    if (is_joypad) {
        if (ui_mode == RGB_MODE_BREATHING) {
            lv_label_set_text(ui_lbl_colour_l_rgb, lang.muxrgb.stick.primary);
            lv_label_set_text(ui_lbl_colour_r_rgb, lang.muxrgb.stick.secondary);
        } else if (ui_mode == RGB_MODE_STICK_FOLLOW) {
            lv_label_set_text(ui_lbl_colour_l_rgb, lang.muxrgb.stick.single);
            lv_label_set_text(ui_lbl_colour_r_rgb, lang.muxrgb.stick.single);
        } else {
            lv_label_set_text(ui_lbl_colour_l_rgb, lang.muxrgb.stick.both);
            lv_label_set_text(ui_lbl_colour_r_rgb, lang.muxrgb.stick.both);
        }

        colourl_set_dropdown_set(ui_mode == RGB_MODE_STICK_FOLLOW ? colourl_dropdown_stick : colourl_dropdown_full);
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *breath_speed_options[] = {
        lang.muxrgb.breath_speed_name.fast,
        lang.muxrgb.breath_speed_name.medium,
        lang.muxrgb.breath_speed_name.slow,
    };

    const int joypad_speed_dropdown = rgb_caps && rgb_caps->backend == rgb_backend_joypad;

    char *backend_options[] = {
        lang.muxrgb.backend_name.autom,
        lang.muxrgb.backend_name.sysfs,
        lang.muxrgb.backend_name.serial,
        lang.muxrgb.backend_name.joypad,
    };

    int mode_count = 0;
    char **mode_options = build_mode_options(&mode_count);

    int colour_count = 0;
    int zone_count = 0;
    int combo_count = 0;

    char **colour_options = build_colour_options(&colour_count);
    char **zone_options = build_zone_options(&zone_count);
    char **combo_options = build_combo_options(&combo_count);

    INIT_OPTION_ITEM(-1, rgb, mode, lang.muxrgb.mode, "mode", mode_options, mode_count);
    INIT_OPTION_ITEM(-1, rgb, bright, lang.muxrgb.bright, "bright", NULL, 0);
    if (joypad_speed_dropdown) {
        INIT_OPTION_ITEM(-1, rgb, breath_speed, lang.muxrgb.breath_speed, "breath_speed", NULL, 0);
    } else {
        INIT_OPTION_ITEM(-1, rgb, breath_speed, lang.muxrgb.breath_speed, "breath_speed", breath_speed_options, 3);
    }
    INIT_OPTION_ITEM(-1, rgb, colour_l, lang.muxrgb.colourl, "colour_l", colour_options, colour_count);
    INIT_OPTION_ITEM(-1, rgb, colour_r, lang.muxrgb.colourr, "colour_r", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, colour_m, lang.muxrgb.colourm, "colour_m", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, colour_f1, lang.muxrgb.colourf1, "colour_f1", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, colour_f2, lang.muxrgb.colourf2, "colour_f2", zone_options, zone_count);
    INIT_OPTION_ITEM(-1, rgb, combo, lang.muxrgb.combo, "combo", combo_options, combo_count);
    INIT_OPTION_ITEM(-1, rgb, backend, lang.muxrgb.backend, "backend", backend_options, 4);

    char *bright_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_lbl_bright_rgb, ui_dro_bright_rgb, bright_values);
    free(bright_values);

    if (joypad_speed_dropdown) {
        char *speed_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
        apply_theme_list_drop_down(&theme, ui_lbl_breath_speed_rgb, ui_dro_breath_speed_rgb, speed_values);
        free(speed_values);
    }

    free_string_array(mode_options, mode_count);
    free_string_array(colour_options, colour_count);
    free_string_array(zone_options, zone_count);
    free_string_array(combo_options, combo_count);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!rgb_caps) LOG_WARN(mux_module, "No caps entry for board '%s'; showing all rows", device.board.name);

    for (size_t i = 0; i < STICK_FOLLOW_PALETTE_COUNT; i++) {
        const int idx = stick_follow_palette_idx[i];
        if (idx < 0 || idx >= (int) rgb_colour_count) {
            LOG_WARN(
                mux_module, "stick_follow_palette_idx[%zu]=%d is out of range (palette size %zu)", i, idx,
                (size_t) rgb_colour_count
            );
        }
    }

    if (rgb_caps && rgb_caps->backend == rgb_backend_joypad) {
        lv_label_set_text(ui_lbl_breath_speed_rgb, lang.muxrgb.speed);
    }

    const int initial_mode = current_mode_enum();
    apply_mode_visibility(initial_mode);
    last_applied_mode = initial_mode;

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 2, 0, 1);
}

static int save_mode = 0;
static mux_dialogue save_dlg;

static void hide_save_dialog(void) {
    dialogue_dismiss(&save_mode, &save_dlg);
}

static int any_rgb_modified(void) {
    if ((int) lv_dropdown_get_selected(ui_dro_mode_rgb) != mode_original) return 1;
    if (pct_to_int(lv_dropdown_get_selected(ui_dro_bright_rgb), 0, 255) != bright_original) return 1;

    if ((int) lv_dropdown_get_selected(ui_dro_breath_speed_rgb) != breath_speed_original) return 1;
    if (colour_palette_idx() != colour_l_original) return 1;

    if ((int) lv_dropdown_get_selected(ui_dro_colour_r_rgb) != colour_r_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_colour_m_rgb) != colour_m_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_colour_f1_rgb) != colour_f1_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_colour_f2_rgb) != colour_f2_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_combo_rgb) != combo_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_backend_rgb) != backend_original) return 1;

    return 0;
}

static void handle_option_prev(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
    rgb_focus();
}

static void handle_option_next(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
    rgb_focus();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
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
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_rgb_options(tst_wait_f);

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
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

    if (dialogue_guard_unsaved(&save_mode, &save_dlg, &theme, any_rgb_modified())) return;

    play_sound(snd_back);
    save_rgb_options(tst_wait_f);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "rgb");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || block_input || hold_call) return;

    save_rgb_options(tst_wait_m);
    init_dropdown_settings();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || block_input || hold_call || save_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.set, 0},
                                  {NULL, NULL, 0}});

#define RGB(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_rgb, UDATA);
    RGB_ELEMENTS
#undef RGB

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

    restore_rgb_options();
    init_dropdown_settings();

    rgb_focus();
    nav_silent = 1;

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.back
    );

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
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
