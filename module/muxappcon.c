#include "muxshare.h"
#include "ui/ui_muxappcon.h"

#define APPCON(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(APPCON_ELEMENTS) };
#undef APPCON

static char app_name[MAX_BUFFER_SIZE];
static char app_dir[MAX_BUFFER_SIZE];

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];

static int group_index = 0;

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define APPCON(NAME, UDATA) {UDATA, lang.muxappcon.help.NAME},
        APPCON_ELEMENTS
#undef APPCON
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void add_static_item(
    const int index, const char *item_label, const char *item_value, const char *glyph_name, const int add_bottom_border
) {
    lv_obj_t *ui_pnl_info_item = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(ui_pnl_info_item);

    lv_obj_t *ui_lbl_info_item = lv_label_create(ui_pnl_info_item);
    apply_theme_list_item(&theme, ui_lbl_info_item, item_label);

    lv_obj_t *ui_ico_info_item = lv_img_create(ui_pnl_info_item);
    apply_theme_list_glyph(&theme, ui_ico_info_item, mux_module, glyph_name);

    lv_obj_t *ui_lbl_info_item_value = lv_label_create(ui_pnl_info_item);
    apply_theme_list_value(&theme, ui_lbl_info_item_value, item_value);

    if (add_bottom_border) {
        lv_obj_set_height(ui_pnl_info_item, 1);
        lv_obj_set_style_border_width(ui_pnl_info_item, 1, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(ui_pnl_info_item, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnl_info_item, theme.list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_side(ui_pnl_info_item, LV_BORDER_SIDE_BOTTOM, MU_OBJ_MAIN_DEFAULT);
    } else if (theme.mux.item.count < ui_count_dynamic) {
        ui_objects_panel[group_index] = ui_pnl_info_item;
        ui_objects[group_index] = ui_lbl_info_item;
        lv_obj_set_user_data(ui_lbl_info_item, "info_item");
        ui_objects_glyph[group_index] = ui_ico_info_item;
        ui_objects_value[group_index] = ui_lbl_info_item_value;
        group_index++;
    }

    lv_obj_move_to_index(ui_pnl_info_item, index);
}

static void add_info_item_type(lv_obj_t *ui_lbl_item_value, const char *get_file, const char *opt_type) {
    const char *value = get_file;

    if (!*value) value = strcmp(opt_type, "gov") == 0 ? device.cpu.dflt : "System";

    char cap_value[MAX_BUFFER_SIZE];
    snprintf(cap_value, sizeof(cap_value), "%s", value);

    apply_theme_list_value(&theme, ui_lbl_item_value, str_capital_all(cap_value));
}

static void add_info_items(void) {
    const char *app_gov = get_application_line(app_dir, "gov", 1);
    add_info_item_type(ui_val_governor_appcon, app_gov, "gov");

    const char *app_con = get_application_line(app_dir, "con", 1);
    add_info_item_type(ui_val_control_appcon, app_con, "con");
}

static void init_navigation_group(void) {
    int line_index = 0;

    add_static_item(line_index++, lang.muxappcon.name, app_name, "app", 0);
    add_static_item(line_index, "", "", "", 1);

    INIT_VALUE_ITEM(-1, appcon, governor, lang.muxappcon.governor, "governor", "");
    INIT_VALUE_ITEM(-1, appcon, control, lang.muxappcon.control, "control", "");

    add_info_items();

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (strcasecmp(get_last_dir(app_dir), "RetroArch") != 0) HIDE_VALUE_ITEM(appcon, control);

    list_nav_move(direct_to_previous(ui_objects, ui_count_static, &nav_moved), +1);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 0, -1, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"governor", &kiosk.content.governor},
        {"control", &kiosk.content.control},
    };

    if ((unsigned) current_item_index >= ui_count_dynamic) return;
    const menu_entry *entry = &entries[current_item_index];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    play_sound(snd_confirm);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, entry->mux_name);

    load_mux(entry->mux_name);

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define APPCON(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_appcon, UDATA);
    APPCON_ELEMENTS
#undef APPCON

    overlay_display();
}

void muxappcon_main(const int auto_assign, const char *name, const char *dir, const char *sys, const int app) {
    (void) auto_assign;
    (void) sys;
    (void) app;

    group_index = 0;

    snprintf(app_name, sizeof(app_name), "%s", name);
    snprintf(app_dir, sizeof(app_dir), "%s", dir);

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxappcon.title);
    init_muxappcon(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);
}
