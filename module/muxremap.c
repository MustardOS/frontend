#include "muxshare.h"
#include <SDL2/SDL.h>

#define REMAP_SLOT_COUNT  23
#define REMAP_DEVICE_ITEM (-100)

typedef struct {
    const char *gc_key;
    const char *label;
    int is_axis;
    int is_half_axis;
} remap_slot_def;

static const remap_slot_def slot_defs[REMAP_SLOT_COUNT] = {
        {"a",             "A",          0, 0},
        {"b",             "B",          0, 0},
        {"c",             "C",          0, 0},
        {"x",             "X",          0, 0},
        {"y",             "Y",          0, 0},
        {"z",             "Z",          0, 0},
        {"leftshoulder",  "L1",         0, 0},
        {"rightshoulder", "R1",         0, 0},
        {"lefttrigger",   "L2",         0, 1},
        {"righttrigger",  "R2",         0, 1},
        {"leftstick",     "L3",         0, 0},
        {"rightstick",    "R3",         0, 0},
        {"start",         "Start",      0, 0},
        {"back",          "Select",     0, 0},
        {"guide",         "Menu",       0, 0},
        {"dpup",          "DPAD Up",    0, 0},
        {"dpdown",        "DPAD Down",  0, 0},
        {"dpleft",        "DPAD Left",  0, 0},
        {"dpright",       "DPAD Right", 0, 0},
        {"leftx",         "Left X",     1, 0},
        {"lefty",         "Left Y",     1, 0},
        {"rightx",        "Right X",    1, 0},
        {"righty",        "Right Y",    1, 0},
};

static char phys[REMAP_SLOT_COUNT][32];
static lv_obj_t *item_values[REMAP_SLOT_COUNT];

static lv_obj_t *device_panel = NULL;
static lv_obj_t *device_label = NULL;
static lv_obj_t *device_value = NULL;
static lv_obj_t *device_glyph = NULL;
static int has_device_item = 0;

static char ctrl_guid[64] = "";
static char ctrl_name[128] = "";
static int remap_dev_idx = 0;

static volatile int capture_active = 0;
static int capture_target = -1;
static int capture_pending_clear = 0;
static char capture_saved_phys[32];
static int capture_saved_modified = 0;

#define AXIS_CAPTURE_THRESHOLD 16384

typedef enum {
    SAVE_USER = 0,
    SAVE_MODERN,
    SAVE_RETRO,
    SAVE_CANCEL,
    SAVE_NOPE
} save_option_t;

static int save_mode = 0;
static int mapping_modified = 0;
static mux_dialogue save_dlg;

static void detect_device_at(int idx) {
    ctrl_guid[0] = '\0';
    ctrl_name[0] = '\0';

    if (idx == 0) {
        const char *bn = board_name();
        if (bn) snprintf(ctrl_name, sizeof(ctrl_name), "%s", bn);

        if (device.BOARD.SDL_MAP[0] != '\0') {
            const char *c1 = strchr(device.BOARD.SDL_MAP, ',');
            if (c1) {
                size_t gl = (size_t)(c1 - device.BOARD.SDL_MAP);
                if (gl > 0 && gl < sizeof(ctrl_guid)) {
                    memcpy(ctrl_guid, device.BOARD.SDL_MAP, gl);
                    ctrl_guid[gl] = '\0';
                }
            }
        }

        if (ctrl_guid[0] != '\0') return;
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_PumpEvents();
    SDL_JoystickUpdate();

    if (idx >= SDL_NumJoysticks()) return;

    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(idx);
    SDL_JoystickGetGUIDString(guid, ctrl_guid, sizeof(ctrl_guid));

    const char *name = SDL_JoystickNameForIndex(idx);
    if (name) snprintf(ctrl_name, sizeof(ctrl_name), "%s", name);
}

static void parse_gcdb_slot(const char *line, int idx) {
    char search[48];
    snprintf(search, sizeof(search), ",%s:", slot_defs[idx].gc_key);

    const char *p = strstr(line, search);
    if (!p) {
        snprintf(phys[idx], sizeof(phys[0]), "---");
        return;
    }
    p += strlen(search);

    const char *end = strchr(p, ',');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= sizeof(phys[0])) len = sizeof(phys[0]) - 1;

    memcpy(phys[idx], p, len);
    phys[idx][len] = '\0';

    if (phys[idx][0] == '\0') snprintf(phys[idx], sizeof(phys[0]), "---");
}

