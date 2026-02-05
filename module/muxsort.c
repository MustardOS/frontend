#include "muxshare.h"
#include "ui/ui_muxsort.h"

#define UI_COUNT 16

#define SORT(NAME, ENUM, UDATA) static int NAME##_original;
SORT_ELEMENTS
#undef SORT

static void show_help() {
    struct help_msg help_messages[] = {
#define SORT(NAME, ENUM, UDATA) { ui_lbl##NAME##_sort, lang.MUXSORT.HELP.ENUM },
            SORT_ELEMENTS
#undef SORT
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define SORT(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_sort);
    SORT_ELEMENTS
#undef SORT
}

static void restore_sort_options(void) {
#define SORT(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_sort, config.SORT.ENUM);
    SORT_ELEMENTS
#undef SORT
}

static void save_sort_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(sort, Default, "sort/default", INT, 0);
    CHECK_AND_SAVE_STD(sort, Collection, "sort/collection", INT, 0);
    CHECK_AND_SAVE_STD(sort, History, "sort/history", INT, 0);
    CHECK_AND_SAVE_STD(sort, Abandoned, "sort/abandoned", INT, 0);
    CHECK_AND_SAVE_STD(sort, Alternate, "sort/alternate", INT, 0);
    CHECK_AND_SAVE_STD(sort, Backlog, "sort/backlog", INT, 0);
    CHECK_AND_SAVE_STD(sort, Complected, "sort/complected", INT, 0);
    CHECK_AND_SAVE_STD(sort, Completed, "sort/completed", INT, 0);
    CHECK_AND_SAVE_STD(sort, Cursed, "sort/cursed", INT, 0);
    CHECK_AND_SAVE_STD(sort, Experimental, "sort/experimental", INT, 0);
    CHECK_AND_SAVE_STD(sort, Homebrew, "sort/homebrew", INT, 0);
    CHECK_AND_SAVE_STD(sort, Inprogress, "sort/inprogress", INT, 0);
    CHECK_AND_SAVE_STD(sort, Patched, "sort/patched", INT, 0);
    CHECK_AND_SAVE_STD(sort, Replay, "sort/replay", INT, 0);
    CHECK_AND_SAVE_STD(sort, Romhack, "sort/romhack", INT, 0);
    CHECK_AND_SAVE_STD(sort, Translated, "sort/translated", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, sort, Default, lang.MUXSORT.DEFAULT, "default", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Collection, lang.MUXSORT.COLLECTION, "collection", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, History, lang.MUXSORT.HISTORY, "history", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Abandoned, lang.MUXSORT.ABANDONED, "abandoned", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Alternate, lang.MUXSORT.ALTERNATE, "alternate", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Backlog, lang.MUXSORT.BACKLOG, "backlog", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Complected, lang.MUXSORT.COMPLECTED, "complected", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Completed, lang.MUXSORT.COMPLETED, "completed", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Cursed, lang.MUXSORT.CURSED, "cursed", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Experimental, lang.MUXSORT.EXPERIMENTAL, "experimental", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Homebrew, lang.MUXSORT.HOMEBREW, "homebrew", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Inprogress, lang.MUXSORT.INPROGRESS, "inprogress", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Patched, lang.MUXSORT.PATCHED, "patched", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Replay, lang.MUXSORT.REPLAY, "replay", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Romhack, lang.MUXSORT.ROMHACK, "romhack", NULL, 0);
    INIT_OPTION_ITEM(-1, sort, Translated, lang.MUXSORT.TRANSLATED, "translated", NULL, 0);

    char *increment_values = generate_number_string(0, 100, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droDefault_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droCollection_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droHistory_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droAbandoned_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droAlternate_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droBacklog_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droComplected_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droCompleted_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droCursed_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droExperimental_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droHomebrew_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droInprogress_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droPatched_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droReplay_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droRomhack_sort, increment_values);
    apply_theme_list_drop_down(&theme, ui_droTranslated_sort, increment_values);
    free(increment_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
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

    save_sort_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");
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
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

#define SORT(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_sort, UDATA);
    SORT_ELEMENTS
#undef SORT

    overlay_display();
}

int muxsort_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSORT.TITLE);
    init_muxsort(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_sort_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_option_next,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
