#include "muxshare.h"
#include <SDL2/SDL.h>

#define EMPTY_SLOT "---"

#define REMAP_SLOT_COUNT 23
#define PHYS_LABEL_SIZE  64

#define REMAP_DEVICE_ITEM (-100)
#define REMAP_LAYOUT_ITEM (-101)

typedef struct {
    const char *gc_key;
    const char *label;
    int is_axis;
} remap_slot_def;

static const remap_slot_def slot_defs[REMAP_SLOT_COUNT] = {
    {"a", "A", 0},
    {"b", "B", 0},
    {"c", "C", 0},
    {"x", "X", 0},
    {"y", "Y", 0},
    {"z", "Z", 0},
    {"leftshoulder", "L1", 0},
    {"rightshoulder", "R1", 0},
    {"lefttrigger", "L2", 0},
    {"righttrigger", "R2", 0},
    {"leftstick", "L3", 0},
    {"rightstick", "R3", 0},
    {"start", "Start", 0},
    {"back", "Select", 0},
    {"guide", "Menu", 0},
    {"dpup", "DPAD Up", 0},
    {"dpdown", "DPAD Down", 0},
    {"dpleft", "DPAD Left", 0},
    {"dpright", "DPAD Right", 0},
    {"leftx", "Left X", 1},
    {"lefty", "Left Y", 1},
    {"rightx", "Right X", 1},
    {"righty", "Right Y", 1},
};

static char phys[REMAP_SLOT_COUNT][32];

static lv_obj_t *item_panels[REMAP_SLOT_COUNT];
static lv_obj_t *item_labels[REMAP_SLOT_COUNT];
static lv_obj_t *item_values[REMAP_SLOT_COUNT];
static lv_obj_t *item_glyphs[REMAP_SLOT_COUNT];

static lv_obj_t *device_panel = NULL;
static lv_obj_t *device_label = NULL;
static lv_obj_t *device_value = NULL;
static lv_obj_t *device_glyph = NULL;

static lv_obj_t *layout_panel = NULL;
static lv_obj_t *layout_label = NULL;
static lv_obj_t *layout_value = NULL;
static lv_obj_t *layout_glyph = NULL;

static int has_device_item = 0;

static char ctrl_guid[64] = "";
static char ctrl_name[128] = "";
static int remap_dev_idx = 0;

static volatile int capture_active = 0;
static int capture_target = -1;
static int capture_pending_clear = 0;
static lv_timer_t *capture_timeout_timer = NULL;
static int capture_timeout_seconds = 0;

#define AXIS_CAPTURE_THRESHOLD  16384
#define CAPTURE_TIMEOUT_TICK_MS 1000
#define CAPTURE_TIMEOUT_SECONDS 3

static int remap_layout = 0; // 0 = Retro, 1 = Modern
static int pending_layout = -1;

static int entry_layout = 0;

static int save_mode = 0;
static int mapping_modified = 0;
static mux_dialogue save_dlg;

static void detect_device_at(const int idx) {
    ctrl_guid[0] = '\0';
    ctrl_name[0] = '\0';

    // Pull GUID from the board SDL map for the primary device if available
    if (idx == 0 && device.board.sdl_map[0] != '\0') {
        const char *c1 = strchr(device.board.sdl_map, ',');
        if (c1) {
            const size_t gl = (size_t) (c1 - device.board.sdl_map);
            if (gl > 0 && gl < sizeof(ctrl_guid)) {
                memcpy(ctrl_guid, device.board.sdl_map, gl);
                ctrl_guid[gl] = '\0';
            }
        }
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_PumpEvents();
    SDL_JoystickUpdate();

    if (idx >= SDL_NumJoysticks()) return;

    if (ctrl_guid[0] == '\0') {
        const SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(idx);
        SDL_JoystickGetGUIDString(guid, ctrl_guid, sizeof(ctrl_guid));
    }

    // Use the actual SDL input name (e.g. "muOS-Keys"), not the board internal name...
    const char *name = SDL_JoystickNameForIndex(idx);
    if (name) snprintf(ctrl_name, sizeof(ctrl_name), "%s", name);
}

static void parse_gcdb_value(const char *line, const char *gc_key, char *out) {
    const size_t out_size = 32;

    char search[48];
    snprintf(search, sizeof(search), ",%s:", gc_key);

    const char *p = strstr(line, search);
    if (!p) {
        snprintf(out, out_size, EMPTY_SLOT);
        return;
    }
    p += strlen(search);

    const char *end = strchr(p, ',');
    size_t len = end ? (size_t) (end - p) : strlen(p);
    if (len >= out_size) len = out_size - 1;

    memcpy(out, p, len);
    out[len] = '\0';

    if (out[0] == '\0') snprintf(out, out_size, EMPTY_SLOT);
}

static void parse_gcdb_slot(const char *line, const int idx) {
    parse_gcdb_value(line, slot_defs[idx].gc_key, phys[idx]);
}

static int slot_index_for_key(const char *gc_key) {
    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        if (strcmp(slot_defs[i].gc_key, gc_key) == 0) return i;
    }
    return -1;
}