static void load_from_gcdb(const char *guid) {
    FILE *f = fopen("/usr/lib/gamecontrollerdb.txt", "r");
    if (!f) return;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        str_remchar(line, '\n');
        if (strncmp(line, guid, 32) == 0) {
            for (int i = 0; i < REMAP_SLOT_COUNT; i++) parse_gcdb_slot(line, i);
            fclose(f);

            return;
        }
    }
    fclose(f);
}

// Device 0 uses the board SDL_MAP; all others search the gcdb by GUID.
static void load_current_mapping(void) {
    for (int i = 0; i < REMAP_SLOT_COUNT; i++)
        snprintf(phys[i], sizeof(phys[0]), "---");

    if (remap_dev_idx == 0 && device.BOARD.SDL_MAP[0]) {
        for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
            parse_gcdb_slot(device.BOARD.SDL_MAP, i);
        }

        return;
    }

    if (ctrl_guid[0]) load_from_gcdb(ctrl_guid);
}

static void build_mapping_line(char *buf) {
    char tmp[4096];
    int n = snprintf(tmp, sizeof(tmp), "%s,%s", ctrl_guid[0] ? ctrl_guid : "00000000000000000000000000000000", ctrl_name[0] ? ctrl_name : "muOS-Keys");

    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        if (phys[i][0] && strcmp(phys[i], "---") != 0) n += snprintf(tmp + n, sizeof(tmp) - (size_t) n, ",%s:%s", slot_defs[i].gc_key, phys[i]);
    }

    snprintf(tmp + n, sizeof(tmp) - (size_t) n, ",platform:Linux,");
    snprintf(buf, 4096, "%s", tmp);
}

static void save_to_file(const char *path, int append_mode) {
    char mapping[4096];
    build_mapping_line(mapping);

    if (append_mode) {
        const char *args[] = {
                OPT_PATH "script/mux/sdl_remap.sh",
                path,
                mapping,
                NULL
        };

        run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
    } else {
        FILE *f = fopen(path, "w");
        if (!f) {
            LOG_ERROR(mux_module, "Failed to write remap to %s", path);
            return;
        }

        fprintf(f, "# muOS user input remap\n%s\n", mapping);
        fclose(f);

        LOG_SUCCESS(mux_module, "User remap saved to %s", path);
    }

    SDL_GameControllerAddMappingsFromFile(OPT_PATH "share/info/gamecontrollerdb/user.txt");
}

static void do_save(save_option_t opt) {
    switch (opt) {
        case SAVE_USER:
            save_to_file(OPT_PATH "share/info/gamecontrollerdb/user.txt", 0);
            break;
        case SAVE_MODERN:
            save_to_file(OPT_PATH "share/info/gamecontrollerdb/modern.txt", 1);
            break;
        case SAVE_RETRO:
            save_to_file(OPT_PATH "share/info/gamecontrollerdb/retro.txt", 1);
            break;
        default:
            break;
    }
}

static void refresh_slot_labels(void) {
    for (int i = 0; i < REMAP_SLOT_COUNT; i++)
        lv_label_set_text(item_values[i], phys[i]);
}

static void update_nav_b(void) {
    lv_label_set_text(ui_lblNavB, mapping_modified ? lang.GENERIC.SAVE : lang.GENERIC.BACK);
}

static void cycle_to_next_device(void) {
    SDL_PumpEvents();
    SDL_JoystickUpdate();

    int num = SDL_NumJoysticks();
    if (num <= 1) return;

    remap_dev_idx = (remap_dev_idx + 1) % num;
    detect_device_at(remap_dev_idx);

    load_current_mapping();
    refresh_slot_labels();

    mapping_modified = 0;
    update_nav_b();

    if (device_value) lv_label_set_text(device_value, ctrl_name[0] ? ctrl_name : lang.GENERIC.UNKNOWN);

    lv_label_set_text(ui_lblTitle, ctrl_name[0] ? ctrl_name : lang.MUXREMAP.TITLE);

    play_sound(SND_NAVIGATE);
}

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

