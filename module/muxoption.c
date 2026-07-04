#include "muxshare.h"
#include "ui/ui_muxoption.h"

#define OPTION(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(OPTION_ELEMENTS) };
#undef OPTION

typedef enum { view_options = 0, view_info } option_view_t;

static option_view_t current_view = view_options;

static char rom_name[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char rom_system[MAX_BUFFER_SIZE];

static int is_dir = 0;

static const char *curr_dir = "";
static const char *core_file = "";

static char *playtime_json_str = NULL;
static struct json playtime_json_root = {0};
static int playtime_json_loaded = 0;

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];

static int rem_config = 0;
static int options_item_index = 0;
static int info_item_index = 0;

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define OPTION(NAME, UDATA) {UDATA, lang.muxoption.help.NAME},
        OPTION_ELEMENTS
#undef OPTION
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

#define OPTION_VISIBLE(NAME, OBJ)                                                                                      \
    static int visible_##NAME(void) {                                                                                  \
        return !lv_obj_has_flag(OBJ, LV_OBJ_FLAG_HIDDEN);                                                              \
    }

OPTION_VISIBLE(control, ui_pnl_control_option)

OPTION_VISIBLE(retroarch, ui_pnl_retro_arch_option)

OPTION_VISIBLE(remconfig, ui_pnl_rem_config_option)

OPTION_VISIBLE(tag, ui_pnl_tag_option)

static void refresh_option_row_widths(void) {
#define OPTION(NAME, UDATA)                                                                                            \
    OPTION_APPLY_WIDTH(NAME);                                                                                          \
    OPTION_APPLY_LONG(NAME);

    OPTION_ELEMENTS
#undef OPTION

    OPTION_APPLY_WIDTH(storage);
    OPTION_APPLY_WIDTH(folder);

    OPTION_APPLY_LONG(storage);
    OPTION_APPLY_LONG(folder);

    if (!is_dir) {
        OPTION_APPLY_WIDTH(name);
        OPTION_APPLY_WIDTH(time);
        OPTION_APPLY_WIDTH(launch);

        OPTION_APPLY_LONG(name);
        OPTION_APPLY_LONG(time);
        OPTION_APPLY_LONG(launch);
    }
}

static void add_info_item_type(
    lv_obj_t *ui_lbl_item_value, const char *get_file, const char *get_dir, const char *opt_type, const int cap_label
) {
    const char *value = get_file;
    if (!*value) value = get_dir;

    const int is_cfg = strcmp(opt_type, "core") == 0;
    const int is_gov = strcmp(opt_type, "governor") == 0;
    const int is_con = strcmp(opt_type, "control") == 0;
    const int is_tag = strcmp(opt_type, "tag") == 0;
    const int is_flt = strcmp(opt_type, "filter") == 0;
    const int is_shd = strcmp(opt_type, "shader") == 0;
    const int is_rac = strcmp(opt_type, "retroarch") == 0;

    if (!*value) {
        value = is_cfg   ? lang.muxoption.not_assigned
                : is_gov ? device.cpu.dflt
                : is_con ? lang.muxoption.none
                : is_tag ? lang.muxoption.not_assigned
                : is_flt ? lang.muxoption.none
                : is_shd ? lang.muxoption.none
                : is_rac ? lang.generic.disabled
                         : lang.generic.unknown;
    }

    char cap_value[MAX_BUFFER_SIZE];
    if (is_cfg) {
        if (strcmp(value, lang.muxoption.not_assigned) == 0) {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.muxoption.not_assigned);
        } else {
            snprintf(cap_value, sizeof(cap_value), "%s", format_core_name(value, 1));
        }
    } else if (is_rac) {
        if (strcmp(value, "false") == 0) {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.generic.disabled);
        } else {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.generic.enabled);
        }
    } else if (is_shd) {
        if (strcmp(value, lang.muxoption.none) == 0) {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.muxoption.none);
        } else {
            char *meta_name = read_shader_info(value, "Name");

            if (meta_name && *meta_name) {
                snprintf(cap_value, sizeof(cap_value), "%s", meta_name);
            } else {
                snprintf(cap_value, sizeof(cap_value), "%s", str_replace(value, "_", " "));
            }

            if (meta_name) free(meta_name);
        }
    } else if (is_flt) {
        snprintf(cap_value, sizeof(cap_value), "%s", str_replace(value, "_", " "));
    } else {
        snprintf(cap_value, sizeof(cap_value), "%s", value);
    }

    apply_theme_list_value(&theme, ui_lbl_item_value, cap_label ? str_capital_all(cap_value) : cap_value);
}