// The boards own built-in controls are always device index 0 and is the only source
// of a sane fallback physical input we can trust for a cleared critical slot, very important!
static int get_board_default_phys(const int idx, char *out) {
    if (remap_dev_idx != 0 || !device.board.sdl_map[0]) return 0;

    parse_gcdb_value(device.board.sdl_map, slot_defs[idx].gc_key, out);
    return strcmp(out, EMPTY_SLOT) != 0;
}

static void phys_to_label(const char *p, char out[PHYS_LABEL_SIZE]) {
    if (!p || p[0] == '\0' || strcmp(p, EMPTY_SLOT) == 0) {
        snprintf(out, PHYS_LABEL_SIZE, EMPTY_SLOT);
        return;
    }

    char *ep;
    long n;

    if ((p[0] == '+' || p[0] == '-') && p[1] == 'a') {
        n = strtol(p + 2, &ep, 10);
        if (ep != p + 2) {
            snprintf(out, PHYS_LABEL_SIZE, "Axis %ld %c", n, p[0]);
            return;
        }
    }

    if (p[0] == 'a') {
        n = strtol(p + 1, &ep, 10);
        if (ep != p + 1) {
            snprintf(out, PHYS_LABEL_SIZE, "Axis %ld", n);
            return;
        }
    }

    if (p[0] == 'b') {
        n = strtol(p + 1, &ep, 10);
        if (ep != p + 1) {
            snprintf(out, PHYS_LABEL_SIZE, "Button %ld", n);
            return;
        }
    }

    if (p[0] == 'h') {
        const long hat = strtol(p + 1, &ep, 10);
        if (ep != p + 1 && *ep == '.') {
            char *ep2;
            const long val = strtol(ep + 1, &ep2, 10);
            if (ep2 != ep + 1) {
                const char *dir;
                switch (val) {
                    case 1:
                        dir = "Up";
                        break;
                    case 2:
                        dir = "Right";
                        break;
                    case 3:
                        dir = "Up+Right";
                        break;
                    case 4:
                        dir = "Down";
                        break;
                    case 6:
                        dir = "Down+Right";
                        break;
                    case 8:
                        dir = "Left";
                        break;
                    case 9:
                        dir = "Up+Left";
                        break;
                    case 12:
                        dir = "Down+Left";
                        break;
                    default:
                        dir = "?";
                        break;
                }
                snprintf(out, PHYS_LABEL_SIZE, "Hat %ld %s", hat, dir);
                return;
            }
        }
    }

    snprintf(out, PHYS_LABEL_SIZE, "%s", p);
}

static int load_from_file(const char *path, const char *guid) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[2048];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        str_remchar(line, '\n');
        if (strncmp(line, guid, 32) == 0) {
            for (int i = 0; i < REMAP_SLOT_COUNT; i++)
                parse_gcdb_slot(line, i);
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}

// Load from the active layout file (retro.txt or modern.txt), fall back to board SDL_MAP.
static void load_current_mapping(void) {
    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        snprintf(phys[i], sizeof(phys[0]), EMPTY_SLOT);
    }

    const char *layout_file = remap_layout == 1 ? OPT_PATH "share/info/gamecontrollerdb/modern.txt"
                                                : OPT_PATH "share/info/gamecontrollerdb/retro.txt";
    if (ctrl_guid[0] && load_from_file(layout_file, ctrl_guid)) return;

    if (remap_dev_idx == 0 && device.board.sdl_map[0]) {
        for (int i = 0; i < REMAP_SLOT_COUNT; i++)
            parse_gcdb_slot(device.board.sdl_map, i);
    }
}

