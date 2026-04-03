#include "muxshare.h"
#include "ui/ui_muxoption.h"

#define OPTION(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(OPTION_ELEMENTS)
};
#undef OPTION

typedef enum {
    VIEW_OPTIONS = 0,
    VIEW_INFO
} option_view_t;

static option_view_t current_view = VIEW_OPTIONS;

static lv_obj_t *ui_pnlInfoView;
static lv_obj_t *ui_pnlOptionsView;

static char rom_name[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char rom_system[MAX_BUFFER_SIZE];

static int is_dir = 0;
static char *curr_dir = "";
static char *core_file = "";

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];

static int rem_config = 0;
static int options_item_index = 0;
static int info_item_index = 0;
static int info_has_init = 0;

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    struct help_msg help_messages[] = {
#define OPTION(NAME, ENUM, UDATA) { UDATA, lang.MUXOPTION.HELP.ENUM },
            OPTION_ELEMENTS
#undef OPTION
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int get_info_base_index(void) {
    return is_dir ? 6 : 7; // Don't you even think about it... https://youtu.be/YBCgmi4SItA
}

static int visible_control(void) {
    return !lv_obj_has_flag(ui_pnlControl_option, LV_OBJ_FLAG_HIDDEN);
}

static int visible_retroarch(void) {
    return !lv_obj_has_flag(ui_pnlRetroArch_option, LV_OBJ_FLAG_HIDDEN);
}

static int visible_remconfig(void) {
    return !lv_obj_has_flag(ui_pnlRemConfig_option, LV_OBJ_FLAG_HIDDEN);
}

static int visible_tag(void) {
    return !lv_obj_has_flag(ui_pnlTag_option, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_option_row_widths(void) {
#define OPTION(NAME, ENUM, UDATA) \
    OPTION_APPLY_WIDTH(NAME);     \
    OPTION_APPLY_LONG(NAME);

    OPTION_ELEMENTS
#undef OPTION

    OPTION_APPLY_WIDTH(Storage);
    OPTION_APPLY_WIDTH(Folder);

    OPTION_APPLY_LONG(Storage);
    OPTION_APPLY_LONG(Folder);

    if (!is_dir) {
        OPTION_APPLY_WIDTH(Name);
        OPTION_APPLY_WIDTH(Time);
        OPTION_APPLY_WIDTH(Launch);

        OPTION_APPLY_LONG(Name);
        OPTION_APPLY_LONG(Time);
        OPTION_APPLY_LONG(Launch);
    }
}

static void create_view_containers(void) {
    ui_pnlOptionsView = lv_obj_create(ui_pnlContent);
    lv_obj_remove_style_all(ui_pnlOptionsView);
    lv_obj_set_size(ui_pnlOptionsView, lv_pct(100), lv_pct(100));
    lv_obj_align(ui_pnlOptionsView, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(ui_pnlOptionsView, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_pnlOptionsView, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlOptionsView, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlOptionsView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlOptionsView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlOptionsView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlOptionsView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_layout(ui_pnlOptionsView, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ui_pnlOptionsView, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_pnlOptionsView, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    ui_pnlInfoView = lv_obj_create(ui_pnlContent);
    lv_obj_remove_style_all(ui_pnlInfoView);
    lv_obj_set_size(ui_pnlInfoView, lv_pct(100), lv_pct(100));
    lv_obj_align(ui_pnlInfoView, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(ui_pnlInfoView, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_pnlInfoView, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlInfoView, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlInfoView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlInfoView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlInfoView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlInfoView, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_layout(ui_pnlInfoView, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ui_pnlInfoView, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_pnlInfoView, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

static void add_info_item_type(lv_obj_t *ui_lblItemValue, const char *get_file, const char *get_dir, const char *opt_type, bool cap_label) {
    const char *value = get_file;
    if (!*value) value = get_dir;

    bool is_cfg = strcmp(opt_type, "core") == 0;
    bool is_gov = strcmp(opt_type, "governor") == 0;
    bool is_con = strcmp(opt_type, "control") == 0;
    bool is_tag = strcmp(opt_type, "tag") == 0;
    bool is_flt = strcmp(opt_type, "filter") == 0;
    bool is_rac = strcmp(opt_type, "retroarch") == 0;

    if (!*value) {
        value = is_cfg ? lang.MUXOPTION.NOT_ASSIGNED :
                is_gov ? device.CPU.DEFAULT :
                is_con ? lang.MUXOPTION.NONE :
                is_tag ? lang.MUXOPTION.NOT_ASSIGNED :
                is_flt ? lang.MUXOPTION.NONE :
                is_rac ? lang.GENERIC.DISABLED :
                lang.GENERIC.UNKNOWN;
    }

    char cap_value[MAX_BUFFER_SIZE];
    if (is_cfg) {
        if (strcmp(value, lang.MUXOPTION.NOT_ASSIGNED) == 0) {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.MUXOPTION.NOT_ASSIGNED);
        } else {
            snprintf(cap_value, sizeof(cap_value), "%s", format_core_name(value, 1));
        }
    } else if (is_rac) {
        if (strcmp(value, "false") == 0) {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.GENERIC.DISABLED);
        } else {
            snprintf(cap_value, sizeof(cap_value), "%s", lang.GENERIC.ENABLED);
        }
    } else if (is_flt) {
        snprintf(cap_value, sizeof(cap_value), "%s", str_replace(value, "_", " "));
    } else {
        snprintf(cap_value, sizeof(cap_value), "%s", value);
    }

    apply_theme_list_value(&theme, ui_lblItemValue, cap_label ? str_capital_all(cap_value) : cap_value);
}

static void add_info_items(void) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", rom_dir, rom_name);

    char *sys_dir = get_content_path(file_path);
    char *file_name = get_file_name(file_path);

    core_file = get_content_line(sys_dir, file_name, "cfg", 2);
    const char *core_dir = get_content_line(sys_dir, NULL, "cfg", 1);
    add_info_item_type(ui_lblCoreValue_option, core_file, core_dir, "core", false);

    const char *gov_file = get_content_line(sys_dir, file_name, "gov", 1);
    const char *gov_dir = get_content_line(sys_dir, NULL, "gov", 1);
    add_info_item_type(ui_lblGovernorValue_option, gov_file, gov_dir, "governor", true);

    const char *control_file = get_content_line(sys_dir, file_name, "con", 1);
    const char *control_dir = get_content_line(sys_dir, NULL, "con", 1);
    add_info_item_type(ui_lblControlValue_option, control_file, control_dir, "control", true);

    const char *rac_file = get_content_line(sys_dir, file_name, "rac", 1);
    const char *rac_dir = get_content_line(sys_dir, NULL, "rac", 1);
    add_info_item_type(ui_lblRetroArchValue_option, rac_file, rac_dir, "retroarch", true);

    const char *flt_file = get_content_line(sys_dir, file_name, "flt", 1);
    const char *flt_dir = get_content_line(sys_dir, NULL, "flt", 1);
    add_info_item_type(ui_lblColFilterValue_option, flt_file, flt_dir, "filter", true);

    if (!is_dir) {
        const char *tag_file = get_content_line(sys_dir, file_name, "tag", 1);
        const char *tag_dir = get_content_line(sys_dir, NULL, "tag", 1);
        add_info_item_type(ui_lblTagValue_option, tag_file, tag_dir, "tag", true);
    }
}

static struct json get_playtime_json(void) {
    static char *json_str = NULL;
    static struct json root = {0};
    static int loaded = 0;

    if (!loaded) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INFO_ACT_PATH "/" PLAYTIME_DATA);

        if (!file_exist(path)) return (struct json) {0};

        json_str = read_all_char_from(path);
        if (!json_str || !json_valid(json_str)) {
            free(json_str);
            json_str = NULL;
            return (struct json) {0};
        }

        root = json_parse(json_str);
        loaded = 1;
    }

    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", rom_dir, rom_name);

    return json_object_get(root, fullpath);
}

static char *get_time_played(void) {
    struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return lang.GENERIC.UNKNOWN;

    static char time_buffer[MAX_BUFFER_SIZE] = "0m";
    int total_time = json_int(json_object_get(playtime_json, "total_time"));

    int days = total_time / 86400;
    int hours = (total_time % 86400) / 3600;
    int minutes = (total_time % 3600) / 60;

    if (days > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dd %dh %dm",
                 days, hours, minutes);
    } else if (hours > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dh %dm",
                 hours, minutes);
    } else if (minutes > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dm",
                 minutes);
    } else {
        snprintf(time_buffer, sizeof(time_buffer), "0m");
    }

    return time_buffer;
}

static char *get_launch_count(void) {
    struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return "0";

    static char launch_count[MAX_BUFFER_SIZE];
    snprintf(launch_count, sizeof(launch_count), "%d",
             json_int(json_object_get(playtime_json, "launches")));

    return launch_count;
}

static void populate_info_values(void) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", rom_dir, rom_name);

    char *sys_dir = get_content_path(file_path);
    char *file_name = get_file_name(file_path);

    core_file = get_content_line(sys_dir, file_name, "cfg", 2);
    const char *core_dir = get_content_line(sys_dir, NULL, "cfg", 1);
    add_info_item_type(ui_lblCoreValue_option, core_file, core_dir, "core", false);

    const char *gov_file = get_content_line(sys_dir, file_name, "gov", 1);
    const char *gov_dir = get_content_line(sys_dir, NULL, "gov", 1);
    add_info_item_type(ui_lblGovernorValue_option, gov_file, gov_dir, "governor", true);

    const char *control_file = get_content_line(sys_dir, file_name, "con", 1);
    const char *control_dir = get_content_line(sys_dir, NULL, "con", 1);
    add_info_item_type(ui_lblControlValue_option, control_file, control_dir, "control", true);

    const char *rac_file = get_content_line(sys_dir, file_name, "rac", 1);
    const char *rac_dir = get_content_line(sys_dir, NULL, "rac", 1);
    add_info_item_type(ui_lblRetroArchValue_option, rac_file, rac_dir, "retroarch", true);

    const char *flt_file = get_content_line(sys_dir, file_name, "flt", 1);
    const char *flt_dir = get_content_line(sys_dir, NULL, "flt", 1);
    add_info_item_type(ui_lblColFilterValue_option, flt_file, flt_dir, "filter", true);

    if (!is_dir) {
        const char *tag_file = get_content_line(sys_dir, file_name, "tag", 1);
        const char *tag_dir = get_content_line(sys_dir, NULL, "tag", 1);
        add_info_item_type(ui_lblTagValue_option, tag_file, tag_dir, "tag", true);
    }

    lv_label_set_text(ui_lblStorageValue_option, get_storage_label(rom_dir));

    char rel_path[PATH_MAX];
    union_get_relative_path(rom_dir, rel_path, sizeof(rel_path));

    if (strncasecmp(rel_path, "ROMS", 4) == 0) {
        char *p = rel_path + 4;
        while (*p == '/') p++;
        memmove(rel_path, p, strlen(p) + 1);
    }

    curr_dir = get_last_subdir(strip_dir(file_path), '/', rel_path[0] ? 4 : 3);
    lv_label_set_text(ui_lblFolderValue_option, curr_dir);

    if (!is_dir) {
        char friendly_name[MAX_BUFFER_SIZE];
        resolve_friendly_name(sys_dir, strip_ext(file_name), friendly_name);
        adjust_visual_label(friendly_name, config.VISUAL.NAME, config.VISUAL.DASH);

        lv_label_set_text(ui_lblNameValue_option, friendly_name);
        lv_label_set_text(ui_lblTimeValue_option, get_time_played());
        lv_label_set_text(ui_lblLaunchValue_option, get_launch_count());
    }
}

static void build_all_items(void) {
    memset(ui_objects, 0, sizeof(ui_objects));
    memset(ui_objects_panel, 0, sizeof(ui_objects_panel));
    memset(ui_objects_glyph, 0, sizeof(ui_objects_glyph));
    memset(ui_objects_value, 0, sizeof(ui_objects_value));

    lv_obj_clean(ui_pnlOptionsView);
    init_muxoption(ui_pnlOptionsView);

#define OPTION(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_option, UDATA);
    OPTION_ELEMENTS
#undef OPTION

    INIT_VALUE_ITEM(-1, option, Core, lang.MUXOPTION.CORE, "core", "");
    INIT_VALUE_ITEM(-1, option, Governor, lang.MUXOPTION.GOVERNOR, "governor", "");
    INIT_VALUE_ITEM(-1, option, Control, lang.MUXOPTION.CONTROL, "control", "");
    INIT_VALUE_ITEM(-1, option, RetroArch, lang.MUXOPTION.RETROARCH, "retroarch", "");
    INIT_VALUE_ITEM(-1, option, RemConfig, lang.MUXOPTION.REMCONFIG, "remconfig", is_dir ? lang.GENERIC.DIRECTORY : lang.GENERIC.CONTENT);
    INIT_VALUE_ITEM(-1, option, ColFilter, lang.MUXOPTION.COLFILTER, "colfilter", "");
    if (!is_dir) INIT_VALUE_ITEM(-1, option, Tag, lang.MUXOPTION.TAG, "tag", "");

    INIT_VALUE_ITEM(-1, option, Storage, lang.GENERIC.STORAGE, "storage", "");
    INIT_VALUE_ITEM(-1, option, Folder, lang.MUXOPTION.FOLDER, "folder", "");

    if (!is_dir) {
        INIT_VALUE_ITEM(-1, option, Name, lang.MUXOPTION.NAME, "name", "");
        INIT_VALUE_ITEM(-1, option, Time, lang.MUXOPTION.TIME, "time", "");
        INIT_VALUE_ITEM(-1, option, Launch, lang.MUXOPTION.LAUNCH, "launch", "");
    }

    lv_obj_set_parent(ui_pnlStorage_option, ui_pnlInfoView);
    lv_obj_set_parent(ui_pnlFolder_option, ui_pnlInfoView);
    lv_obj_set_parent(ui_pnlName_option, ui_pnlInfoView);
    lv_obj_set_parent(ui_pnlTime_option, ui_pnlInfoView);
    lv_obj_set_parent(ui_pnlLaunch_option, ui_pnlInfoView);

    lv_obj_add_flag(ui_pnlStorage_option, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlFolder_option, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlName_option, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlTime_option, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlLaunch_option, LV_OBJ_FLAG_HIDDEN);

    populate_info_values();

    lv_obj_update_layout(ui_pnlContent);
    refresh_option_row_widths();

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    lv_obj_add_flag(ui_pnlInfoView, LV_OBJ_FLAG_HIDDEN);
}

static void build_options_view(void) {
#define OPTION(NAME, ENUM, UDATA) OPTION_SHOW(NAME);
    OPTION_ELEMENTS
#undef OPTION

    OPTION_HIDE(Storage);
    OPTION_HIDE(Folder);
    OPTION_HIDE(Name);
    OPTION_HIDE(Time);
    OPTION_HIDE(Launch);

    lv_obj_update_layout(ui_pnlContent);
    refresh_option_row_widths();

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    const char *core_label = lv_label_get_text(ui_lblCoreValue_option);
    if (core_label && !strcasestr(core_label, "RetroArch")) {
        HIDE_VALUE_ITEM(option, RetroArch);
        HIDE_VALUE_ITEM(option, RemConfig);
    }

    if (hdmi_mode) {
        HIDE_VALUE_ITEM(option, ColFilter);
    }
}

static void build_info_view(void) {
#define OPTION(NAME, ENUM, UDATA) OPTION_HIDE(NAME);
    OPTION_ELEMENTS
#undef OPTION

    OPTION_SHOW(Storage);
    OPTION_SHOW(Folder);

    if (!is_dir) {
        OPTION_SHOW(Name);
        OPTION_SHOW(Time);
        OPTION_SHOW(Launch);
    }

    populate_info_values();

    lv_obj_update_layout(ui_pnlContent);
    refresh_option_row_widths();

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
}

static void check_focus(void) {
    if (current_view != VIEW_OPTIONS) {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        return;
    }

    lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);

    struct _lv_obj_t *focused_element = lv_group_get_focused(ui_group);
    if (focused_element == ui_lblRemConfig_option) {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void refresh_option_view(void) {
    int restore_index = 0;
    int anchor_index = 0;

    lv_group_remove_all_objs(ui_group);
    lv_group_remove_all_objs(ui_group_value);
    lv_group_remove_all_objs(ui_group_glyph);
    lv_group_remove_all_objs(ui_group_panel);

    if (current_view == VIEW_OPTIONS) {
        lv_label_set_text(ui_lblTitle, lang.MUXOPTION.TITLE_MAIN);

        lv_obj_clear_flag(ui_pnlOptionsView, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlInfoView, LV_OBJ_FLAG_HIDDEN);

        build_options_view();

        lv_label_set_text(ui_lblNavY, lang.GENERIC.INFO);

        restore_index = options_item_index;
        anchor_index = 0;
    } else {
        lv_label_set_text(ui_lblTitle, lang.MUXOPTION.TITLE_INFO);

        lv_obj_add_flag(ui_pnlOptionsView, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlInfoView, LV_OBJ_FLAG_HIDDEN);

        build_info_view();

        restore_index = info_item_index;
        anchor_index = get_info_base_index();
    }

    lv_obj_update_layout(ui_pnlContent);

    if (!ui_count) {
        current_item_index = 0;
        check_focus();
        return;
    }

    if (current_view == VIEW_OPTIONS) {
        int option_count = get_info_base_index();
        if (restore_index < 0) restore_index = 0;
        if (restore_index >= option_count) restore_index = option_count - 1;
    } else {
        int info_count = is_dir ? 2 : 5;
        if (restore_index < 0) restore_index = 0;
        if (restore_index >= info_count) restore_index = info_count - 1;
    }

    nav_silent = 1;

    if (ui_objects[anchor_index]) lv_group_focus_obj(ui_objects[anchor_index]);
    if (ui_objects_value[anchor_index]) lv_group_focus_obj(ui_objects_value[anchor_index]);
    if (ui_objects_glyph[anchor_index]) lv_group_focus_obj(ui_objects_glyph[anchor_index]);
    if (ui_objects_panel[anchor_index]) lv_group_focus_obj(ui_objects_panel[anchor_index]);

    current_item_index = anchor_index;

    if (restore_index > 0) {
        list_nav_move(restore_index, +1);
    } else {
        if (lv_group_get_focused(ui_group)) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));
            set_label_long_mode(&theme, lv_group_get_focused(ui_group));
        }

        if (lv_group_get_focused(ui_group_value)) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group_value));
            set_label_long_mode(&theme, lv_group_get_focused(ui_group_value));
        }

        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
        update_label_scroll();
        nav_moved = 1;
    }

    nav_silent = 0;

    if (current_view == VIEW_INFO && !info_has_init) info_has_init = 1;
    check_focus();
}