static struct json get_playtime_json(void) {
    if (!playtime_json_loaded) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INFO_ACT_PATH "/" PLAYTIME_DATA);

        if (!file_exist(path)) return (struct json) {0};

        playtime_json_str = read_all_char_from(path);
        if (!playtime_json_str || !json_valid(playtime_json_str)) {
            free(playtime_json_str);
            playtime_json_str = NULL;
            return (struct json) {0};
        }

        playtime_json_root = json_parse(playtime_json_str);
        playtime_json_loaded = 1;
    }

    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", rom_dir, rom_name);

    return json_object_get(playtime_json_root, fullpath);
}

static char *get_time_played(void) {
    const struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return "0";

    static char time_buffer[MAX_BUFFER_SIZE] = "0m";
    const int total_time = json_int(json_object_get(playtime_json, "total_time"));

    const int days = total_time / 86400;
    const int hours = total_time % 86400 / 3600;
    const int minutes = total_time % 3600 / 60;

    if (days > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dd %dh %dm", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dh %dm", hours, minutes);
    } else if (minutes > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dm", minutes);
    } else {
        snprintf(time_buffer, sizeof(time_buffer), "0m");
    }

    return time_buffer;
}

static char *get_launch_count(void) {
    const struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return "0";

    static char launch_count[MAX_BUFFER_SIZE];
    snprintf(launch_count, sizeof(launch_count), "%d", json_int(json_object_get(playtime_json, "launches")));

    return launch_count;
}

static void populate_info_values(void) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", rom_dir, rom_name);

    char *sys_dir = get_content_path(file_path);
    const char *file_name = get_file_name(file_path);

    core_file = get_content_line(sys_dir, file_name, "cfg", 2);
    const char *core_dir = get_content_line(sys_dir, NULL, "cfg", 1);
    add_info_item_type(ui_val_core_option, core_file, core_dir, "core", 0);

    const char *gov_file = get_content_line(sys_dir, file_name, "gov", 1);
    const char *gov_dir = get_content_line(sys_dir, NULL, "gov", 1);
    add_info_item_type(ui_val_governor_option, gov_file, gov_dir, "governor", 1);

    const char *control_file = get_content_line(sys_dir, file_name, "con", 1);
    const char *control_dir = get_content_line(sys_dir, NULL, "con", 1);
    add_info_item_type(ui_val_control_option, control_file, control_dir, "control", 1);

    const char *rac_file = get_content_line(sys_dir, file_name, "rac", 1);
    const char *rac_dir = get_content_line(sys_dir, NULL, "rac", 1);
    add_info_item_type(ui_val_retro_arch_option, rac_file, rac_dir, "retroarch", 1);

    const char *flt_file = get_content_line(sys_dir, file_name, "flt", 1);
    const char *flt_dir = get_content_line(sys_dir, NULL, "flt", 1);
    add_info_item_type(ui_val_col_filter_option, flt_file, flt_dir, "filter", 1);

    const char *shd_file = get_content_line(sys_dir, file_name, "shd", 1);
    const char *shd_dir = get_content_line(sys_dir, NULL, "shd", 1);
    add_info_item_type(ui_val_shader_option, shd_file, shd_dir, "shader", 1);

    if (!is_dir) {
        const char *tag_file = get_content_line(sys_dir, file_name, "tag", 1);
        const char *tag_dir = get_content_line(sys_dir, NULL, "tag", 1);
        add_info_item_type(ui_val_tag_option, tag_file, tag_dir, "tag", 1);
    }

    lv_label_set_text(
        ui_val_storage_option,
        get_storage_label(rom_dir, lang.muxoption.primary, lang.muxoption.secondary, lang.muxoption.external)
    );

    char rel_path[PATH_MAX];
    union_get_relative_path(rom_dir, rel_path, sizeof(rel_path));

    if (strncasecmp(rel_path, MAIN_ROM_DIR, 4) == 0) {
        const char *p = rel_path + 4;
        while (*p == '/')
            p++;
        memmove(rel_path, p, strlen(p) + 1);
    }

    curr_dir = get_last_subdir(strip_dir(file_path), '/', rel_path[0] ? 4 : 3);
    lv_label_set_text(ui_val_folder_option, curr_dir);

    if (!is_dir) {
        char friendly_name[MAX_BUFFER_SIZE];
        resolve_friendly_name(file_path, friendly_name);
        adjust_visual_label(friendly_name, config.visual.name, config.visual.dash);

        lv_label_set_text(ui_val_name_option, friendly_name);
        lv_label_set_text(ui_val_time_option, get_time_played());
        lv_label_set_text(ui_val_launch_option, get_launch_count());
    }
}