static void build_mapping_line(char *buf) {
    char tmp[4096];
    int n = snprintf(
        tmp, sizeof(tmp), "%s,%s", ctrl_guid[0] ? ctrl_guid : "00000000000000000000000000000000",
        ctrl_name[0] ? ctrl_name : "muOS-Keys"
    );

    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        if (phys[i][0] && strcmp(phys[i], EMPTY_SLOT) != 0)
            n += snprintf(tmp + n, sizeof(tmp) - (size_t) n, ",%s:%s", slot_defs[i].gc_key, phys[i]);
    }

    snprintf(tmp + n, sizeof(tmp) - (size_t) n, ",platform:Linux,");
    snprintf(buf, 4096, "%s", tmp);
}

static int focused_slot(void) {
    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return REMAP_DEVICE_ITEM;

    return (int) (intptr_t) lv_obj_get_user_data(panel);
}

static void refresh_slot_labels(void) {
    char label[PHYS_LABEL_SIZE];
    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        phys_to_label(phys[i], label);
        lv_label_set_text(item_values[i], label);
    }
}

static void check_focus(void) {
    const int slot = focused_slot();

    lv_label_set_text(ui_lbl_nav_b, mapping_modified ? lang.generic.save : lang.generic.back);
    lv_obj_clear_flag(ui_lbl_nav_b, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_b_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

    if (slot == REMAP_DEVICE_ITEM) {
        if (has_device_item) {
            lv_label_set_text(ui_lbl_nav_a, lang.generic.change);
            lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        }
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else if (slot == REMAP_LAYOUT_ITEM) {
        lv_label_set_text(ui_lbl_nav_a, lang.generic.change);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_label_set_text(ui_lbl_nav_a, lang.generic.select);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void update_footer_glyph(lv_obj_t *glyph, const char *name) {
    if (!glyph) return;

    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];

    if (generate_image_embed(
            mux_dim, "footer", name, image_path, sizeof(image_path), image_embed, sizeof(image_embed)
        )) {
        const int footer_target = resolve_glyph_size(
            config.settings.themeopt.glyph_size_footer, theme.glyph.footer, theme.mux.item.height * 3 / 4
        );

        const int footer_px = glyph_explicit_px(config.settings.themeopt.glyph_size_footer, theme.glyph.footer);

        append_glyph_size_hint(image_embed, sizeof(image_embed), footer_target);
        lv_img_set_src(glyph, image_embed);
        apply_glyph_scale(glyph, image_embed, footer_px, footer_px);
    }

    lv_obj_update_layout(glyph);
}

static void apply_layout(const int layout) {
    remap_layout = layout;
    config.settings.remap.layout = (int16_t) layout;
    create_directories(CONF_CONFIG_PATH "settings/remap/layout", 1);
    write_text_to_file(CONF_CONFIG_PATH "settings/remap/layout", "w", INT, layout);
    mux_input_reload_mappings();

    load_current_mapping();
    refresh_slot_labels();
    mapping_modified = 0;
    check_focus();

    if (layout_value)
        lv_label_set_text(layout_value, remap_layout == 1 ? lang.muxremap.layout_modern : lang.muxremap.layout_retro);

    update_footer_glyph(ui_lbl_nav_a_glyph, remap_layout ? "b" : "a");
    update_footer_glyph(ui_lbl_nav_b_glyph, remap_layout ? "a" : "b");
    update_footer_glyph(ui_lbl_nav_x_glyph, remap_layout ? "y" : "x");
    update_footer_glyph(ui_lbl_nav_y_glyph, remap_layout ? "x" : "y");
}

static void cycle_to_next_device(void) {
    SDL_PumpEvents();
    SDL_JoystickUpdate();

    const int num = SDL_NumJoysticks();
    if (num <= 1) return;

    remap_dev_idx = (remap_dev_idx + 1) % num;
    detect_device_at(remap_dev_idx);

    load_current_mapping();
    refresh_slot_labels();

    mapping_modified = 0;
    check_focus();

    if (device_value) lv_label_set_text(device_value, ctrl_name[0] ? ctrl_name : lang.generic.unknown);

    lv_label_set_text(ui_lbl_title, ctrl_name[0] ? ctrl_name : lang.muxremap.title);

    play_sound(snd_navigate);
}

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;

    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void close_remap_module(void) {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "inputremap");
    mux_input_stop();
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
    check_focus();
}

static void clear_capture_state(void) {
    capture_active = 0;
    capture_target = -1;
    capture_pending_clear = 0;
    SAFE_DELETE(capture_timeout_timer, lv_timer_del);
}

static void set_capture_countdown_label(void) {
    if (capture_target < 0 || capture_target >= REMAP_SLOT_COUNT) return;

    char label[PHYS_LABEL_SIZE];
    snprintf(label, sizeof(label), "%s (%d)", lang.muxremap.waiting, capture_timeout_seconds);
    lv_label_set_text(item_values[capture_target], label);
}

static void capture_timeout_cb(lv_timer_t *t) {
    capture_timeout_seconds--;

    if (capture_timeout_seconds > 0) {
        set_capture_countdown_label();
        return;
    }

    lv_timer_del(t);
    capture_timeout_timer = NULL;

    if (capture_target >= 0 && capture_target < REMAP_SLOT_COUNT) {
        char label[PHYS_LABEL_SIZE];
        phys_to_label(phys[capture_target], label);
        lv_label_set_text(item_values[capture_target], label);
    }

    clear_capture_state();
    check_focus();
    play_sound(snd_back);
}

static int find_slot_using_phys(const char *phys_str, const int exclude_slot) {
    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        if (i == exclude_slot) continue;
        if (phys[i][0] && strcmp(phys[i], EMPTY_SLOT) != 0 && strcmp(phys[i], phys_str) == 0) return i;
    }
    return -1;
}

static void raw_event_capture(const SDL_Event *ev) {
    if (capture_pending_clear) {
        clear_capture_state();
        return;
    }

    if (!capture_active || capture_target < 0 || save_mode) return;

    char phys_str[32] = "";

    switch (ev->type) {
        case SDL_JOYBUTTONDOWN:
            snprintf(phys_str, sizeof(phys_str), "b%d", ev->jbutton.button);
            break;
        case SDL_JOYHATMOTION:
            if (ev->jhat.value == SDL_HAT_CENTERED) return;
            snprintf(phys_str, sizeof(phys_str), "h%d.%d", ev->jhat.hat, ev->jhat.value);
            break;
        case SDL_JOYAXISMOTION: {
            const int16_t val = ev->jaxis.value;
            if (val < -AXIS_CAPTURE_THRESHOLD || val > AXIS_CAPTURE_THRESHOLD) {
                const int axis = ev->jaxis.axis;
                if (slot_defs[capture_target].is_axis) {
                    snprintf(phys_str, sizeof(phys_str), "a%d", axis);
                } else {
                    snprintf(phys_str, sizeof(phys_str), "%sa%d", val > 0 ? "+" : "-", axis);
                }
            }
            break;
        }
        default:
            return;
    }

    if (phys_str[0] == '\0') return;

    if (find_slot_using_phys(phys_str, capture_target) >= 0) {
        play_sound(snd_error);
        toast_message(lang.muxremap.already_used, tst_wait_m);

        capture_timeout_seconds = CAPTURE_TIMEOUT_SECONDS;
        set_capture_countdown_label();
        return;
    }

    snprintf(phys[capture_target], sizeof(phys[0]), "%s", phys_str);
    char label[PHYS_LABEL_SIZE];
    phys_to_label(phys[capture_target], label);
    lv_label_set_text(item_values[capture_target], label);

    mapping_modified = 1;
    capture_pending_clear = 1;
    SAFE_DELETE(capture_timeout_timer, lv_timer_del);

    check_focus();
}

static void cycle_layout(void) {
    const int new_layout = 1 - remap_layout;
    if (mapping_modified) {
        pending_layout = new_layout;
        play_sound(snd_info_open);
        show_save_dialog();
        return;
    }

    apply_layout(new_layout);
    play_sound(snd_navigate);
}

// This is a fallback catch... So DPAD and A+B must never all be capable of ending up unmapped,
// or the whole frontend becomes unusable with no way back into the remapper to fix it. If one of
// these was cleared, first try to fail safe back to the boards own default physical input for it,
// but only if that physical control has not been intentionally reassigned to something else.
// Anything that still can't be resolved blocks it entirely upon save and quit...
static int validate_and_backfill_mapping(void) {
    static const char *const critical_keys[] = {"dpup", "dpdown", "dpleft", "dpright", "a", "b"};

    int recovered = 0;
    int missing = 0;

    for (size_t k = 0; k < A_SIZE(critical_keys); k++) {
        const int idx = slot_index_for_key(critical_keys[k]);
        if (idx < 0 || (phys[idx][0] && strcmp(phys[idx], EMPTY_SLOT) != 0)) continue;

        char fallback[32];
        if (get_board_default_phys(idx, fallback) && find_slot_using_phys(fallback, idx) < 0) {
            snprintf(phys[idx], sizeof(phys[0]), "%s", fallback);
            recovered = 1;
        } else {
            missing = 1;
        }
    }

    if (recovered) refresh_slot_labels();

    return !missing;
}

static int do_save(void) {
    if (!validate_and_backfill_mapping()) {
        play_sound(snd_error);
        toast_message(lang.muxremap.core_missing, tst_wait_l);
        return 0;
    }

    char mapping[4096];
    build_mapping_line(mapping);

    const char *target = remap_layout == 1 ? "modern" : "retro";
    const char *args[] = {OPT_PATH "script/mux/sdl_remap.sh", target, mapping, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    mux_input_reload_mappings();
    return 1;
}

static void handle_a(void) {
    if (hold_call) return;

    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) {
            if (!do_save()) {
                pending_layout = -1;
                check_focus();
                return;
            }

            entry_layout = remap_layout;
        }

        if (pending_layout >= 0) {
            apply_layout(pending_layout);
            pending_layout = -1;
            play_sound(snd_navigate);
        } else {
            if (opt == mux_unsaved_discard) apply_layout(entry_layout);
            play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
            close_remap_module();
        }
        return;
    }

    if (capture_active) return;

    const int slot = focused_slot();

    if (slot == REMAP_DEVICE_ITEM) {
        if (has_device_item) cycle_to_next_device();
        return;
    }

    if (slot == REMAP_LAYOUT_ITEM) {
        cycle_layout();
        return;
    }

    if (slot < 0 || slot >= REMAP_SLOT_COUNT) return;

    capture_target = slot;
    capture_active = 1;
    capture_timeout_seconds = CAPTURE_TIMEOUT_SECONDS;
    capture_timeout_timer = lv_timer_create(capture_timeout_cb, CAPTURE_TIMEOUT_TICK_MS, NULL);

    set_capture_countdown_label();
    play_sound(snd_confirm);
}

