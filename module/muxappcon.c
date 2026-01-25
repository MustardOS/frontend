#include "muxshare.h"
#include "ui/ui_muxappcon.h"

#define UI_COUNT 3

static char app_name[MAX_BUFFER_SIZE];
static char app_dir[MAX_BUFFER_SIZE];

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];

static int group_index = 0;

static void list_nav_move(int steps, int direction);

static void show_help() {
    struct help_msg help_messages[] = {
#define APPCON(NAME, ENUM, UDATA) { ui_lbl##NAME##_appcon, lang.MUXAPPCON.HELP.ENUM },
            APPCON_ELEMENTS
#undef APPCON
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
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

static void add_info_item_type(lv_obj_t *ui_lblItemValue, const char *get_file, const char *opt_type) {
    const char *value = get_file;

    if (!*value) value = strcmp(opt_type, "gov") == 0 ? device.CPU.DEFAULT : "System";

    char cap_value[MAX_BUFFER_SIZE];
    snprintf(cap_value, sizeof(cap_value), "%s", value);

    apply_theme_list_value(&theme, ui_lblItemValue, str_capital_all(cap_value));
}

static void add_info_items(void) {
    const char *app_gov = get_application_line(app_dir, "gov", 1);
    add_info_item_type(ui_lblGovernorValue_appcon, app_gov, "gov");

    const char *app_con = get_application_line(app_dir, "con", 1);
    add_info_item_type(ui_lblControlValue_appcon, app_con, "con");
}

static void init_navigation_group(void) {
    int line_index = 0;

    add_static_item(line_index++, lang.MUXAPPCON.NAME, app_name, "app", false);
    add_static_item(line_index, "", "", "", true);

    INIT_VALUE_ITEM(-1, appcon, Governor, lang.MUXAPPCON.GOVERNOR, "governor", "");
    INIT_VALUE_ITEM(-1, appcon, Control, lang.MUXAPPCON.CONTROL, "control", "");

    add_info_items();

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (strcasecmp(get_last_dir(app_dir), "RetroArch") != 0) HIDE_VALUE_ITEM(appcon, Control);

    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, -1);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {"governor", &kiosk.CONTENT.GOVERNOR},
            {"control",  &kiosk.CONTENT.CONTROL},
    };

    if ((unsigned) current_item_index >= UI_COUNT) return;
    const menu_entry *entry = &entries[current_item_index];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    play_sound(SND_CONFIRM);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, entry->mux_name);

    load_mux(entry->mux_name);

    close_input();
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

    play_sound(SND_BACK);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define APPCON(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_appcon, UDATA);
    APPCON_ELEMENTS
#undef APPCON

    overlay_display();
}

int muxappcon_main(int nothing, char *name, char *dir, char *sys, int app) {
    group_index = 0;

    snprintf(app_name, sizeof(app_name), "%s", name);
    snprintf(app_dir, sizeof(app_dir), "%s", dir);

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXAPPCON.TITLE);
    init_muxappcon(ui_pnlContent);
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
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
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
