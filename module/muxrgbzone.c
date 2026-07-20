#include "muxshare.h"
#include "ui/ui_muxrgbzone.h"

#define RGBZONE(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(RGBZONE_ELEMENTS) };
#undef RGBZONE

#define RGBZONE(NAME, UDATA) static int NAME##_original;
RGBZONE_ELEMENTS
#undef RGBZONE

typedef struct {
    const char *code;
    const char *label;
    int16_t *colour_cfg;
    int16_t *bright_cfg;
    const char *colour_path;
    const char *bright_path;
} rgb_zone_field_t;

#define RGBZONE_SEL_PATH "/tmp/rgb_zone_sel"

static rgb_zone_field_t current_zone;
static int current_zone_valid = 0;
static char current_zone_udata[32];

static void resolve_current_zone(void) {
    char *code = read_line_char_from(RGBZONE_SEL_PATH, 1);
    if (!code || !*code) {
        free(code);
        current_zone_valid = 0;
        return;
    }

    const char *board = device.board.name;
    const int dual_right = board && strcmp(board, "tui-brick-pro") == 0;

    if (strcmp(code, "l") == 0) {
        current_zone = (rgb_zone_field_t) {
            "l",
            dual_right ? lang.muxrgb.zone_l_arc1 : lang.muxrgb.zone_l,
            &config.settings.rgb.colour_l,
            &config.settings.rgb.bright_l,
            "settings/rgb/colour_l",
            "settings/rgb/bright_l",
        };
    } else if (strcmp(code, "r") == 0) {
        current_zone = (rgb_zone_field_t) {
            "r",
            dual_right ? lang.muxrgb.zone_l_arc2 : lang.muxrgb.zone_r,
            &config.settings.rgb.colour_r,
            &config.settings.rgb.bright_r,
            "settings/rgb/colour_r",
            "settings/rgb/bright_r",
        };
    } else if (strcmp(code, "m") == 0) {
        current_zone = (rgb_zone_field_t) {
            "m",
            lang.muxrgb.zone_m,
            &config.settings.rgb.colour_m,
            &config.settings.rgb.bright_m,
            "settings/rgb/colour_m",
            "settings/rgb/bright_m",
        };
    } else if (strcmp(code, "f1") == 0) {
        current_zone = (rgb_zone_field_t) {
            "f1",
            lang.muxrgb.zone_f1,
            &config.settings.rgb.colour_f1,
            &config.settings.rgb.bright_f1,
            "settings/rgb/colour_f1",
            "settings/rgb/bright_f1",
        };
    } else if (strcmp(code, "f2") == 0) {
        current_zone = (rgb_zone_field_t) {
            "f2",
            lang.muxrgb.zone_f2,
            &config.settings.rgb.colour_f2,
            &config.settings.rgb.bright_f2,
            "settings/rgb/colour_f2",
            "settings/rgb/bright_f2",
        };
    } else if (strcmp(code, "rs1") == 0) {
        current_zone = (rgb_zone_field_t) {
            "rs1",
            lang.muxrgb.zone_rs1,
            &config.settings.rgb.colour_rs1,
            &config.settings.rgb.bright_rs1,
            "settings/rgb/colour_rs1",
            "settings/rgb/bright_rs1",
        };
    } else if (strcmp(code, "rs2") == 0) {
        current_zone = (rgb_zone_field_t) {
            "rs2",
            lang.muxrgb.zone_rs2,
            &config.settings.rgb.colour_rs2,
            &config.settings.rgb.bright_rs2,
            "settings/rgb/colour_rs2",
            "settings/rgb/bright_rs2",
        };
    } else if (strcmp(code, "shl1") == 0) {
        current_zone = (rgb_zone_field_t) {
            "shl1",
            lang.muxrgb.zone_shl1,
            &config.settings.rgb.colour_shl1,
            &config.settings.rgb.bright_shl1,
            "settings/rgb/colour_shl1",
            "settings/rgb/bright_shl1",
        };
    } else if (strcmp(code, "shl2") == 0) {
        current_zone = (rgb_zone_field_t) {
            "shl2",
            lang.muxrgb.zone_shl2,
            &config.settings.rgb.colour_shl2,
            &config.settings.rgb.bright_shl2,
            "settings/rgb/colour_shl2",
            "settings/rgb/bright_shl2",
        };
    } else if (strcmp(code, "shr2") == 0) {
        current_zone = (rgb_zone_field_t) {
            "shr2",
            lang.muxrgb.zone_shr2,
            &config.settings.rgb.colour_shr2,
            &config.settings.rgb.bright_shr2,
            "settings/rgb/colour_shr2",
            "settings/rgb/bright_shr2",
        };
    } else if (strcmp(code, "shr1") == 0) {
        current_zone = (rgb_zone_field_t) {
            "shr1",
            lang.muxrgb.zone_shr1,
            &config.settings.rgb.colour_shr1,
            &config.settings.rgb.bright_shr1,
            "settings/rgb/colour_shr1",
            "settings/rgb/bright_shr1",
        };
    } else {
        free(code);
        current_zone_valid = 0;
        return;
    }

    free(code);

    snprintf(current_zone_udata, sizeof current_zone_udata, "zone_%s", current_zone.code);
    current_zone_valid = 1;
}