static inline void focus_and_update(int target) {
    if (ui_objects[target]) lv_group_focus_obj(ui_objects[target]);
    if (ui_objects_value[target]) lv_group_focus_obj(ui_objects_value[target]);
    if (ui_objects_glyph[target]) lv_group_focus_obj(ui_objects_glyph[target]);
    if (ui_objects_panel[target]) lv_group_focus_obj(ui_objects_panel[target]);

    current_item_index = target;
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);

    struct _lv_obj_t *focused;

    focused = lv_group_get_focused(ui_group);
    if (focused) {
        apply_text_long_dot(&theme, ui_pnlContent, focused);
        set_label_long_mode(&theme, focused);
    }

    focused = lv_group_get_focused(ui_group_value);
    if (focused) {
        apply_text_long_dot(&theme, ui_pnlContent, focused);
        set_label_long_mode(&theme, focused);
    }

    update_label_scroll();
    nav_moved = 1;
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;

    if (first_open) {
        first_open = 0;
    } else if (!nav_silent) {
        play_sound(SND_NAVIGATE);
    }

    int base_index = (current_view == VIEW_INFO) ? get_info_base_index() : 0;
    int view_count = (current_view == VIEW_INFO) ? (is_dir ? 2 : 5) : get_info_base_index();

    for (int i = 0; i < steps; i++) {
        if (lv_group_get_focused(ui_group)) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));
        }

        if (lv_group_get_focused(ui_group_value)) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group_value));
        }

        if (current_view == VIEW_INFO) {
            int local = current_item_index - base_index;
            int wrap_target = -1;

            if (direction < 0 && local == 0) {
                wrap_target = base_index + (view_count - 1);
            } else if (direction > 0 && local == view_count - 1) {
                wrap_target = base_index;
            }

            if (wrap_target >= 0) {
                focus_and_update(wrap_target);
                continue;
            }
        }

        gen_step_movement(1, direction, false, 0);
    }

    if (!nav_silent) {
        if (current_view == VIEW_OPTIONS) {
            options_item_index = current_item_index;
        } else {
            info_item_index = current_item_index - base_index;
            if (info_item_index < 0) info_item_index = 0;
        }
    }

    if (lv_group_get_focused(ui_group)) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));
        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    }

    if (lv_group_get_focused(ui_group_value)) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group_value));
        set_label_long_mode(&theme, lv_group_get_focused(ui_group_value));
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    update_label_scroll();
    nav_moved = 1;

    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static char *change_config_opt(int steps) {
    int max_opt = is_dir ? 1 : 2;
    rem_config += steps;

    if (rem_config > max_opt) rem_config = 0;
    if (rem_config < 0) rem_config = max_opt;

    char *remove_options_dir[] = {
            lang.MUXOPTION.FOLDER,
            lang.MUXOPTION.CORE
    };

    char *remove_options_all[] = {
            lang.GENERIC.CONTENT,
            lang.MUXOPTION.FOLDER,
            lang.MUXOPTION.CORE
    };

    return is_dir ? remove_options_dir[rem_config] : remove_options_all[rem_config];
}

