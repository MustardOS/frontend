#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "../common/soundfont.h"

static int preview_toast = 0;

static void show_help(void) {
    show_info_box(lang.muxsoundfont.title, lang.muxsoundfont.help, 0);
}

static void populate_soundfonts(void) {
    char **names = NULL;
    size_t count = 0;

    if (soundfont_scan(&names, &count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        return;
    }

    for (size_t i = 0; i < count; i++)
        add_item(&items, &item_count, names[i], names[i], "", content_type_item);

    free_array(names, count);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static int is_active_soundfont(const char *name) {
    const char *saved = config.settings.general.soundfont;

    return *saved && !strcasecmp(name, saved);
}

static int saved_item_index(void) {
    for (size_t i = 0; i < item_count; i++)
        if (is_active_soundfont(items[i].name)) return (int) i;

    return 0;
}

static void create_soundfont_items(void) {
    reset_ui_groups();
    populate_soundfonts();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        lv_obj_t *ui_pnl_soundfont = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_soundfont);

        lv_obj_t *ui_lbl_soundfont_item = lv_label_create(ui_pnl_soundfont);
        apply_theme_list_item(&theme, ui_lbl_soundfont_item, items[i].display_name);

        lv_obj_t *ui_lbl_soundfont_glyph = lv_img_create(ui_pnl_soundfont);
        const char *glyph = is_active_soundfont(items[i].name) ? "current" : "soundfont";
        apply_theme_list_glyph(&theme, ui_lbl_soundfont_glyph, mux_module, glyph);

        lv_group_add_obj(ui_group, ui_lbl_soundfont_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_soundfont_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_soundfont);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_soundfont_item, ui_lbl_soundfont_glyph, items[i].name);
        apply_text_long_dot(&theme, ui_lbl_soundfont_item);
    }
}

static void close_screen(void) {
    soundfont_preview_stop();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "soundfont");
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call || !ui_count_static) return;

    play_sound(snd_confirm);

    toast_message(lang.generic.saving, tst_wait_f);
    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/general/soundfont", CHAR, items[current_item_index].name);

    refresh_config = 1;

    close_screen();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    close_screen();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || !ui_count_static) return;

    const char *selected = items[current_item_index].name;
    const int state = soundfont_preview_state();

    if (state != soundfont_idle && !strcasecmp(soundfont_preview_name(), selected)) {
        play_sound(snd_back);
        soundfont_preview_stop();
        return;
    }

    play_sound(snd_confirm);
    soundfont_preview_start(selected);

    if (soundfont_preview_state() == soundfont_loading) {
        preview_toast = 1;
        toast_message(lang.muxsoundfont.loading, tst_wait_l);
    } else {
        play_sound(snd_error);
        toast_message(lang.muxsoundfont.error_preview, tst_wait_m);
    }
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void ui_refresh_task(lv_timer_t *timer) {
    ui_gen_refresh_task(timer);

    soundfont_preview_poll();

    if (preview_toast && soundfont_preview_state() == soundfont_playing) preview_toast = 0;

    if (preview_toast && soundfont_preview_state() == soundfont_failed) {
        preview_toast = 0;

        play_sound(snd_error);
        toast_message(lang.muxsoundfont.error_preview, tst_wait_m);
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.muxsoundfont.preview, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxsoundfont_main(void) {
    preview_toast = 0;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxsoundfont.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_soundfont_items();
    init_elements();

    if (ui_count_static > 0) {
        LOG_SUCCESS(mux_module, "%d Soundfont%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        list_nav_next(saved_item_index());
        lv_obj_update_layout(ui_pnl_content);
    } else {
        LOG_ERROR(mux_module, "No Soundfonts Detected!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxsoundfont.none);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
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
    orientation_introduce(mux_module, lang.muxsoundfont.title, lang.muxsoundfont.overview);

    mux_input_task(&input_opts);

    soundfont_preview_stop();
    free_items(&items, &item_count);

    return 0;
}
