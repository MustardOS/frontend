#include "muxshare.h"
#include "ui/ui_muxoption.h"

#define UI_COUNT 7

static char rom_name[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char rom_system[MAX_BUFFER_SIZE];

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];

static int group_index = 0;

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblSearch_option,   lang.MUXOPTION.HELP.SEARCH},
            {ui_lblCore_option,     lang.MUXOPTION.HELP.CORE},
            {ui_lblGovernor_option, lang.MUXOPTION.HELP.GOV},
            {ui_lblControl_option,  lang.MUXOPTION.HELP.CONTROL},
            {ui_lblTag_option,      lang.MUXOPTION.HELP.TAG},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void add_static_item(int index, const char *item_label, const char *item_value,
                            const char *glyph_name, bool add_bottom_border) {
    lv_obj_t *ui_pnlInfoItem = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(ui_pnlInfoItem);

    lv_obj_t *ui_lblInfoItem = lv_label_create(ui_pnlInfoItem);
    apply_theme_list_item(&theme, ui_lblInfoItem, item_label);

    lv_obj_t *ui_icoInfoItem = lv_img_create(ui_pnlInfoItem);
    apply_theme_list_glyph(&theme, ui_icoInfoItem, mux_module, glyph_name);

    lv_obj_t *ui_lblInfoItemValue = lv_label_create(ui_pnlInfoItem);
    apply_theme_list_value(&theme, ui_lblInfoItemValue, item_value);

    if (add_bottom_border) {
        lv_obj_set_height(ui_pnlInfoItem, 1);
        lv_obj_set_style_border_width(ui_pnlInfoItem, 1, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(ui_pnlInfoItem, lv_color_hex(theme.LIST_DEFAULT.TEXT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnlInfoItem, theme.LIST_DEFAULT.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_side(ui_pnlInfoItem, LV_BORDER_SIDE_BOTTOM, MU_OBJ_MAIN_DEFAULT);
    } else if (theme.MUX.ITEM.COUNT < UI_COUNT) {
        ui_objects_panel[group_index] = ui_pnlInfoItem;
        ui_objects[group_index] = ui_lblInfoItem;
        lv_obj_set_user_data(ui_lblInfoItem, "info_item");
        ui_objects_glyph[group_index] = ui_icoInfoItem;
        ui_objects_value[group_index] = ui_lblInfoItemValue;
        group_index++;
    }

    lv_obj_move_to_index(ui_pnlInfoItem, index);
}

static void add_info_item_type(lv_obj_t *ui_lblItemValue, const char *get_file, const char *get_dir,
                               const char *opt_type, bool cap_label) {
    const char *value = get_file;

    if (!*value) value = get_dir;

    if (!*value) {
        value = !strcmp(opt_type, "con") ? lang.MUXOPTION.NONE :
                !strcmp(opt_type, "tag") ? lang.MUXOPTION.NOT_ASSIGNED :
                "System";
    }

    char cap_value[MAX_BUFFER_SIZE];
    snprintf(cap_value, sizeof(cap_value), "%s", value);

    apply_theme_list_value(&theme, ui_lblItemValue, cap_label ? str_capital_all(cap_value) : cap_value);
}

static void add_info_items(void) {
    const char *core_file = get_content_line(rom_dir, rom_name, "cfg", 2);
    const char *core_dir = get_content_line(rom_dir, NULL, "cfg", 1);
    add_info_item_type(ui_lblCoreValue_option, core_file, core_dir, "core", false);

    const char *gov_file = get_content_line(rom_dir, rom_name, "gov", 1);
    const char *gov_dir = get_content_line(rom_dir, NULL, "gov", 1);
    add_info_item_type(ui_lblGovernorValue_option, gov_file, gov_dir, "governor", true);

    const char *control_file = get_content_line(rom_dir, rom_name, "con", 1);
    const char *control_dir = get_content_line(rom_dir, NULL, "con", 1);
    add_info_item_type(ui_lblControlValue_option, control_file, control_dir, "con", true);

    const char *tag_file = get_content_line(rom_dir, rom_name, "tag", 1);
    const char *tag_dir = get_content_line(rom_dir, NULL, "tag", 1);
    add_info_item_type(ui_lblTagValue_option, tag_file, tag_dir, "tag", true);
}

static struct json get_playtime_json(void) {
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", rom_dir, rom_name);

    char playtime_data[MAX_BUFFER_SIZE];
    snprintf(playtime_data, sizeof(playtime_data), INFO_ACT_PATH "/" PLAYTIME_DATA);

    if (!file_exist(playtime_data)) {
        LOG_WARN(mux_module, "Playtime Data Not Found At: %s", playtime_data)
        return (struct json) {0};
    } else {
        LOG_SUCCESS(mux_module, "Found Playtime Data At: %s", playtime_data)
    }

    char *json_str = read_all_char_from(playtime_data);
    if (!json_valid(json_str)) {
        free(json_str);
        return (struct json) {0};
    }

    struct json fn_json = json_parse(json_str);

    struct json playtime_json = json_object_get(fn_json, fullpath);
    if (!json_exists(playtime_json)) return (struct json) {0};

    free(json_str);

    return playtime_json;
}

static char *get_time_played(void) {
    struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return lang.GENERIC.UNKNOWN;

    static char time_buffer[MAX_BUFFER_SIZE] = "0m";
    int total_time = json_int(json_object_get(playtime_json, "total_time"));

    int days = total_time / 86400;
    int hours = (total_time % 86400) / 3600;
    int minutes = (total_time % 3600) / 60;

    if (days > 0)
        snprintf(time_buffer, sizeof(time_buffer), "%dd %dh %dm",
                 days, hours, minutes);
    else if (hours > 0)
        snprintf(time_buffer, sizeof(time_buffer), "%dh %dm",
                 hours, minutes);
    else if (minutes > 0)
        snprintf(time_buffer, sizeof(time_buffer), "%dm",
                 minutes);
    else
        snprintf(time_buffer, sizeof(time_buffer), "%s",
                 lang.GENERIC.UNKNOWN);

    return time_buffer;
}

static char *get_launch_count(void) {
    struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return lang.GENERIC.UNKNOWN;

    static char launch_count[MAX_BUFFER_SIZE];
    snprintf(launch_count, sizeof(launch_count), "%d",
             json_int(json_object_get(playtime_json, "launches")));

    return launch_count;
}

static void init_navigation_group(void) {
    int line_index = 0;

    add_static_item(line_index++, lang.MUXOPTION.DIRECTORY, get_last_subdir(rom_dir, '/', 4), "folder", false);
    add_static_item(line_index++, lang.MUXOPTION.NAME, rom_name, "rom", false);
    add_static_item(line_index++, lang.MUXOPTION.TIME, get_time_played(), "time", false);
    add_static_item(line_index++, lang.MUXOPTION.LAUNCH, get_launch_count(), "count", false);
    add_static_item(line_index, "", "", "", true);

    INIT_VALUE_ITEM(-1, option, Search, lang.MUXOPTION.SEARCH, "search", "");

    const char *dot = strrchr(rom_name, '.');
    if ((dot && dot != rom_name)) {
        INIT_VALUE_ITEM(-1, option, Core, lang.MUXOPTION.CORE, "core", "");
        INIT_VALUE_ITEM(-1, option, Governor, lang.MUXOPTION.GOVERNOR, "governor", "");
        INIT_VALUE_ITEM(-1, option, Control, lang.MUXOPTION.CONTROL, "control", "");
        INIT_VALUE_ITEM(-1, option, Tag, lang.MUXOPTION.TAG, "tag", "");

        add_info_items();
    }

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();
    ui_group_value = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
    }

    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
        nav_move(ui_group_value, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"search",   "search",   &kiosk.CONTENT.SEARCH},
            {"core",     "assign",   &kiosk.CONTENT.CORE},
            {"governor", "governor", &kiosk.CONTENT.GOVERNOR},
            {"control",  "control",  &kiosk.CONTENT.CONTROL},
            {"tag",      "tag",      &kiosk.CONTENT.TAG}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);
    if (strcasecmp(u_data, "info_item") == 0) return;

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (kiosk.ENABLE && elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                kiosk_denied();
                return;
            }

            play_sound(SND_CONFIRM);
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, elements[i].glyph_name);
            load_mux(elements[i].mux_name);
            break;
        }
    }

    close_input();
    mux_input_stop();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define OPTION(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_option, UDATA);
    OPTION_ELEMENTS
#undef OPTION

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxoption_main(int nothing, char *name, char *dir, char *sys, int app) {
    group_index = 0;

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    init_module("muxoption");

    if (file_exist(OPTION_SKIP)) {
        remove(OPTION_SKIP);
        LOG_INFO(mux_module, "Skipping Options Module - Not Required...")
        close_input();
        return 0;
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXOPTION.TITLE);
    init_muxoption(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