static void handle_option_prev(void) {
    if (current_view != VIEW_OPTIONS || msgbox_active || hold_call) return;

    struct _lv_obj_t *f = lv_group_get_focused(ui_group);
    if (f == ui_lblRemConfig_option) {
        play_sound(SND_OPTION);
        lv_label_set_text(ui_lblRemConfigValue_option, change_config_opt(-1));
    }
}

static void handle_option_next(void) {
    if (current_view != VIEW_OPTIONS || msgbox_active || hold_call) return;

    struct _lv_obj_t *f = lv_group_get_focused(ui_group);
    if (f == ui_lblRemConfig_option) {
        play_sound(SND_OPTION);
        lv_label_set_text(ui_lblRemConfigValue_option, change_config_opt(+1));
    }
}

static void handle_a(void) {
    if (current_view != VIEW_OPTIONS || msgbox_active || hold_call) return;

    struct _lv_obj_t *f = lv_group_get_focused(ui_group);
    if (f == ui_lblRemConfig_option) return;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {"search",    &kiosk.CONTENT.SEARCH,    NULL},
            {"assign",    &kiosk.CONTENT.CORE,      NULL},
            {"governor",  &kiosk.CONTENT.GOVERNOR,  NULL},
            {"control",   &kiosk.CONTENT.CONTROL,   visible_control},
            {"retroarch", &kiosk.CONTENT.RETROARCH, visible_retroarch},
            {"remconfig", &kiosk.CONTENT.REMCONFIG, visible_remconfig},
            {"filter",    &kiosk.CONTENT.COLFILTER, NULL},
            {"tag",       &kiosk.CONTENT.TAG,       visible_tag},
    };

    const menu_entry *visible_entries[UI_COUNT];
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

    play_sound(SND_CONFIRM);
    load_mux(entry->mux_name);

    write_text_to_file(MUOS_OPI_LOAD, "w", INT, current_item_index);

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (current_view == VIEW_INFO) {
        play_sound(SND_BACK);

        info_item_index = current_item_index - get_info_base_index();
        if (info_item_index < 0) info_item_index = 0;

        current_view = VIEW_OPTIONS;
        refresh_option_view();
        return;
    }

    play_sound(SND_BACK);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    mux_input_stop();
}

