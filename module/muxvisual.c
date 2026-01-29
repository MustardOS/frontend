#include "muxshare.h"
#include "ui/ui_muxvisual.h"

#define UI_COUNT 20

#define VISUAL(NAME, ENUM, UDATA) static int NAME##_original;
VISUAL_ELEMENTS
#undef VISUAL

static int overlay_count;

static void show_help() {
    struct help_msg help_messages[] = {
#define VISUAL(NAME, ENUM, UDATA) { ui_lbl##NAME##_visual, lang.MUXVISUAL.HELP.ENUM },
            VISUAL_ELEMENTS
#undef VISUAL
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define VISUAL(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_visual);
    VISUAL_ELEMENTS
#undef VISUAL

    OverlayTransparency_original = pct_to_int(lv_dropdown_get_selected(ui_droOverlayTransparency_visual), 0, 100);
}

static void restore_visual_options(void) {
#define VISUAL(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_visual, config.VISUAL.ENUM);
    VISUAL_ELEMENTS
#undef VISUAL

    lv_dropdown_set_selected(ui_droOverlayImage_visual, (config.VISUAL.OVERLAYIMAGE > overlay_count) ? 0 : config.VISUAL.OVERLAYIMAGE);
    lv_dropdown_set_selected(ui_droOverlayTransparency_visual, int_to_pct(config.VISUAL.OVERLAYTRANSPARENCY, 0, 100));
}

static void save_visual_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(visual, Battery, "visual/battery", INT, 0);
    CHECK_AND_SAVE_STD(visual, Clock, "visual/clock", INT, 0);
    CHECK_AND_SAVE_STD(visual, Network, "visual/network", INT, 0);
    CHECK_AND_SAVE_STD(visual, Name, "visual/name", INT, 0);
    CHECK_AND_SAVE_STD(visual, Dash, "visual/dash", INT, 0);
    CHECK_AND_SAVE_STD(visual, FriendlyFolder, "visual/friendlyfolder", INT, 0);
    CHECK_AND_SAVE_STD(visual, TheTitleFormat, "visual/thetitleformat", INT, 0);
    CHECK_AND_SAVE_STD(visual, TitleIncludeRootDrive, "visual/titleincluderootdrive", INT, 0);
    CHECK_AND_SAVE_STD(visual, FolderItemCount, "visual/folderitemcount", INT, 0);
    CHECK_AND_SAVE_STD(visual, DisplayEmptyFolder, "visual/folderempty", INT, 0);
    CHECK_AND_SAVE_STD(visual, MenuCounterFolder, "visual/counterfolder", INT, 0);
    CHECK_AND_SAVE_STD(visual, MenuCounterFile, "visual/counterfile", INT, 0);
    CHECK_AND_SAVE_STD(visual, Hidden, "visual/hidden", INT, 0);
    CHECK_AND_SAVE_STD(visual, ContentCollect, "visual/contentcollect", INT, 0);
    CHECK_AND_SAVE_STD(visual, ContentHistory, "visual/contenthistory", INT, 0);
    CHECK_AND_SAVE_STD(visual, GroupTags, "visual/grouptags", INT, 0);
    CHECK_AND_SAVE_STD(visual, PinnedCollect, "visual/pinnedcollect", INT, 0);
    CHECK_AND_SAVE_STD(visual, DropHistory, "visual/drophistory", INT, 0);
    CHECK_AND_SAVE_STD(visual, OverlayImage, "visual/overlayimage", INT, 0);
    CHECK_AND_SAVE_PCT(visual, OverlayTransparency, "visual/overlaytransparency", INT, 0, 100);

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

    char *visual_names[] = {
            lang.MUXVISUAL.NAME.FULL,
            lang.MUXVISUAL.NAME.REM_SQ,
            lang.MUXVISUAL.NAME.REM_PA,
            lang.MUXVISUAL.NAME.REM_SQPA
    };

    char *tag_group[] = {
            lang.GENERIC.DISABLED,
            lang.MUXVISUAL.GROUPTAGS.ALPHA,
            lang.MUXVISUAL.GROUPTAGS.TAG
    };

    INIT_OPTION_ITEM(-1, visual, Battery, lang.MUXVISUAL.BATTERY, "battery", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, Clock, lang.MUXVISUAL.CLOCK, "clock", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, Network, lang.MUXVISUAL.NETWORK, "network", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, Name, lang.MUXVISUAL.NAME.TITLE, "name", visual_names, 4);
    INIT_OPTION_ITEM(-1, visual, Dash, lang.MUXVISUAL.DASH, "dash", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, FriendlyFolder, lang.MUXVISUAL.FRIENDLYFOLDER, "friendlyfolder", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, TheTitleFormat, lang.MUXVISUAL.THETITLEFORMAT, "thetitleformat", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, TitleIncludeRootDrive, lang.MUXVISUAL.TITLEINCLUDEROOTDRIVE, "titleincluderootdrive", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, FolderItemCount, lang.MUXVISUAL.FOLDERITEMCOUNT, "folderitemcount", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, DisplayEmptyFolder, lang.MUXVISUAL.DISPLAYEMPTYFOLDER, "folderempty", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, MenuCounterFolder, lang.MUXVISUAL.MENUCOUNTERFOLDER, "counterfolder", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, MenuCounterFile, lang.MUXVISUAL.MENUCOUNTERFILE, "counterfile", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, Hidden, lang.MUXVISUAL.HIDDEN, "hidden", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, ContentCollect, lang.MUXVISUAL.CONTENTCOLLECT, "collection", toggle_icon_visible, 3);
    INIT_OPTION_ITEM(-1, visual, ContentHistory, lang.MUXVISUAL.CONTENTHISTORY, "history", toggle_icon_visible, 3);
    INIT_OPTION_ITEM(-1, visual, GroupTags, lang.MUXVISUAL.GROUPTAGS.TITLE, "grouptags", tag_group, 3);
    INIT_OPTION_ITEM(-1, visual, PinnedCollect, lang.MUXVISUAL.PINNEDCOLLECT, "pinnedcollect", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, DropHistory, lang.MUXVISUAL.DROPHISTORY, "drophistory", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, OverlayImage, lang.MUXVISUAL.OVERLAY.IMAGE, "overlayimage", NULL, 0);
    INIT_OPTION_ITEM(-1, visual, OverlayTransparency, lang.MUXVISUAL.OVERLAY.TRANSPARENCY, "overlaytransparency", NULL, 0);

    overlay_count = load_overlay_set(ui_droOverlayImage_visual);

    char *pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droOverlayTransparency_visual, pct_values);
    free(pct_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!device.BOARD.HASNETWORK) HIDE_OPTION_ITEM(visual, Network);
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

    save_visual_options();

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

#define VISUAL(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_visual, UDATA);
    VISUAL_ELEMENTS
#undef VISUAL

    overlay_display();
}

int muxvisual_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXVISUAL.TITLE);
    init_muxvisual(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_visual_options();
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