static void rebuild_ui_groups(void) {
    lv_obj_update_layout(ui_pnl_content);
    refresh_option_row_widths();
    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void build_all_items(void) {
    memset(ui_objects, 0, sizeof(ui_objects));
    memset(ui_objects_panel, 0, sizeof(ui_objects_panel));
    memset(ui_objects_glyph, 0, sizeof(ui_objects_glyph));
    memset(ui_objects_value, 0, sizeof(ui_objects_value));

    init_muxoption(ui_pnl_content);

#define OPTION(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_option, UDATA);
    OPTION_ELEMENTS
#undef OPTION

    INIT_VALUE_ITEM(-1, option, core, lang.muxoption.core, "core", "");
    INIT_VALUE_ITEM(-1, option, governor, lang.muxoption.governor, "governor", "");
    INIT_VALUE_ITEM(-1, option, control, lang.muxoption.control, "control", "");
    INIT_VALUE_ITEM(-1, option, retro_arch, lang.muxoption.retroarch, "retroarch", "");
    INIT_VALUE_ITEM(
        -1, option, rem_config, lang.muxoption.remconfig, "remconfig",
        is_dir ? lang.generic.directory : lang.generic.content
    );
    INIT_VALUE_ITEM(-1, option, col_filter, lang.muxoption.colfilter, "colfilter", "");
    INIT_VALUE_ITEM(-1, option, shader, lang.muxoption.shader, "shader", "");
    INIT_VALUE_ITEM(-1, option, tag, lang.muxoption.tag, "tag", "");

    INIT_VALUE_ITEM(-1, option, storage, lang.generic.storage, "storage", "");
    INIT_VALUE_ITEM(-1, option, folder, lang.muxoption.folder, "folder", "");

    INIT_VALUE_ITEM(-1, option, name, lang.muxoption.name, "name", "");
    INIT_VALUE_ITEM(-1, option, time, lang.muxoption.time, "time", "");
    INIT_VALUE_ITEM(-1, option, launch, lang.muxoption.launch, "launch", "");

    if (is_dir) {
        HIDE_VALUE_ITEM(option, tag);
        HIDE_VALUE_ITEM(option, name);
        HIDE_VALUE_ITEM(option, time);
        HIDE_VALUE_ITEM(option, launch);
    }

    populate_info_values();
    rebuild_ui_groups();
}

static void build_options_view(void) {
    SHOW_VALUE_ITEM(option, core);
    SHOW_VALUE_ITEM(option, governor);
    SHOW_VALUE_ITEM(option, control);
    SHOW_VALUE_ITEM(option, retro_arch);
    SHOW_VALUE_ITEM(option, rem_config);
    SHOW_VALUE_ITEM(option, col_filter);
    SHOW_VALUE_ITEM(option, shader);
    if (!is_dir) SHOW_VALUE_ITEM(option, tag);

    HIDE_VALUE_ITEM(option, storage);
    HIDE_VALUE_ITEM(option, folder);
    if (!is_dir) {
        HIDE_VALUE_ITEM(option, name);
        HIDE_VALUE_ITEM(option, time);
        HIDE_VALUE_ITEM(option, launch);
    }

    const char *core_label = lv_label_get_text(ui_val_core_option);
    if (core_label && !strcasestr(core_label, "RetroArch")) {
        HIDE_VALUE_ITEM(option, retro_arch);
        HIDE_VALUE_ITEM(option, rem_config);
    }

    if (hdmi_mode) {
        HIDE_VALUE_ITEM(option, col_filter);
        HIDE_VALUE_ITEM(option, shader);
    }
}

static void build_info_view(void) {
    SHOW_VALUE_ITEM(option, storage);
    SHOW_VALUE_ITEM(option, folder);

    if (!is_dir) {
        SHOW_VALUE_ITEM(option, name);
        SHOW_VALUE_ITEM(option, time);
        SHOW_VALUE_ITEM(option, launch);
    }

    HIDE_VALUE_ITEM(option, core);
    HIDE_VALUE_ITEM(option, governor);
    HIDE_VALUE_ITEM(option, control);
    HIDE_VALUE_ITEM(option, retro_arch);
    HIDE_VALUE_ITEM(option, rem_config);
    HIDE_VALUE_ITEM(option, col_filter);
    HIDE_VALUE_ITEM(option, shader);
    if (!is_dir) HIDE_VALUE_ITEM(option, tag);
}

static void check_focus(void) {
    if (current_view != view_options) {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        return;
    }

    lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_rem_config_option) {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void refresh_option_view(void) {
    int anchor_index = 0;

    if (current_view == view_options) {
        lv_label_set_text(ui_lbl_title, lang.muxoption.title_main);

        build_options_view();

        lv_label_set_text(ui_lbl_nav_y, lang.generic.info);

        anchor_index = options_item_index;
        current_item_index = options_item_index;
    } else {
        lv_label_set_text(ui_lbl_title, lang.muxoption.title_info);

        build_info_view();

        anchor_index = 7 + info_item_index;
        current_item_index = info_item_index;
    }

    lv_obj_update_layout(ui_pnl_content);

    if (!ui_count_static) {
        current_item_index = 0;
        check_focus();
        return;
    }

    if (ui_objects[anchor_index]) lv_group_focus_obj(ui_objects[anchor_index]);
    if (ui_objects_value[anchor_index]) lv_group_focus_obj(ui_objects_value[anchor_index]);
    if (ui_objects_glyph[anchor_index]) lv_group_focus_obj(ui_objects_glyph[anchor_index]);
    if (ui_objects_panel[anchor_index]) lv_group_focus_obj(ui_objects_panel[anchor_index]);

    check_focus();
    nav_moved = 1;
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    if (first_open) first_open = 0;

    for (int i = 0; i < steps; i++) {
        if (lv_group_get_focused(ui_group)) {
            apply_text_long_dot(&theme, lv_group_get_focused(ui_group));
        }

        if (lv_group_get_focused(ui_group_value)) {
            apply_text_long_dot(&theme, lv_group_get_focused(ui_group_value));
        }
        gen_step_movement(1, direction, 0, 0, 1);
    }

    if (!nav_silent) {
        if (current_view == view_options) {
            options_item_index = current_item_index;
        } else {
            info_item_index = current_item_index;
        }
    }

    if (lv_group_get_focused(ui_group)) {
        apply_text_long_dot(&theme, lv_group_get_focused(ui_group));
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    }

    if (lv_group_get_focused(ui_group_value)) {
        apply_text_long_dot(&theme, lv_group_get_focused(ui_group_value));
        set_label_long_mode(&theme, lv_group_get_focused(ui_group_value), config.visual.name_scroll);
    }

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
    update_label_scroll();
    nav_moved = 1;

    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static int remove_mode = 0;
static mux_dialogue remove_dlg;

static void show_remove_dialog(void) {
    remove_mode = 1;
    remove_dlg.selected = 0;
    dialogue_show(&remove_dlg);
    dialogue_refresh(&remove_dlg, &theme);
}

static void hide_remove_dialog(void) {
    remove_mode = 0;
    dialogue_hide(&remove_dlg);
}

static void do_remove(void) {
    play_sound(snd_muos);

    switch (rem_config) {
        case 0:
            if (is_dir) {
                remove_dir_config(curr_dir, core_file);
            } else {
                remove_content_config(strip_ext(rom_name), core_file);
            }
            break;
        case 1:
            if (is_dir) {
                remove_core_config(core_file);
            } else {
                remove_dir_config(curr_dir, core_file);
            }
            break;
        case 2:
            remove_core_config(core_file);
            break;
        default:
            break;
    }

    refresh_option_view();
}

static char *change_config_opt(const int steps) {
    char *remove_options_dir[] = {lang.muxoption.folder, lang.muxoption.core};

    char *remove_options_all[] = {lang.generic.content, lang.muxoption.folder, lang.muxoption.core};

    char **opts = is_dir ? remove_options_dir : remove_options_all;
    const int max_opt = (int) (is_dir ? A_SIZE(remove_options_dir) : A_SIZE(remove_options_all)) - 1;

    rem_config += steps;
    if (rem_config > max_opt) rem_config = 0;
    if (rem_config < 0) rem_config = max_opt;

    return opts[rem_config];
}

static void handle_option_prev(void) {
    if (remove_mode) {
        if (swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (current_view != view_options || msgbox_active || hold_call) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_rem_config_option) {
        play_sound(snd_option);
        lv_label_set_text(ui_val_rem_config_option, change_config_opt(-1));
    }
}

static void handle_option_next(void) {
    if (remove_mode) {
        if (swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (current_view != view_options || msgbox_active || hold_call) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_rem_config_option) {
        play_sound(snd_option);
        lv_label_set_text(ui_val_rem_config_option, change_config_opt(+1));
    }
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (remove_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (remove_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (remove_mode) {
        const mux_confirm_opt opt = (mux_confirm_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == mux_confirm_yep) do_remove();
        return;
    }

    if (current_view != view_options || msgbox_active || hold_call) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_rem_config_option) return;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"assign", &kiosk.content.core, NULL},
        {"governor", &kiosk.content.governor, NULL},
        {"control", &kiosk.content.control, visible_control},
        {"retroarch", &kiosk.content.retroarch, visible_retroarch},
        {"remconfig", &kiosk.content.remconfig, visible_remconfig},
        {"filter", &kiosk.content.colfilter, NULL},
        {"shader", &kiosk.content.shader, NULL},
        {"tag", &kiosk.content.tag, visible_tag},
    };

    const menu_entry *visible_entries[ui_count_dynamic];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    play_sound(snd_confirm);
    load_mux(entry->mux_name);

    write_text_to_file(MUOS_OPI_LOAD, "w", INT, current_item_index);

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (remove_mode) {
        hide_remove_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (current_view == view_info) {
        play_sound(snd_back);

        current_view = view_options;

        refresh_option_view();
        return;
    }

    play_sound(snd_back);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    mux_input_stop();
}

static void handle_x(void) {
    if (current_view != view_options) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused != ui_lbl_rem_config_option || remove_mode) return;

    if (config.settings.advanced.trust_remove) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
    show_remove_dialog();
}

static void handle_y(void) {
    if (hold_call) return;

    play_sound(snd_confirm);

    current_view = current_view == view_options ? view_info : view_options;
    refresh_option_view();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.remove, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.info, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

void muxoption_main(const int auto_assign, const char *name, const char *dir, const char *sys, const int app) {
    (void) auto_assign;
    (void) app;

    nav_silent = 1;
    rem_config = 0;
    current_view = view_options;

    options_item_index = 0;
    info_item_index = 0;

    playtime_json_loaded = 0;
    free(playtime_json_str);
    playtime_json_str = NULL;
    playtime_json_root = (struct json) {0};

    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_dir = dir_exist(rom_dir);

    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    init_module(__func__);

    if (file_exist(OPTION_SKIP)) {
        remove(OPTION_SKIP);
        remove(MUOS_SYS_LOAD);
        LOG_INFO(mux_module, "Skipping Options Module - Not Required...");

        return;
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxoption.title_main);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    build_all_items();
    refresh_option_view();

    if (file_exist(MUOS_OPI_LOAD) && current_view == view_options) {
        list_nav_move(read_line_int_from(MUOS_OPI_LOAD, 1), +1);
        remove(MUOS_OPI_LOAD);
    }

    nav_silent = 0;

    dialogue_init_confirm(
        &remove_dlg, &theme, ui_screen, lang.generic.confirm, NULL, lang.generic.remove, lang.generic.cancel,
        lang.generic.select, lang.generic.back
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

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);
}