static void rgb_apply(void) {
    const char *argv[2];
    argv[0] = RGBLED_BIN;
    argv[1] = "restore";
    run_exec(argv, 2, 0, 0, NULL, NULL);
}

static void init_dropdown_settings(void) {
#define RGBZONE(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_rgbzone);
    RGBZONE_ELEMENTS
#undef RGBZONE
}

static char **build_colour_options(int *count) {
    char **out = calloc(rgb_colour_count, sizeof(char *));
    if (!out) {
        *count = 0;
        return NULL;
    }

    for (size_t i = 0; i < rgb_colour_count; i++)
        out[i] = strdup(rgb_colours[i].name);
    *count = (int) rgb_colour_count;

    return out;
}

static void free_string_array(char **arr, const int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++)
        free(arr[i]);
    free(arr);
}

static void restore_rgbzone_options(void) {
    lv_dropdown_set_selected(ui_dro_colour_rgbzone, *current_zone.colour_cfg);
    lv_dropdown_set_selected(ui_dro_bright_rgbzone, int_to_pct(*current_zone.bright_cfg, 0, 255));
}

static void save_rgbzone_options(const int toast_vis) {
    int is_modified = 0;

    const int cur_colour = lv_dropdown_get_selected(ui_dro_colour_rgbzone);
    if (cur_colour != colour_original) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof path, "/opt/muos/config/%s", current_zone.colour_path);
        write_text_to_file(path, "w", INT, cur_colour);
        is_modified++;
    }

    const int bright_pct = lv_dropdown_get_selected(ui_dro_bright_rgbzone);
    const int cur_bright = pct_to_int(bright_pct, 0, 255);
    if (cur_bright != bright_original) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof path, "/opt/muos/config/%s", current_zone.bright_path);
        write_text_to_file(path, "w", INT, cur_bright);
        is_modified++;
    }

    if (is_modified > 0) {
        toast_message(lang.generic.saving, toast_vis);
        refresh_config = 1;
    }

    rgb_apply();
}

static int any_rgbzone_modified(void) {
    if ((int) lv_dropdown_get_selected(ui_dro_colour_rgbzone) != colour_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_bright_rgbzone) != bright_original) return 1;

    return 0;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    int colour_count = 0;
    char **colour_options = build_colour_options(&colour_count);

    char *bright_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);

    INIT_OPTION_ITEM(-1, rgbzone, colour, lang.muxrgb.colour, "colour", colour_options, colour_count);
    INIT_OPTION_ITEM(-1, rgbzone, bright, lang.muxrgb.bright, "bright", NULL, 0);

    apply_theme_list_drop_down(&theme, ui_lbl_bright_rgbzone, ui_dro_bright_rgbzone, bright_values);
    free(bright_values);

    free_string_array(colour_options, colour_count);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 2, 0, 1);
}

static int save_mode = 0;
static mux_dialogue save_dlg;

static void hide_save_dialog(void) {
    dialogue_dismiss(&save_mode, &save_dlg);
}

static void handle_option_prev(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_dpad_up(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down();
}

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_rgbzone_options(tst_wait_f);

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, current_zone_udata);
        load_mux("rgb");

        mux_input_stop();
        return;
    }

    if (msgbox_active || block_input || hold_call) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_b(void) {
    if (block_input || hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (dialogue_guard_unsaved(&save_mode, &save_dlg, &theme, any_rgbzone_modified())) return;

    play_sound(snd_back);
    if (any_rgbzone_modified()) save_rgbzone_options(tst_wait_f);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, current_zone_udata);
    load_mux("rgb");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || block_input || hold_call) return;

    save_rgbzone_options(tst_wait_m);
    init_dropdown_settings();
}

static void handle_y(void) {
    if (msgbox_active || save_mode || block_input || hold_call) return;

    lv_dropdown_set_selected(ui_dro_colour_rgbzone, 0);
    save_rgbzone_options(tst_wait_m);
    init_dropdown_settings();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.set, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, (char *) rgb_colours[0].name, 0},
                                  {NULL, NULL, 0}});

#define RGBZONE(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_rgbzone, UDATA);
    RGBZONE_ELEMENTS
#undef RGBZONE

    overlay_display();
}

int muxrgbzone_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    resolve_current_zone();
    if (!current_zone_valid) {
        load_mux("rgb");
        return 0;
    }

    init_ui_common_screen(&theme, &device, &lang, current_zone.label);
    init_muxrgbzone(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_rgbzone_options();
    init_dropdown_settings();

    nav_silent = 1;

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);
    nav_silent = 0;

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
