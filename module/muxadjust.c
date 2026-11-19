#include "muxshare.h"
#include <common/ui/orientation.h>
#include "ui/ui_muxadjust.h"

#define ADJUST(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(ADJUST_ELEMENTS) };
#undef ADJUST

int adjust_screen = content_layout_explore;
char adjust_folder[PATH_MAX];

static mux_dialogue reset_dlg;
static mux_dialogue folders_dlg;

static mux_dialogue *open_dialogue(void) {
    if (dialogue_active(&reset_dlg)) return &reset_dlg;
    if (dialogue_active(&folders_dlg)) return &folders_dlg;
    return NULL;
}

static void show_folder_count(void) {
    char count[16];
    snprintf(count, sizeof(count), "%d", folder_custom_count());
    lv_dropdown_set_options(ui_dro_folders_adjust, count);
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
        {"customcolour", lang.muxcustom.help.colours}, {"menuadjust", lang.muxcustom.help.menu_adjust},
        {"explore", lang.muxadjust.help.explore},      {"collection", lang.muxadjust.help.collection},
        {"history", lang.muxadjust.help.history},      {"folders", lang.muxadjust.help.folders},
        {"reset", lang.muxadjust.help.reset},
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void reset_adjustments(void) {
    for (int screen = 0; screen < content_layout_count; screen++)
        content_layout_reset(&config.settings.layout[screen]);

    const int failed = !content_layout_save(&config);
    refresh_config = 1;

    toast_message(failed ? lang.generic.save_fail : lang.generic.saving, failed ? tst_wait_m : tst_wait_f);
}

static void reset_folders(void) {
    const int failed = !folder_custom_reset_all();
    show_folder_count();

    toast_message(failed ? lang.generic.save_fail : lang.generic.saving, failed ? tst_wait_m : tst_wait_f);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, 0, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_OPTION_ITEM(-1, adjust, colours, lang.muxcustom.customcolours, "customcolour", NULL, 0);
    INIT_OPTION_ITEM(-1, adjust, menu, lang.muxcustom.menu_adjust, "menuadjust", NULL, 0);
    INIT_OPTION_ITEM(-1, adjust, explore, lang.muxadjust.explore, "explore", NULL, 0);
    INIT_OPTION_ITEM(-1, adjust, collection, lang.muxadjust.collection, "collection", NULL, 0);
    INIT_OPTION_ITEM(-1, adjust, history, lang.muxadjust.history, "history", NULL, 0);
    char *folder_value[] = {""};
    INIT_OPTION_ITEM(-1, adjust, folders, lang.muxadjust.folders, "folders", folder_value, 1);
    show_folder_count();
    INIT_OPTION_ITEM(-1, adjust, reset, lang.muxadjust.reset, "reset", NULL, 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    int nav_moved = 0;
    list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
}

static void open_module(const char *name) {
    play_sound(snd_confirm);
    load_mux(name);

    mux_input_stop();
}

static void open_screen(const int screen) {
    adjust_screen = screen;
    adjust_folder[0] = '\0';
    play_sound(snd_confirm);
    load_mux("adjustscreen");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    mux_dialogue *dialogue = open_dialogue();
    if (dialogue) {
        dialogue_navigate(dialogue, &theme, -1);
        play_sound(snd_navigate);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    mux_dialogue *dialogue = open_dialogue();
    if (dialogue) {
        dialogue_navigate(dialogue, &theme, +1);
        play_sound(snd_navigate);
        return;
    }

    handle_list_nav_down();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    mux_dialogue *dialogue = open_dialogue();
    if (dialogue) {
        const mux_confirm_opt choice = (mux_confirm_opt) dialogue->selected;
        dialogue_dismiss(dialogue);
        if (choice != mux_confirm_yep) return;

        if (dialogue == &reset_dlg) {
            reset_adjustments();
        } else {
            reset_folders();
        }
        return;
    }

    const lv_obj_t *focused = lv_group_get_focused(ui_group);
    if (focused == ui_lbl_colours_adjust) {
        open_module("colour");
    } else if (focused == ui_lbl_menu_adjust) {
        open_module("menuadjust");
    } else if (focused == ui_lbl_explore_adjust) {
        open_screen(content_layout_explore);
    } else if (focused == ui_lbl_collection_adjust) {
        open_screen(content_layout_collection);
    } else if (focused == ui_lbl_history_adjust) {
        open_screen(content_layout_history);
    } else if (focused == ui_lbl_folders_adjust) {
        if (!folder_custom_count()) {
            play_sound(snd_error);
            return;
        }

        play_sound(snd_confirm);
        dialogue_open(&folders_dlg, &theme);
    } else if (focused == ui_lbl_reset_adjust) {
        play_sound(snd_confirm);
        dialogue_open(&reset_dlg, &theme);
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    mux_dialogue *dialogue = open_dialogue();
    if (dialogue) {
        dialogue_mark_cancelled(dialogue);
        dialogue_dismiss(dialogue);
        return;
    }

    play_sound(snd_back);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themeopt");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || open_dialogue() || progress_onscreen != -1 || !ui_count_static || hold_call) return;

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

#define ADJUST(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_adjust, UDATA);
    ADJUST_ELEMENTS
#undef ADJUST

    overlay_display();
}

int muxadjust_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxadjust.title);
    init_muxadjust(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    dialogue_init_confirm(
        &reset_dlg, &theme, ui_screen, lang.muxadjust.reset_title, lang.muxadjust.reset_desc, lang.generic.reset,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    dialogue_init_confirm(
        &folders_dlg, &theme, ui_screen, lang.muxadjust.folders_title, lang.muxadjust.folders_desc, lang.generic.reset,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
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
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxadjust.title, lang.muxadjust.overview);

    mux_input_task(&input_opts);

    return 0;
}