static void handle_x(void) {
    if (current_view != VIEW_OPTIONS) return;

    struct _lv_obj_t *f = lv_group_get_focused(ui_group);
    if (f == ui_lblRemConfig_option) {
        if (!hold_call) {
            play_sound(SND_ERROR);
            toast_message(lang.GENERIC.HOLD_REMOVE, SHORT);
            return;
        }

        play_sound(SND_MUOS);

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
}

static void handle_y(void) {
    if (hold_call) return;

    if (current_view == VIEW_OPTIONS) {
        options_item_index = current_item_index;
        if (!info_has_init) info_item_index = 0;
    } else {
        info_item_index = current_item_index - get_info_base_index();
        if (info_item_index < 0) info_item_index = 0;
    }

    play_sound(SND_CONFIRM);

    current_view = (current_view == VIEW_OPTIONS) ? VIEW_INFO : VIEW_OPTIONS;
    refresh_option_view();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.REMOVE, 0},
            {ui_lblNavYGlyph,  "",                  0},
            {ui_lblNavY,       lang.GENERIC.INFO,   0},
            {NULL, NULL,                            0}
    });

    overlay_display();
}

int muxoption_main(int nothing, char *name, char *dir, char *sys, int app) {
    rem_config = 0;
    current_view = VIEW_OPTIONS;

    options_item_index = 0;
    info_item_index = 0;
    info_has_init = 0;

    char resolved_dir[PATH_MAX];
    if (!union_resolve_to_real(dir, resolved_dir, sizeof(resolved_dir))) {
        snprintf(resolved_dir, sizeof(resolved_dir), "%s", dir);
    }

    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", resolved_dir, name);
    is_dir = dir_exist(rom_dir);

    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", resolved_dir);

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    init_module(__func__);

    if (file_exist(OPTION_SKIP)) {
        remove(OPTION_SKIP);
        remove(MUOS_SYS_LOAD);
        LOG_INFO(mux_module, "Skipping Options Module - Not Required...");

        return 0;
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXOPTION.TITLE_MAIN);
    create_view_containers();
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    build_all_items();
    refresh_option_view();

    if (file_exist(MUOS_OPI_LOAD) && current_view == VIEW_OPTIONS) {
        list_nav_move(read_line_int_from(MUOS_OPI_LOAD, 1), +1);
        remove(MUOS_OPI_LOAD);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
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