static void raw_event_capture(const SDL_Event *ev) {
    if (capture_pending_clear) {
        capture_active = 0;
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
            int16_t val = ev->jaxis.value;
            if (val < -AXIS_CAPTURE_THRESHOLD || val > AXIS_CAPTURE_THRESHOLD) {
                int axis = ev->jaxis.axis;
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

    snprintf(phys[capture_target], sizeof(phys[0]), "%s", phys_str);
    lv_label_set_text(item_values[capture_target], phys[capture_target]);

    mapping_modified = 1;
    capture_pending_clear = 1;
    update_nav_b();
}

static int focused_slot(void) {
    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return REMAP_DEVICE_ITEM;

    return (int) (intptr_t) lv_obj_get_user_data(panel);
}

static void handle_a(void) {
    if (hold_call) return;

    if (save_mode) {
        if (save_dlg.selected == SAVE_CANCEL) {
            hide_save_dialog();
            play_sound(SND_BACK);

            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "remap");
            mux_input_stop();
        } else {
            do_save((save_option_t) save_dlg.selected);
            mapping_modified = 0;

            hide_save_dialog();
            play_sound(SND_CONFIRM);

            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "remap");
            mux_input_stop();
        }
        return;
    }

    if (capture_active) return;

    int slot = focused_slot();
    if (slot < 0 || slot >= REMAP_SLOT_COUNT) return;

    capture_target = slot;
    snprintf(capture_saved_phys, sizeof(capture_saved_phys), "%s", phys[slot]);

    capture_saved_modified = mapping_modified;
    capture_active = 1;

    lv_label_set_text(item_values[slot], lang.MUXREMAP.WAITING);
    play_sound(SND_CONFIRM);
}

static void handle_b(void) {
    if (hold_call) return;

    if (capture_pending_clear) {
        snprintf(phys[capture_target], sizeof(phys[0]), "%s", capture_saved_phys);

        lv_label_set_text(item_values[capture_target], phys[capture_target]);
        mapping_modified = capture_saved_modified;

        update_nav_b();

        capture_target = -1;
        capture_pending_clear = 0;

        return;
    }

    if (capture_active) {
        capture_active = 0;
        lv_label_set_text(item_values[capture_target], phys[capture_target]);
        capture_target = -1;

        return;
    }

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (mapping_modified) {
        play_sound(SND_INFO_OPEN);
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "remap");
    mux_input_stop();
}

static void handle_x(void) {
    if (hold_call || capture_active || save_mode) return;

    int slot = focused_slot();
    if (slot < 0 || slot >= REMAP_SLOT_COUNT) return;

    play_sound(SND_INFO_CLOSE);

    snprintf(phys[slot], sizeof(phys[0]), "---");
    lv_label_set_text(item_values[slot], phys[slot]);
    mapping_modified = 1;

    update_nav_b();
}

static void handle_y(void) {
    if (hold_call || capture_active || save_mode) return;

    play_sound(SND_INFO_CLOSE);

    load_current_mapping();
    refresh_slot_labels();
    mapping_modified = 0;

    update_nav_b();
}

static void handle_dpad_left(void) {
    if (capture_active || save_mode) return;
    if (focused_slot() == REMAP_DEVICE_ITEM) cycle_to_next_device();
}

static void handle_dpad_right(void) {
    if (capture_active || save_mode) return;
    if (focused_slot() == REMAP_DEVICE_ITEM) cycle_to_next_device();
}

