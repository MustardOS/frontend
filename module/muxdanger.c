#include "muxshare.h"
#include "ui/ui_muxdanger.h"

#define DANGER(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(DANGER_ELEMENTS) };
#undef DANGER

#define DANGER(NAME, UDATA) static int NAME##_original;
DANGER_ELEMENTS
#undef DANGER

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define DANGER(NAME, UDATA) {UDATA, lang.muxdanger.help.NAME},
        DANGER_ELEMENTS
#undef DANGER
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define DANGER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_danger);
    DANGER_ELEMENTS
#undef DANGER
}

static void restore_danger_options(void) {
    lv_dropdown_set_selected(ui_dro_io_stats_danger, config.danger.io_stats);
    lv_dropdown_set_selected(ui_dro_idle_flush_danger, config.danger.idle_flush);
    lv_dropdown_set_selected(ui_dro_child_first_danger, config.danger.child_first);
    lv_dropdown_set_selected(ui_dro_tune_scale_danger, config.danger.tune_scale);
    lv_dropdown_set_selected(ui_dro_page_cluster_danger, config.danger.page_cluster);

    map_drop_down_to_index(ui_dro_vm_swap_danger, config.danger.vm_swap, four_values, 25, 3);
    map_drop_down_to_index(ui_dro_dirty_ratio_danger, config.danger.dirty_ratio, four_values, 25, 4);
    map_drop_down_to_index(ui_dro_dirty_back_danger, config.danger.dirty_back, four_values, 25, 1);
    map_drop_down_to_index(ui_dro_cache_pressure_danger, config.danger.cache, four_values, 25, 15);
    map_drop_down_to_index(ui_dro_no_merge_danger, config.danger.merge, merge_values, 3, 2);
    map_drop_down_to_index(ui_dro_nr_requests_danger, config.danger.requests, request_values, 8, 1);
    map_drop_down_to_index(ui_dro_read_ahead_danger, config.danger.read_ahead, read_ahead_values, 9, 6);
    map_drop_down_to_index(ui_dro_time_slice_danger, config.danger.time_slice, time_slice_values, 11, 1);

    lv_dropdown_set_selected(ui_dro_card_mode_danger, strcasecmp(config.danger.card_mode, "deadline") != 0);
    lv_dropdown_set_selected(ui_dro_state_danger, strcasecmp(config.danger.state, "mem") != 0);
}

static void save_danger_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(danger, io_stats, "danger/iostats", INT, 0);
    CHECK_AND_SAVE_STD(danger, idle_flush, "danger/idle_flush", INT, 0);
    CHECK_AND_SAVE_STD(danger, child_first, "danger/child_first", INT, 0);
    CHECK_AND_SAVE_STD(danger, tune_scale, "danger/tune_scale", INT, 0);
    CHECK_AND_SAVE_STD(danger, page_cluster, "danger/page_cluster", INT, 0);

    CHECK_AND_SAVE_MAP(danger, vm_swap, "danger/vmswap", four_values, 25, 3);
    CHECK_AND_SAVE_MAP(danger, dirty_ratio, "danger/dirty_ratio", four_values, 25, 4);
    CHECK_AND_SAVE_MAP(danger, dirty_back, "danger/dirty_back_ratio", four_values, 25, 1);
    CHECK_AND_SAVE_MAP(danger, cache_pressure, "danger/cache_pressure", four_values, 25, 15);
    CHECK_AND_SAVE_MAP(danger, no_merge, "danger/nomerges", merge_values, 3, 2);
    CHECK_AND_SAVE_MAP(danger, nr_requests, "danger/nr_requests", request_values, 8, 1);
    CHECK_AND_SAVE_MAP(danger, read_ahead, "danger/read_ahead", read_ahead_values, 9, 6);
    CHECK_AND_SAVE_MAP(danger, time_slice, "danger/time_slice", time_slice_values, 11, 1);

    CHECK_AND_SAVE_VAL(danger, card_mode, "danger/cardmode", CHAR, cardmode_values);
    CHECK_AND_SAVE_VAL(danger, state, "danger/state", CHAR, state_values);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *read_ahead_options[] = {"64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384"};
    char *time_slice_values[] = {"10", "100", "200", "300", "400", "500", "600", "700", "800", "900", "1000"};

    char *cardmode_options[] = {"deadline", "noop"};
    char *state_options[] = {"mem", "freeze"};

    INIT_OPTION_ITEM(-1, danger, vm_swap, lang.muxdanger.vmswap, "vmswap", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, dirty_ratio, lang.muxdanger.dirtyratio, "dirty-ratio", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, dirty_back, lang.muxdanger.dirtyback, "dirty-back", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, cache_pressure, lang.muxdanger.cachepressure, "cache", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, no_merge, lang.muxdanger.nomerge, "merge", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, nr_requests, lang.muxdanger.nrrequests, "requests", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, read_ahead, lang.muxdanger.readahead, "readahead", read_ahead_options, 9);
    INIT_OPTION_ITEM(-1, danger, page_cluster, lang.muxdanger.pagecluster, "cluster", NULL, 0);
    INIT_OPTION_ITEM(-1, danger, time_slice, lang.muxdanger.timeslice, "timeslice", time_slice_values, 11);
    INIT_OPTION_ITEM(-1, danger, io_stats, lang.muxdanger.iostats, "iostats", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, idle_flush, lang.muxdanger.idleflush, "idleflush", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, child_first, lang.muxdanger.childfirst, "child", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, tune_scale, lang.muxdanger.tunescale, "tunescale", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, danger, card_mode, lang.muxdanger.cardmode, "cardmode", cardmode_options, 2);
    INIT_OPTION_ITEM(-1, danger, state, lang.muxdanger.state, "state", state_options, 2);

    char *four_values = generate_number_string(0, 100, 4, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_vm_swap_danger, four_values);
    apply_theme_list_drop_down(&theme, ui_dro_dirty_ratio_danger, four_values);
    apply_theme_list_drop_down(&theme, ui_dro_dirty_back_danger, four_values);
    apply_theme_list_drop_down(&theme, ui_dro_cache_pressure_danger, four_values);
    free(four_values);

    char *merge_values = generate_number_string(0, 2, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_no_merge_danger, merge_values);
    free(merge_values);

    char *request_values = generate_number_string(64, 512, 64, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_nr_requests_danger, request_values);
    free(request_values);

    char *cluster_values = generate_number_string(0, 10, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_page_cluster_danger, cluster_values);
    free(cluster_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }
    play_sound(snd_back);

    save_danger_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define DANGER(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_danger, UDATA);
    DANGER_ELEMENTS
#undef DANGER

    overlay_display();
}

int muxdanger_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxdanger.title);
    init_muxdanger(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_danger_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
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
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
