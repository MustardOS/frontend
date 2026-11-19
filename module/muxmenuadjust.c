#include "muxshare.h"
#include <common/ui/orientation.h>
#include <common/ui/nav/menu_style.h>
#include "ui/ui_muxmenuadjust.h"

#define MENUADJUST(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(MENUADJUST_ELEMENTS) };
#undef MENUADJUST

#define MENUADJUST(NAME, UDATA) static int NAME##_original;
MENUADJUST_ELEMENTS
#undef MENUADJUST

static mux_dialogue reset_dlg;
static mux_dialogue arrange_dlg;

static const int menu_style_bases[] = {
    menu_style_theme,   menu_style_list,   menu_style_grid,    menu_style_carousel, menu_style_bubbles,
    menu_style_orbit,   menu_style_stack,  menu_style_clock,   menu_style_rolodex,  menu_style_dock,
    menu_style_fan,     menu_style_tiles,  menu_style_sidebar, menu_style_fluent,   menu_style_banners,
    menu_style_desktop, menu_style_slices,
};

static int menu_arrange_styles[MENU_ARRANGEMENT_MAX] = {menu_style_theme};
static int menu_arrange_count = 1;
static int menu_arrange_custom = -1;

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define MENUADJUST(NAME, UDATA) {UDATA, lang.muxcustom.help.NAME},
        MENUADJUST_ELEMENTS
#undef MENUADJUST
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static const char *menu_arrange_label(const int style) {
    switch (style) {
        case menu_style_zigzag:
            return lang.muxcustom.menu_arrange.staggered;
        case menu_style_paged:
            return lang.muxcustom.menu_arrange.pages;
        case menu_style_carousel:
            return lang.muxcustom.menu_arrange.flat;
        case menu_style_coverflow:
            return lang.muxcustom.menu_arrange.tilted;
        case menu_style_spotlight:
            return lang.muxcustom.menu_arrange.spotlight;
        case menu_style_dock:
            return lang.muxcustom.menu_arrange.bottom;
        case menu_style_crossbar:
        case menu_style_tabbar:
            return lang.muxcustom.menu_arrange.top;
        case menu_style_sidebar:
            return lang.muxcustom.menu_arrange.side;
        case menu_style_fluent:
            return lang.muxcustom.menu_arrange.blocks;
        case menu_style_mascot:
            return lang.muxcustom.menu_arrange.mascot;
        case menu_style_grid:
            return lang.muxcustom.menu_arrange.rows;
        default:
            return lang.muxcustom.menu_arrange.standard;
    }
}

static int menu_style_base_index(const int style) {
    const int base = menu_style_base(style);
    for (size_t i = 0; i < A_SIZE(menu_style_bases); i++) {
        if (menu_style_bases[i] == base) return (int) i;
    }
    return 0;
}

static void populate_menu_arrange(const int style) {
    const uint32_t selected = lv_dropdown_get_selected(ui_dro_menu_style_menuadjust);
    const int base = menu_style_bases[selected < A_SIZE(menu_style_bases) ? selected : 0];
    menu_arrange_count = menu_style_arrangements(base, menu_arrange_styles);

    lv_dropdown_clear_options(ui_dro_menu_arrange_menuadjust);
    const int active = menu_style_resolve(config.settings.themeopt.menu_style);
    int chosen = 0;
    menu_arrange_custom = -1;
    for (int i = 0; i < menu_arrange_count; i++) {
        const int changed = menu_arrange_styles[i] == active && menu_style_customised(active);
        if (changed) menu_arrange_custom = i;
        lv_dropdown_add_option(
            ui_dro_menu_arrange_menuadjust,
            changed ? lang.muxcustom.menu_arrange.custom : menu_arrange_label(menu_arrange_styles[i]),
            LV_DROPDOWN_POS_LAST
        );
        if (menu_arrange_styles[i] == menu_style_resolve(style)) chosen = i;
    }

    lv_dropdown_set_selected(ui_dro_menu_arrange_menuadjust, chosen);
}

static int selected_menu_style(void) {
    const uint32_t arrange = lv_dropdown_get_selected(ui_dro_menu_arrange_menuadjust);
    return menu_arrange_styles[arrange < (uint32_t) menu_arrange_count ? arrange : 0];
}

static int arrange_reset_available(void) {
    return lv_group_get_focused(ui_group) == ui_lbl_menu_arrange_menuadjust && menu_arrange_custom >= 0
           && (int) lv_dropdown_get_selected(ui_dro_menu_arrange_menuadjust) == menu_arrange_custom;
}

