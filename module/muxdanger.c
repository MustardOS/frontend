#include "muxshare.h"
#include "ui/ui_muxdanger.h"

#define UI_COUNT 15

#define DANGER(NAME, UDATA) static int NAME##_original;
    DANGER_ELEMENTS
#undef DANGER

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblVmSwap_danger,        lang.MUXDANGER.HELP.VMSWAP},
            {ui_lblDirtyRatio_danger,    lang.MUXDANGER.HELP.DIRTYRATIO},
            {ui_lblDirtyBack_danger,     lang.MUXDANGER.HELP.DIRTYBACK},
            {ui_lblCachePressure_danger, lang.MUXDANGER.HELP.CACHE},
            {ui_lblNoMerge_danger,       lang.MUXDANGER.HELP.NOMERGE},
            {ui_lblNrRequests_danger,    lang.MUXDANGER.HELP.REQUESTS},
            {ui_lblReadAhead_danger,     lang.MUXDANGER.HELP.READAHEAD},
            {ui_lblPageCluster_danger,   lang.MUXDANGER.HELP.PAGECLUSTER},
            {ui_lblTimeSlice_danger,     lang.MUXDANGER.HELP.TIMESLICE},
            {ui_lblIoStats_danger,       lang.MUXDANGER.HELP.IOSTATS},
            {ui_lblIdleFlush_danger,     lang.MUXDANGER.HELP.IDLEFLUSH},
            {ui_lblChildFirst_danger,    lang.MUXDANGER.HELP.CHILDFIRST},
            {ui_lblTuneScale_danger,     lang.MUXDANGER.HELP.TUNESCALE},
            {ui_lblCardMode_danger,      lang.MUXDANGER.HELP.CARDMODE},
            {ui_lblState_danger,         lang.MUXDANGER.HELP.STATE},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define DANGER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_danger);
    DANGER_ELEMENTS
#undef DANGER
}

static void restore_danger_options(void) {
    lv_dropdown_set_selected(ui_droIoStats_danger, config.DANGER.IOSTATS);
    lv_dropdown_set_selected(ui_droIdleFlush_danger, config.DANGER.IDLEFLUSH);
    lv_dropdown_set_selected(ui_droChildFirst_danger, config.DANGER.CHILDFIRST);
    lv_dropdown_set_selected(ui_droTuneScale_danger, config.DANGER.TUNESCALE);
    lv_dropdown_set_selected(ui_droPageCluster_danger, config.DANGER.PAGECLUSTER);

    map_drop_down_to_index(ui_droVmSwap_danger, config.DANGER.VMSWAP, four_values, 25, 3);
    map_drop_down_to_index(ui_droDirtyRatio_danger, config.DANGER.DIRTYRATIO, four_values, 25, 4);
    map_drop_down_to_index(ui_droDirtyBack_danger, config.DANGER.DIRTYBACK, four_values, 25, 1);
    map_drop_down_to_index(ui_droCachePressure_danger, config.DANGER.CACHE, four_values, 25, 15);
    map_drop_down_to_index(ui_droNoMerge_danger, config.DANGER.MERGE, merge_values, 3, 2);
    map_drop_down_to_index(ui_droNrRequests_danger, config.DANGER.REQUESTS, request_values, 8, 1);
    map_drop_down_to_index(ui_droReadAhead_danger, config.DANGER.READAHEAD, read_ahead_values, 9, 6);
    map_drop_down_to_index(ui_droTimeSlice_danger, config.DANGER.TIMESLICE, time_slice_values, 11, 1);

    lv_dropdown_set_selected(ui_droCardMode_danger, !strcasecmp(config.DANGER.CARDMODE, "noop"));
    lv_dropdown_set_selected(ui_droState_danger, !strcasecmp(config.DANGER.STATE, "mem"));
}

