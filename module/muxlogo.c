#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "../common/ui/empty_state.h"
#include "../common/task_exec.h"
#include "../common/ui/task_progress.h"
#include "../common/image.h"
#include "../common/ui/notify.h"

static int task_pending = 0;

static mux_dialogue fit_dlg;

static const char *fit_modes[] = {"fullscreen", "stretch", "crop", "original"};

static mux_dialogue reset_dlg;

static int skip_confirm = 0;
static mux_dialogue remove_dlg;

static void remove_logo(void) {
    if (!ui_count_static) return;

    const char *path = items[current_item_index].extra_data;

    if (!path || strncmp(path, INTERNAL_LOGOS, strlen(INTERNAL_LOGOS)) == 0) {
        notify_send(notify_warning, lang.generic.failed);
        return;
    }

    if (remove(path) != 0) {
        notify_send(notify_warning, lang.generic.failed);
        return;
    }

    write_text_to_file(MUOS_PIN_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));

    load_mux("logo");
    mux_input_stop();
}

static void run_logo_task(const char **argv, const int argc, const char *title) {
    const task_exec_spec spec = {
        .argv = argv,
        .argc = argc,
        .mode = task_mode_progress,
        .can_cancel = 0,
        .turbo = 1,
        .title = title,
    };

    if (task_exec_start(&spec) == 0) {
        task_pending = 1;
        task_progress_show();
    }
}

static void apply_logo(const char *mode) {
    const char *argv[] = {OPT_PATH "script/package/logo.sh", "set", items[current_item_index].extra_data, mode, NULL};

    run_logo_task(argv, 4, lang.muxlogo.task.set);
}

static void reset_logo(void) {
    const char *argv[] = {OPT_PATH "script/package/logo.sh", "clear", NULL};

    run_logo_task(argv, 2, lang.muxlogo.task.reset);
}

static void show_help(void) {
    show_info_box(TRS(items[current_item_index].display_name), TRS(lang.muxlogo.help), 0);
}

