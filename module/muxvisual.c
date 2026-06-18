#include "muxshare.h"
#include "ui/ui_muxvisual.h"

static int save_mode = 0;
static int pending_sort = 0;

static mux_dialogue save_dlg;

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;
    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
}

#define VISUAL(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(VISUAL_ELEMENTS)
};
#undef VISUAL

#define VISUAL(NAME, ENUM, UDATA) static int NAME##_original;
VISUAL_ELEMENTS
#undef VISUAL

static int any_visual_modified(void) {
#define VISUAL(NAME, ENUM, UDATA) if (lv_dropdown_get_selected(ui_dro##NAME##_visual) != NAME##_original) return 1;
    VISUAL_ELEMENTS
#undef VISUAL
    return 0;
}

static int overlay_count;

static void show_help(void) {
    struct help_msg help_messages[] = {
#define VISUAL(NAME, ENUM, UDATA) { UDATA, lang.MUXVISUAL.HELP.ENUM },
            VISUAL_ELEMENTS
#undef VISUAL
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define VISUAL(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_visual);
    VISUAL_ELEMENTS
#undef VISUAL
}

static void restore_visual_options(void) {
#define VISUAL(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_visual, config.VISUAL.ENUM);
    VISUAL_ELEMENTS
#undef VISUAL

    lv_dropdown_set_selected(ui_droOverlayImage_visual, (config.VISUAL.OVERLAYIMAGE > overlay_count) ? 0 : config.VISUAL.OVERLAYIMAGE);
    lv_dropdown_set_selected(ui_droOverlayTransparency_visual, int_to_pct(config.VISUAL.OVERLAYTRANSPARENCY, 0, 255));
}

static void save_visual_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(visual, Battery, "visual/battery", INT, 0);
    CHECK_AND_SAVE_STD(visual, Clock, "visual/clock", INT, 0);
    CHECK_AND_SAVE_STD(visual, Network, "visual/network", INT, 0);
    CHECK_AND_SAVE_STD(visual, Bluetooth, "visual/bluetooth", INT, 0);
    CHECK_AND_SAVE_STD(visual, HeaderTitle, "visual/headertitle", INT, 0);
    CHECK_AND_SAVE_STD(visual, DialogueTransition, "visual/dialoguetransition", INT, 0);
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
    CHECK_AND_SAVE_STD(visual, MixedContent, "visual/mixedcontent", INT, 0);
    CHECK_AND_SAVE_STD(visual, ForwardHistory, "visual/forwardhistory", INT, 0);
    CHECK_AND_SAVE_STD(visual, NameScroll, "visual/namescroll", INT, 0);
    CHECK_AND_SAVE_STD(visual, RenderShadows, "visual/shadow", INT, 0);
    CHECK_AND_SAVE_STD(visual, OverlayImage, "visual/overlayimage", INT, 0);

    {
        int ot_current = lv_dropdown_get_selected(ui_droOverlayTransparency_visual);
        if (ot_current != OverlayTransparency_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "visual/overlaytransparency", "w", INT,
                               pct_to_int(ot_current, 0, 255));
        }
    }

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

    char *scroll_mode[] = {
            lang.MUXVISUAL.SCROLL_MODE.DISABLED,
            lang.MUXVISUAL.SCROLL_MODE.CONTINUOUS,
            lang.MUXVISUAL.SCROLL_MODE.BOUNCE
    };

    char *dialogue_transition[] = {
            lang.MUXCONTENT.BOX_ART.TRANSITION.FADE_IN,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_RIGHT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_LEFT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_UP,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_DOWN,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_RIGHT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_LEFT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_UP,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_DOWN,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_RIGHT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_LEFT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_UP,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_DOWN,
            lang.GENERIC.DISABLED
    };

    INIT_OPTION_ITEM(-1, visual, Sort, lang.MUXVISUAL.SORT, "sort", NULL, 0);
    INIT_OPTION_ITEM(-1, visual, Battery, lang.MUXVISUAL.BATTERY, "battery", battery_display, 3);
    INIT_OPTION_ITEM(-1, visual, Clock, lang.MUXVISUAL.CLOCK, "clock", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, Network, lang.MUXVISUAL.NETWORK, "network", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, Bluetooth, lang.MUXVISUAL.BLUETOOTH, "bluetooth", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, HeaderTitle, lang.MUXVISUAL.HEADERTITLE, "headertitle", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, DialogueTransition, lang.MUXVISUAL.DIALOGUETRANSITION, "dialoguetransition", dialogue_transition, 14);
    INIT_OPTION_ITEM(-1, visual, Name, lang.MUXVISUAL.NAME.TITLE, "name", visual_names, 4);
    INIT_OPTION_ITEM(-1, visual, NameScroll, lang.MUXVISUAL.NAMESCROLL, "namescroll", scroll_mode, 3);
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
    INIT_OPTION_ITEM(-1, visual, MixedContent, lang.MUXVISUAL.MIXEDCONTENT, "mixedcontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, ForwardHistory, lang.MUXVISUAL.FORWARDHISTORY, "forwardhistory", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, OverlayImage, lang.MUXVISUAL.OVERLAY.IMAGE, "overlayimage", NULL, 0);
    INIT_OPTION_ITEM(-1, visual, OverlayTransparency, lang.MUXVISUAL.OVERLAY.TRANSPARENCY, "overlaytransparency", NULL, 0);
    INIT_OPTION_ITEM(-1, visual, RenderShadows, lang.MUXVISUAL.RENDERSHADOWS, "shadow", disabled_enabled, 2);

    overlay_count = load_overlay_set(ui_droOverlayImage_visual);

    char *pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droOverlayTransparency_visual, pct_values);
    free(pct_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!device.BOARD.HASNETWORK) HIDE_OPTION_ITEM(visual, Network);
    if (!device.BOARD.HASBLUETOOTH) HIDE_OPTION_ITEM(visual, Bluetooth);
}