static void handle_dpad_up(void) {
    if (capture_active) return;

    if (save_mode) {
        dialogue_navigate(&save_dlg, &theme, -1);
        play_sound(SND_NAVIGATE);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (capture_active) return;

    if (save_mode) {
        dialogue_navigate(&save_dlg, &theme, +1);
        play_sound(SND_NAVIGATE);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (capture_active || save_mode) return;
    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (capture_active || save_mode) return;
    handle_list_nav_down_hold();
}

static void handle_page_up(void) {
    if (capture_active || save_mode) return;
    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (capture_active || save_mode) return;
    handle_list_nav_page_down();
}

static void list_nav_prev(int steps) {
    gen_step_movement(steps, -1, false, 0);
}

static void list_nav_next(int steps) {
    gen_step_movement(steps, +1, false, 0);
}

static void init_navigation_group(void) {
    reset_ui_groups();
    has_device_item = 1;

    {
        device_panel = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(device_panel);

        device_label = lv_label_create(device_panel);
        apply_theme_list_item(&theme, device_label, lang.MUXREMAP.INPUT_LABEL);

        device_value = lv_label_create(device_panel);
        apply_theme_list_value(&theme, device_value, ctrl_name[0] ? ctrl_name : lang.GENERIC.UNKNOWN);

        device_glyph = lv_img_create(device_panel);
        apply_theme_list_glyph(&theme, device_glyph, mux_module, "input");

        lv_group_add_obj(ui_group, device_label);
        lv_group_add_obj(ui_group_value, device_value);
        lv_group_add_obj(ui_group_glyph, device_glyph);
        lv_group_add_obj(ui_group_panel, device_panel);

        lv_obj_set_user_data(device_panel, (void *) (intptr_t) REMAP_DEVICE_ITEM);

        apply_size_to_content(&theme, ui_pnlContent, device_label, device_glyph, lang.MUXREMAP.INPUT_LABEL);
        apply_text_long_dot(&theme, ui_pnlContent, device_label);
    }

    for (int i = 0; i < REMAP_SLOT_COUNT; i++) {
        lv_obj_t *panel = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(panel);

        lv_obj_t *label = lv_label_create(panel);
        apply_theme_list_item(&theme, label, slot_defs[i].label);

        lv_obj_t *value = lv_label_create(panel);
        apply_theme_list_value(&theme, value, phys[i]);

        lv_obj_t *glyph = lv_img_create(panel);
        apply_theme_list_glyph(&theme, glyph, mux_module, "input");

        lv_group_add_obj(ui_group, label);
        lv_group_add_obj(ui_group_value, value);
        lv_group_add_obj(ui_group_glyph, glyph);
        lv_group_add_obj(ui_group_panel, panel);

        lv_obj_set_user_data(panel, (void *) (intptr_t) i);

        apply_size_to_content(&theme, ui_pnlContent, label, glyph, slot_defs[i].label);
        apply_text_long_dot(&theme, ui_pnlContent, label);

        item_panels[i] = panel;
        item_labels[i] = label;
        item_values[i] = value;
        item_glyphs[i] = glyph;
    }

    ui_count = REMAP_SLOT_COUNT + (has_device_item ? 1 : 0);
    lv_obj_update_layout(ui_pnlContent);
    list_nav_next(0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  0},
            {ui_lblNavX,      lang.GENERIC.CLEAR,  0},
            {ui_lblNavYGlyph, "",                  0},
            {ui_lblNavY,      lang.GENERIC.RESET,  0},
            {NULL, NULL,                           0}
    });

    lv_label_set_text(ui_lblScreenMessage, "");

    overlay_display();

    const char *save_opts[SAVE_NOPE] = {
            lang.MUXREMAP.SAVE_USER,
            lang.MUXREMAP.SAVE_MODERN,
            lang.MUXREMAP.SAVE_RETRO,
            lang.MUXREMAP.SAVE_CANCEL,
    };

    dialogue_init(&save_dlg, &theme, ui_screen, lang.MUXREMAP.SAVE_TITLE, save_opts, SAVE_NOPE, lang.GENERIC.SELECT, lang.GENERIC.BACK);
}

int muxremap_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    mapping_modified = 0;
    save_mode = 0;
    capture_active = 0;
    capture_target = -1;
    capture_pending_clear = 0;

    init_ui_common_screen(&theme, &device, &lang, lang.MUXREMAP.TITLE);

    remap_dev_idx = 0;
    detect_device_at(0);
    load_current_mapping();

    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A]          = handle_a,
                    [MUX_INPUT_B]          = handle_b,
                    [MUX_INPUT_X]          = handle_x,
                    [MUX_INPUT_Y]          = handle_y,
                    [MUX_INPUT_DPAD_LEFT]  = handle_dpad_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_dpad_right,
                    [MUX_INPUT_DPAD_UP]    = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN]  = handle_dpad_down,
                    [MUX_INPUT_L1]         = handle_page_up,
                    [MUX_INPUT_R1]         = handle_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP]   = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1]        = handle_page_up,
                    [MUX_INPUT_L2]        = hold_call_set,
                    [MUX_INPUT_R1]        = handle_page_down,
            },
            .raw_event_handler = raw_event_capture,
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