static void create_logo_items(void) {
    free_items(&items, &item_count);

    const char *sources[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};

    for (size_t s = 0; s < A_SIZE(sources) + 1; s++) {
        char dir[MAX_BUFFER_SIZE];

        if (s < A_SIZE(sources)) {
            if (!sources[s] || !*sources[s]) continue;
            snprintf(dir, sizeof(dir), "%s/%s", sources[s], USER_LOGOS);
        } else {
            snprintf(dir, sizeof(dir), "%s", INTERNAL_LOGOS);
        }

        remove_double_slashes(dir);

        struct dirent **entries;
        const int n = scandir(dir, &entries, NULL, alphasort);
        if (n < 0) continue;

        for (int i = 0; i < n; i++) {
            const char *file = entries[i]->d_name;
            const char *dot = strrchr(file, '.');

            if (dot && is_image_ext(dot + 1)) {
                char name[MAX_BUFFER_SIZE];
                snprintf(name, sizeof(name), "%.*s", (int) (dot - file), file);

                char path[MAX_BUFFER_SIZE];
                snprintf(path, sizeof(path), "%s/%s", dir, file);

                if (get_item_index_by_name(items, item_count, name, content_type_item) < 0)
                    add_item(&items, &item_count, name, name, path, content_type_item);
            }

            free(entries[i]);
        }

        free(entries);
    }

    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        lv_obj_t *ui_pnl_logo = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_logo);

        lv_obj_t *ui_lbl_logo_item = lv_label_create(ui_pnl_logo);
        apply_theme_list_item(&theme, ui_lbl_logo_item, items[i].display_name);

        lv_obj_t *ui_lbl_logo_item_glyph = lv_img_create(ui_pnl_logo);
        apply_theme_list_glyph(&theme, ui_lbl_logo_item_glyph, mux_module, "logo");

        lv_group_add_obj(ui_group, ui_lbl_logo_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_logo_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_logo);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_logo_item, ui_lbl_logo_item_glyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_lbl_logo_item);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void image_refresh(void) {
    if (!ui_count_static) {
        lv_img_set_src(ui_img_box, &ui_img_blank);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");

        return;
    }

    char *path = items[current_item_index].extra_data;

    lv_img_cache_invalidate_src(lv_img_get_src(ui_img_box));

    if (strcasecmp(box_image_previous_path, path) == 0) return;

    if (!file_exist(path)) {
        lv_img_set_src(ui_img_box, &ui_img_blank);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");

        return;
    }

    const struct image_settings image_settings = {
        path,
        6,
        validate_int16((int16_t) (device.mux.width * .45), "width"),
        validate_int16(device.mux.height, "height"),
        theme.image_list.pad_left,
        theme.image_list.pad_right,
        theme.image_list.pad_top,
        theme.image_list.pad_bottom
    };

    update_image(ui_img_box, image_settings);
    snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", path);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
    image_refresh();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void finish_task(void) {
    task_pending = 0;

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");
    load_mux("custom");

    mux_input_stop();
}

static void handle_a(void) {
    if (task_progress_handle_a()) {
        if (task_pending && !task_progress_active()) finish_task();
        return;
    }

    if (dialogue_active(&remove_dlg)) {
        const mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        dialogue_dismiss(&remove_dlg);

        if (opt == mux_remove_skip) skip_confirm = 1;
        if (opt != mux_remove_nah) remove_logo();

        return;
    }

    if (dialogue_active(&reset_dlg)) {
        const int confirmed = reset_dlg.selected == 0;
        dialogue_dismiss(&reset_dlg);

        if (confirmed) reset_logo();

        return;
    }

    if (dialogue_active(&fit_dlg)) {
        const int chosen = fit_dlg.selected >= 0 && fit_dlg.selected < (int) A_SIZE(fit_modes) ? fit_dlg.selected : 0;
        dialogue_dismiss(&fit_dlg);

        apply_logo(fit_modes[chosen]);

        return;
    }

    if (msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_confirm);
    dialogue_open(&fit_dlg, &theme);
}

static void handle_dpad_up(void) {
    if (dialogue_active(&remove_dlg)) {
        dialogue_handle_dpad(&remove_dlg, &theme, -1, 1);
        return;
    }

    if (dialogue_active(&reset_dlg)) {
        dialogue_handle_dpad(&reset_dlg, &theme, -1, 1);
        return;
    }

    if (dialogue_active(&fit_dlg)) {
        dialogue_handle_dpad(&fit_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&remove_dlg)) {
        dialogue_handle_dpad(&remove_dlg, &theme, +1, 1);
        return;
    }

    if (dialogue_active(&reset_dlg)) {
        dialogue_handle_dpad(&reset_dlg, &theme, +1, 1);
        return;
    }

    if (dialogue_active(&fit_dlg)) {
        dialogue_handle_dpad(&fit_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&remove_dlg)) {
        dialogue_handle_dpad_hold(&remove_dlg, &theme, -1, 1);
        return;
    }

    if (dialogue_active(&reset_dlg)) {
        dialogue_handle_dpad_hold(&reset_dlg, &theme, -1, 1);
        return;
    }

    if (dialogue_active(&fit_dlg)) {
        dialogue_handle_dpad_hold(&fit_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&remove_dlg)) {
        dialogue_handle_dpad_hold(&remove_dlg, &theme, +1, 1);
        return;
    }

    if (dialogue_active(&reset_dlg)) {
        dialogue_handle_dpad_hold(&reset_dlg, &theme, +1, 1);
        return;
    }

    if (dialogue_active(&fit_dlg)) {
        dialogue_handle_dpad_hold(&fit_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down_hold();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    task_progress_tick();

    if (ui_count_static > 0 && nav_moved) {
        image_refresh();
        adjust_gen_panel();

        lv_obj_invalidate(ui_pnl_box);
        nav_moved = 0;
    }
}

static void handle_b(void) {
    if (task_progress_handle_b()) return;
    if (hold_call) return;

    if (dialogue_active(&remove_dlg)) {
        dialogue_mark_cancelled(&remove_dlg);
        dialogue_dismiss(&remove_dlg);

        return;
    }

    if (dialogue_active(&reset_dlg)) {
        dialogue_mark_cancelled(&reset_dlg);
        dialogue_dismiss(&reset_dlg);

        return;
    }

    if (dialogue_active(&fit_dlg)) {
        dialogue_mark_cancelled(&fit_dlg);
        dialogue_dismiss(&fit_dlg);

        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");

    mux_input_stop();
}

static void handle_x(void) {
    if (dialogue_active(&fit_dlg) || dialogue_active(&reset_dlg) || dialogue_active(&remove_dlg) || msgbox_active
        || task_progress_active() || hold_call)
        return;
    if (!ui_count_static) return;

    play_sound(snd_confirm);

    if (config.settings.advanced.trust_remove || skip_confirm) {
        remove_logo();
        return;
    }

    dialogue_open(&remove_dlg, &theme);
}

static void handle_y(void) {
    if (dialogue_active(&fit_dlg) || dialogue_active(&reset_dlg) || dialogue_active(&remove_dlg) || msgbox_active
        || task_progress_active() || hold_call)
        return;

    play_sound(snd_confirm);
    dialogue_open(&reset_dlg, &theme);
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
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.remove, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.reset, 0},
                                  {NULL, NULL, 0}});

    overlay_display();

    if (!ui_count_static) {
        empty_state_show(lang.muxlogo.none, lang.muxlogo.guidance);

        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

int muxlogo_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxlogo.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_logo_items();
    init_elements();

    const char *fit_labels[] = {
        lang.muxlogo.fit.fullscreen, lang.muxlogo.fit.stretch, lang.muxlogo.fit.crop, lang.muxlogo.fit.original
    };

    dialogue_init(
        &fit_dlg, &theme, ui_screen, lang.muxlogo.fit.title, lang.muxlogo.fit.message, fit_labels, A_SIZE(fit_labels),
        lang.generic.select, lang.generic.cancel
    );

    const char *reset_labels[] = {lang.generic.yes, lang.generic.no};

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, lang.muxlogo.remove.message, lang.generic.select, NULL);

    dialogue_init(
        &reset_dlg, &theme, ui_screen, lang.muxlogo.reset.title, lang.muxlogo.reset.message, reset_labels,
        A_SIZE(reset_labels), lang.generic.select, NULL
    );

    task_progress_init(&theme, ui_screen);

    init_timer(ui_refresh_task, NULL);

    RESET_PATH(box_image_previous_path);

    if (ui_count_static) {
        gen_step_movement(0, +1, 1, 0, 1);
        image_refresh();
    }

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
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
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    orientation_introduce(mux_module, lang.muxlogo.title, lang.muxlogo.overview);

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