static void update_nav(void) {
    const lv_obj_t *focused = lv_group_get_focused(ui_group);
    const int select = focused == ui_lbl_menu_layout_menuadjust || focused == ui_lbl_menu_reset_menuadjust;

    footer_nav_show(ui_lbl_nav_a_glyph, ui_lbl_nav_a, select);
    footer_nav_show(ui_lbl_nav_lr_glyph, ui_lbl_nav_lr, !select);
    footer_nav_show(ui_lbl_nav_x_glyph, ui_lbl_nav_x, arrange_reset_available());
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, 0, 1);
    update_nav();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void reset_arrangement(void) {
    const int style = menu_arrange_styles[menu_arrange_custom];
    const int done = menu_style_reset_layout(style);
    populate_menu_arrange(style);
    update_nav();
    play_sound(done ? snd_confirm : snd_error);
}

static void reset_main_menu(void) {
    int failed = !menu_layout_reset_all();

#define RESET_MAIN(NAME, VALUE)                                                                                        \
    lv_dropdown_set_selected(ui_dro_##NAME##_menuadjust, VALUE);                                                       \
    NAME##_original = VALUE;                                                                                           \
    config.settings.themeopt.NAME = VALUE;                                                                             \
    if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/" #NAME, INT, VALUE)) failed++;
    RESET_MAIN(menu_style, 0)
    RESET_MAIN(menu_label, 1)
    RESET_MAIN(menu_random, 0)
#undef RESET_MAIN

    populate_menu_arrange(menu_style_theme);
    menu_arrange_original = 0;

    lv_dropdown_set_selected(ui_dro_menu_order_menuadjust, 0);
    menu_order_original = 0;
    config.settings.themeopt.menu_order[0] = '\0';
    if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/menu_order", CHAR, "")) failed++;

    refresh_config = 1;
    update_nav();

    play_sound(failed ? snd_error : snd_confirm);
    toast_message(failed ? lang.generic.save_fail : lang.muxcustom.menureset_done, tst_wait_m);
}

static void restore_menu_options(void) {
    lv_dropdown_set_selected(ui_dro_menu_style_menuadjust, menu_style_base_index(config.settings.themeopt.menu_style));
    populate_menu_arrange(config.settings.themeopt.menu_style);
    lv_dropdown_set_selected(ui_dro_menu_label_menuadjust, config.settings.themeopt.menu_label);
    lv_dropdown_set_selected(ui_dro_menu_random_menuadjust, config.settings.themeopt.menu_random);
    lv_dropdown_set_selected(ui_dro_menu_order_menuadjust, config.settings.themeopt.menu_order[0] != '\0');

#define MENUADJUST(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_menuadjust);
    MENUADJUST_ELEMENTS
#undef MENUADJUST
}

static void save_menu_options(void) {
    int is_modified = 0;
    int save_failed = 0;

    if (selected_menu_style() != config.settings.themeopt.menu_style) {
        is_modified++;
        if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/menu_style", INT, selected_menu_style()))
            save_failed++;
    }

    CHECK_AND_SAVE_STD(menuadjust, menu_label, "settings/theme/menu_label", INT, 0);
    CHECK_AND_SAVE_STD(menuadjust, menu_random, "settings/theme/menu_random", INT, 0);

    if (!lv_dropdown_get_selected(ui_dro_menu_order_menuadjust) && menu_order_original) {
        is_modified++;
        if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/menu_order", CHAR, "")) save_failed++;
    }

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }

    REPORT_SAVE_FAILURE();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *menu_style_options[] = {
        lang.muxcustom.menu_style.theme,    lang.muxcustom.menu_style.list,    lang.muxcustom.menu_style.grid,
        lang.muxcustom.menu_style.carousel, lang.muxcustom.menu_style.bubbles, lang.muxcustom.menu_style.orbit,
        lang.muxcustom.menu_style.stack,    lang.muxcustom.menu_style.clock,   lang.muxcustom.menu_style.rolodex,
        lang.muxcustom.menu_style.dock,     lang.muxcustom.menu_style.fan,     lang.muxcustom.menu_style.tiles,
        lang.muxcustom.menu_style.sidebar,  lang.muxcustom.menu_style.fluent,  lang.muxcustom.menu_style.banners,
        lang.muxcustom.menu_style.desktop,  lang.muxcustom.menu_style.slices
    };
    _Static_assert(A_SIZE(menu_style_options) == A_SIZE(menu_style_bases), "menu style labels and styles differ");

    char *menu_arrange_options[] = {lang.muxcustom.menu_arrange.rows};
    char *menu_order_options[] = {lang.muxcustom.menu_order.standard, lang.muxcustom.menu_order.custom};

    INIT_OPTION_ITEM(
        -1, menuadjust, menu_style, lang.muxcustom.menu_style.title, "menustyle", menu_style_options,
        A_SIZE(menu_style_options)
    );
    INIT_OPTION_ITEM(
        -1, menuadjust, menu_arrange, lang.muxcustom.menu_arrange.title, "menuarrange", menu_arrange_options,
        A_SIZE(menu_arrange_options)
    );
    INIT_OPTION_ITEM(-1, menuadjust, menu_layout, lang.muxcustom.menulayout, "menulayout", NULL, 0);
    INIT_OPTION_ITEM(-1, menuadjust, menu_label, lang.muxcustom.menulabel, "menulabel", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, menuadjust, menu_random, lang.muxcustom.menurandom, "menurandom", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, menuadjust, menu_order, lang.muxcustom.menu_order.title, "menuorder", menu_order_options,
        A_SIZE(menu_order_options)
    );
    INIT_OPTION_ITEM(-1, menuadjust, menu_reset, lang.muxcustom.menureset, "menureset", NULL, 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    restore_menu_options();

    int nav_moved = 0;
    list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
}

static mux_dialogue *choice_dialogue(void) {
    if (dialogue_active(&reset_dlg)) return &reset_dlg;
    if (dialogue_active(&arrange_dlg)) return &arrange_dlg;
    return NULL;
}

static void handle_option(const int direction) {
    if (msgbox_active) return;

    if (choice_dialogue()) {
        if (swap_axis) {
            dialogue_navigate(choice_dialogue(), &theme, direction);
            play_sound(snd_navigate);
        }
        return;
    }

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    if (focused == ui_dro_menu_order_menuadjust && !menu_order_original) return;

    move_option(focused, direction);
    if (focused == ui_dro_menu_style_menuadjust) populate_menu_arrange(-1);
    update_nav();
}

static void handle_option_prev(void) {
    handle_option(-1);
}

static void handle_option_next(void) {
    handle_option(+1);
}

static void open_layout(void) {
    save_menu_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "menulayout");

    play_sound(snd_confirm);
    toast_message(lang.generic.loading, tst_wait_f);
    load_mux("layout");

    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (dialogue_active(&reset_dlg)) {
        const mux_confirm_opt choice = (mux_confirm_opt) reset_dlg.selected;
        dialogue_dismiss(&reset_dlg);
        if (choice == mux_confirm_yep) reset_main_menu();
        return;
    }

    if (dialogue_active(&arrange_dlg)) {
        const mux_confirm_opt choice = (mux_confirm_opt) arrange_dlg.selected;
        dialogue_dismiss(&arrange_dlg);
        if (choice == mux_confirm_yep) reset_arrangement();
        return;
    }

    const lv_obj_t *focused = lv_group_get_focused(ui_group);
    if (focused == ui_lbl_menu_layout_menuadjust) {
        open_layout();
    } else if (focused == ui_lbl_menu_reset_menuadjust) {
        play_sound(snd_confirm);
        dialogue_open(&reset_dlg, &theme);
    }
}