static void handle_b(void) {
    if (hold_call || capture_active) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (mapping_modified) {
        pending_layout = -1;
        play_sound(snd_info_open);
        show_save_dialog();
        return;
    }

    play_sound(snd_back);
    close_remap_module();
}

static void handle_x(void) {
    if (hold_call || capture_active || save_mode) return;

    const int slot = focused_slot();
    if (slot < 0 || slot >= REMAP_SLOT_COUNT) return;

    play_sound(snd_info_close);

    snprintf(phys[slot], sizeof(phys[0]), EMPTY_SLOT);
    lv_label_set_text(item_values[slot], phys[slot]);
    mapping_modified = 1;

    check_focus();
}

static void handle_y(void) {
    if (hold_call || capture_active || save_mode) return;

    play_sound(snd_info_close);

    load_current_mapping();
    refresh_slot_labels();
    mapping_modified = 0;

    check_focus();
}

static void handle_dpad_left(void) {
    if (capture_active || save_mode) return;
    const int slot = focused_slot();

    if (slot == REMAP_DEVICE_ITEM && has_device_item) {
        cycle_to_next_device();
    } else if (slot == REMAP_LAYOUT_ITEM) {
        cycle_layout();
    }
}

static void handle_dpad_right(void) {
    if (capture_active || save_mode) return;
    const int slot = focused_slot();

    if (slot == REMAP_DEVICE_ITEM && has_device_item) {
        cycle_to_next_device();
    } else if (slot == REMAP_LAYOUT_ITEM) {
        cycle_layout();
    }
}

