#include "muxshare.h"
#include "ui/ui_muxoption.h"

#define UI_COUNT 6

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
            {ui_lblTag_option,      lang.MUXOPTION.HELP.TAG},
    };

    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s", lang.GENERIC.NO_HELP);

    if (element_focused == help_messages[0].element) {
        snprintf(message, sizeof(message), "%s", help_messages[0].message);
    }

    if (strlen(message) <= 1) snprintf(message, sizeof(message), "%s", lang.GENERIC.NO_HELP);

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
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
        lv_obj_set_style_border_width(ui_pnlInfoItem, 1,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_pnlInfoItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnlInfoItem, theme.LIST_DEFAULT.TEXT_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_pnlInfoItem, LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
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
    if (!*value) value = !strcmp(opt_type, "tag") ? lang.MUXOPTION.NONE : lang.MUXOPTION.NOT_ASSIGNED;

    char cap_value[MAX_BUFFER_SIZE];
    snprintf(cap_value, sizeof(cap_value), "%s", value);

    apply_theme_list_value(&theme, ui_lblItemValue, cap_label ? str_capital_all(cap_value) : cap_value);
}

static void add_info_items() {
    const char *core_file = get_content_line(rom_dir, rom_name, "cfg", 2);
    const char *core_dir = get_content_line(rom_dir, NULL, "cfg", 1);
    add_info_item_type(ui_lblCoreValue_option, core_file, core_dir, "core", false);

    const char *gov_file = get_content_line(rom_dir, rom_name, "gov", 1);
    const char *gov_dir = get_content_line(rom_dir, NULL, "gov", 1);
    add_info_item_type(ui_lblGovernorValue_option, gov_file, gov_dir, "governor", true);

    const char *tag_file = get_content_line(rom_dir, rom_name, "tag", 1);
    const char *tag_dir = get_content_line(rom_dir, NULL, "tag", 1);
    add_info_item_type(ui_lblTagValue_option, tag_file, tag_dir, "tag", true);
}

static void init_navigation_group() {
    int line_index = 0;

    add_static_item(line_index++, lang.MUXOPTION.DIRECTORY, get_last_subdir(rom_dir, '/', 4), "folder", false);
    add_static_item(line_index++, lang.MUXOPTION.NAME, rom_name, "rom", false);
    add_static_item(line_index, "", "", "", true);

    INIT_VALUE_ITEM(-1, option, Search, lang.MUXOPTION.SEARCH, "search", "");

    const char *dot = strrchr(rom_name, '.');
    if ((dot && dot != rom_name)) {
        INIT_VALUE_ITEM(-1, option, Core, lang.MUXOPTION.CORE, "core", "");
        INIT_VALUE_ITEM(-1, option, Governor, lang.MUXOPTION.GOVERNOR, "governor", "");
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

static void handle_confirm() {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"search",   "search",   &kiosk.CONTENT.SEARCH},
            {"core",     "assign",   &kiosk.CONTENT.CORE},
            {"governor", "governor", &kiosk.CONTENT.GOVERNOR},
            {"tag",      "tag",      &kiosk.CONTENT.TAG}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);
    if (strcasecmp(u_data, "info_item") == 0) return;

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (kiosk.ENABLE && elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                play_sound(SND_ERROR);
                toast_message(kiosk_nope(), 1000);
                refresh_screen(ui_screen);
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

static void handle_back() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
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

static void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_standard_panels[0] = ui_pnlFooter;
    ui_mux_standard_panels[1] = ui_pnlHeader;
    ui_mux_standard_panels[2] = ui_pnlHelp;
    ui_mux_standard_panels[3] = ui_pnlProgressBrightness;
    ui_mux_standard_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_standard_panels, sizeof(ui_mux_standard_panels) / sizeof(ui_mux_standard_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

#define OPTION(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_option, UDATA);
    OPTION_ELEMENTS
#undef OPTION

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    if (kiosk.ENABLE) {
        kiosk_image = lv_img_create(ui_screen);
        load_kiosk_image(ui_screen, kiosk_image);
    }

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_standard_panels, sizeof(ui_mux_standard_panels) / sizeof(ui_mux_standard_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxoption_main(int nothing, char *name, char *dir, char *sys) {
    (void) nothing;

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