static void handle_x(void) {
    if (!choice_dialogue() && !msgbox_active && arrange_reset_available()) {
        play_sound(snd_confirm);
        dialogue_open(&arrange_dlg, &theme);
        return;
    }

    orientation_handle_skip();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    mux_dialogue *dialogue = choice_dialogue();
    if (dialogue) {
        dialogue_mark_cancelled(dialogue);
        dialogue_dismiss(dialogue);
        return;
    }

    play_sound(snd_back);

    save_menu_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "menuadjust");

    mux_input_stop();
}

static void handle_dpad(const int direction) {
    if (choice_dialogue()) {
        if (!swap_axis) {
            dialogue_navigate(choice_dialogue(), &theme, direction);
            play_sound(snd_navigate);
        }
        return;
    }

    if (direction < 0) {
        handle_list_nav_up();
    } else {
        handle_list_nav_down();
    }
}

static void handle_dpad_up(void) {
    handle_dpad(-1);
}

static void handle_dpad_down(void) {
    handle_dpad(+1);
}

static void handle_dpad_up_hold(void) {
    if (choice_dialogue()) {
        dialogue_handle_dpad_hold(choice_dialogue(), &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (choice_dialogue()) {
        dialogue_handle_dpad_hold(choice_dialogue(), &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || choice_dialogue() || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.reset, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define MENUADJUST(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_menuadjust, UDATA);
    MENUADJUST_ELEMENTS
#undef MENUADJUST

    overlay_display();
}

int muxmenuadjust_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxmenuadjust.title);
    init_muxmenuadjust(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    dialogue_init_confirm(
        &reset_dlg, &theme, ui_screen, lang.muxcustom.menureset_title, lang.muxcustom.menureset_desc,
        lang.generic.reset, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &arrange_dlg, &theme, ui_screen, lang.muxcustom.menu_arrange.reset_title,
        lang.muxcustom.menu_arrange.reset_desc, lang.generic.reset, lang.generic.cancel, lang.generic.select,
        lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);

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

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxmenuadjust.title, lang.muxmenuadjust.overview);

    mux_input_task(&input_opts);

    return 0;
}