static void handle_dpad_up(void) {
    if (capture_active) return;

    if (save_mode) {
        dialogue_navigate(&save_dlg, &theme, -1);
        play_sound(snd_navigate);
        return;
    }

    handle_list_nav_up();
    check_focus();
}

static void handle_dpad_down(void) {
    if (capture_active) return;

    if (save_mode) {
        dialogue_navigate(&save_dlg, &theme, +1);
        play_sound(snd_navigate);
        return;
    }

    handle_list_nav_down();
    check_focus();
}

static void handle_dpad_up_hold(void) {
    if (capture_active || save_mode) return;

    handle_list_nav_up_hold();
    check_focus();
}

static void handle_dpad_down_hold(void) {
    if (capture_active || save_mode) return;

    handle_list_nav_down_hold();
    check_focus();
}

static void handle_page_up(void) {
    if (capture_active || save_mode) return;

    handle_list_nav_page_up();
    check_focus();
}

static void handle_page_down(void) {
    if (capture_active || save_mode) return;

    handle_list_nav_page_down();
    check_focus();
}

static void init_navigation_group(void) {
    reset_ui_groups();

    const int num_joy = SDL_NumJoysticks();
    has_device_item = num_joy > 1;

    {
        device_panel = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(device_panel);

        device_label = lv_label_create(device_panel);
        apply_theme_list_item(&theme, device_label, lang.muxremap.input_label);

        device_value = lv_label_create(device_panel);
        apply_theme_list_value(&theme, device_value, ctrl_name[0] ? ctrl_name : lang.generic.unknown);

        device_glyph = lv_img_create(device_panel);
        apply_theme_list_glyph(&theme, device_glyph, mux_module, "input");

        lv_obj_set_user_data(device_panel, (void *) (intptr_t) REMAP_DEVICE_ITEM);

        apply_size_to_content(&theme, ui_pnl_content, device_label, device_glyph, lang.muxremap.input_label);
        apply_text_long_dot(&theme, device_label);

        lv_group_add_obj(ui_group, device_label);
        lv_group_add_obj(ui_group_value, device_value);
        lv_group_add_obj(ui_group_glyph, device_glyph);
        lv_group_add_obj(ui_group_panel, device_panel);
    }

    {
        layout_panel = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(layout_panel);

        layout_label = lv_label_create(layout_panel);
        apply_theme_list_item(&theme, layout_label, lang.muxremap.layout_label);

        layout_value = lv_label_create(layout_panel);
        apply_theme_list_value(
            &theme, layout_value, remap_layout == 1 ? lang.muxremap.layout_modern : lang.muxremap.layout_retro
        );

        layout_glyph = lv_img_create(layout_panel);
        apply_theme_list_glyph(&theme, layout_glyph, mux_module, "layout");

        lv_group_add_obj(ui_group, layout_label);
        lv_group_add_obj(ui_group_value, layout_value);
        lv_group_add_obj(ui_group_glyph, layout_glyph);
        lv_group_add_obj(ui_group_panel, layout_panel);

        lv_obj_set_user_data(layout_panel, (void *) (intptr_t) REMAP_LAYOUT_ITEM);

        apply_size_to_content(&theme, ui_pnl_content, layout_label, layout_glyph, lang.muxremap.layout_label);
        apply_text_long_dot(&theme, layout_label);
    }

    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        lv_obj_t *panel = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(panel);

        lv_obj_t *label = lv_label_create(panel);
        apply_theme_list_item(&theme, label, slot_defs[i].label);

        char val_label[PHYS_LABEL_SIZE];
        phys_to_label(phys[i], val_label);
        lv_obj_t *value = lv_label_create(panel);
        apply_theme_list_value(&theme, value, val_label);

        lv_obj_t *glyph = lv_img_create(panel);
        apply_theme_list_glyph(&theme, glyph, mux_module, "input");

        lv_group_add_obj(ui_group, label);
        lv_group_add_obj(ui_group_value, value);
        lv_group_add_obj(ui_group_glyph, glyph);
        lv_group_add_obj(ui_group_panel, panel);

        lv_obj_set_user_data(panel, (void *) (intptr_t) i);

        apply_size_to_content(&theme, ui_pnl_content, label, glyph, slot_defs[i].label);
        apply_text_long_dot(&theme, label);

        item_panels[i] = panel;
        item_labels[i] = label;
        item_values[i] = value;
        item_glyphs[i] = glyph;
    }

    ui_count_static = REMAP_SLOT_COUNT + 2;

    lv_obj_update_layout(ui_pnl_content);
    gen_step_movement(0, +1, 2, 0, 1);
    check_focus();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.clear, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.reset, 0},
                                  {ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {NULL, NULL, 0}});

    lv_label_set_text(ui_lbl_screen_message, "");

    overlay_display();
}

int muxremap_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    remap_layout = config.settings.remap.layout == 1 ? 1 : 0;
    entry_layout = remap_layout;
    pending_layout = -1;
    mapping_modified = 0;
    save_mode = 0;
    capture_active = 0;
    capture_target = -1;
    capture_pending_clear = 0;

    init_ui_common_screen(&theme, &device, &lang, lang.muxremap.title);

    remap_dev_idx = 0;
    detect_device_at(0);
    load_current_mapping();

    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .hold_handler =
            {
                [mux_input_dpad_up] = handle_dpad_up_hold,
                [mux_input_dpad_down] = handle_dpad_down_hold,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .raw_event_handler = raw_event_capture,
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