static void refresh_overlay_preview(void) {
    struct _lv_obj_t *focused = lv_group_get_focused(ui_group);

    if (focused == ui_lblOverlayImage_visual) {
        if (overlay_image) {
            lv_obj_del(overlay_image);
            overlay_image = NULL;
        }

        int16_t saved = config.VISUAL.OVERLAYIMAGE;
        config.VISUAL.OVERLAYIMAGE = (int16_t) lv_dropdown_get_selected(ui_droOverlayImage_visual);

        overlay_image = lv_obj_create(ui_screen);
        lv_obj_remove_style_all(overlay_image);
        lv_obj_clear_flag(overlay_image, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        load_overlay_image(ui_screen, overlay_image);

        config.VISUAL.OVERLAYIMAGE = saved;
    }

    if (overlay_image) {
        int opa = pct_to_int(lv_dropdown_get_selected(ui_droOverlayTransparency_visual), 0, 255);
        lv_obj_set_style_bg_img_opa(overlay_image, opa, MU_OBJ_MAIN_DEFAULT);
    }
}

static void handle_option_prev(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
    refresh_overlay_preview();
}

static void handle_option_next(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), +1);
    refresh_overlay_preview();
}

static void handle_a(void) {
    if (msgbox_active) return;

    if (save_mode) {
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (pending_sort) {
            pending_sort = 0;

            if (opt == MUX_UNSAVED_SAVE) save_visual_options();
            play_sound(SND_CONFIRM);

            load_mux("sort");
            mux_input_stop();

            return;
        }

        if (opt == MUX_UNSAVED_SAVE) save_visual_options();

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");

        mux_input_stop();

        return;
    }

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lblSort_visual) {
        if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_visual_modified()) {
            pending_sort = 1;
            show_save_dialog();

            return;
        }

        play_sound(SND_CONFIRM);

        save_visual_options();
        load_mux("sort");

        mux_input_stop();
    } else {
        handle_option_next();
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_visual_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);

    save_visual_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call || save_mode) return;

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

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