static void save_danger_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(danger, IoStats, "danger/iostats", INT, 0);
    CHECK_AND_SAVE_STD(danger, IdleFlush, "danger/idle_flush", INT, 0);
    CHECK_AND_SAVE_STD(danger, ChildFirst, "danger/child_first", INT, 0);
    CHECK_AND_SAVE_STD(danger, TuneScale, "danger/tune_scale", INT, 0);
    CHECK_AND_SAVE_STD(danger, PageCluster, "danger/page_cluster", INT, 0);

    CHECK_AND_SAVE_MAP(danger, VmSwap, "danger/vmswap", four_values, 25, 3);
    CHECK_AND_SAVE_MAP(danger, DirtyRatio, "danger/dirty_ratio", four_values, 25, 4);
    CHECK_AND_SAVE_MAP(danger, DirtyBack, "danger/dirty_back_ratio", four_values, 25, 1);
    CHECK_AND_SAVE_MAP(danger, CachePressure, "danger/cache_pressure", four_values, 25, 15);
    CHECK_AND_SAVE_MAP(danger, NoMerge, "danger/nomerges", merge_values, 3, 2);
    CHECK_AND_SAVE_MAP(danger, NrRequests, "danger/nr_requests", request_values, 8, 1);
    CHECK_AND_SAVE_MAP(danger, ReadAhead, "danger/read_ahead", read_ahead_values, 9, 6);
    CHECK_AND_SAVE_MAP(danger, TimeSlice, "danger/time_slice", time_slice_values, 11, 1);

    CHECK_AND_SAVE_VAL(danger, CardMode, "danger/cardmode", CHAR, cardmode_values);
    CHECK_AND_SAVE_VAL(danger, State, "danger/state", CHAR, state_values);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);

        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *read_ahead_options[] = {"64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384"};
    char *time_slice_values[] = {"10", "100", "200", "300", "400", "500", "600", "700", "800", "900", "1000"};

    char *cardmode_options[] = {"deadline", "noop"};
    char *state_options[] = {"mem", "freeze"};

    INIT_OPTION_ITEM(-1, danger, VmSwap, lang.MUXDANGER.VMSWAP, "vmswap", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, DirtyRatio, lang.MUXDANGER.DIRTYRATIO, "dirty-ratio", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, DirtyBack, lang.MUXDANGER.DIRTYBACK, "dirty-back", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, CachePressure, lang.MUXDANGER.CACHE, "cache", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, NoMerge, lang.MUXDANGER.NOMERGE, "merge", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, NrRequests, lang.MUXDANGER.REQUESTS, "requests", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, ReadAhead, lang.MUXDANGER.READAHEAD, "readahead", read_ahead_options, 9);
    INIT_OPTION_ITEM(-1, danger, PageCluster, lang.MUXDANGER.PAGECLUSTER, "cluster", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, TimeSlice, lang.MUXDANGER.TIMESLICE, "timeslice", time_slice_values, 11);
    INIT_OPTION_ITEM(-1, danger, IoStats, lang.MUXDANGER.IOSTATS, "iostats", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, IdleFlush, lang.MUXDANGER.IDLEFLUSH, "idleflush", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, ChildFirst, lang.MUXDANGER.CHILDFIRST, "child", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, TuneScale, lang.MUXDANGER.TUNESCALE, "tunescale", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, CardMode, lang.MUXDANGER.CARDMODE, "cardmode", cardmode_options, 2);
    INIT_OPTION_ITEM(-1, danger, State, lang.MUXDANGER.STATE, "state", state_options, 2);

    char *four_values = generate_number_string(0, 100, 4, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droVmSwap_danger, four_values);
    apply_theme_list_drop_down(&theme, ui_droDirtyRatio_danger, four_values);
    apply_theme_list_drop_down(&theme, ui_droDirtyBack_danger, four_values);
    apply_theme_list_drop_down(&theme, ui_droCachePressure_danger, four_values);
    free(four_values);

    char *merge_values = generate_number_string(0, 2, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droNoMerge_danger, merge_values);
    free(merge_values);

    char *request_values = generate_number_string(64, 512, 64, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droNrRequests_danger, request_values);
    free(request_values);

    char *cluster_values = generate_number_string(0, 10, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droPageCluster_danger, cluster_values);
    free(cluster_values);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
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
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
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

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
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

    save_danger_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

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
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

#define DANGER(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_danger, UDATA);
    DANGER_ELEMENTS
#undef DANGER

    overlay_display();
}

static void ui_refresh_task(void) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxdanger_main(void) {
    init_module("muxdanger");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXDANGER.TITLE);
    init_muxdanger(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_danger_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
