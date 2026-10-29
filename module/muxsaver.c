#include "muxshare.h"
#include <common/platform/display.h>
#include <common/saver/type.h>
#include <common/ui/orientation.h>

#define SAVER_OPTION_MAX 4

typedef struct {
    int saver;
    const char *key;
    const char *glyph;
    int16_t *value;
    const char *label;
    char **choices;
    int count;
} saver_option_def;

typedef struct {
    const saver_option_def *def;
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *glyph;
    lv_obj_t *value;
    int original;
} saver_option_row;

static mux_dialogue save_dlg;
static saver_option_row rows[SAVER_OPTION_MAX];
static int row_count;
static int active_type;

static char *dvd_colour_options[] = {
    lang.muxsaver.choice.standard, lang.muxsaver.choice.cycle_bounce, lang.muxsaver.choice.rainbow
};
static char *dvd_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *star_count_options[] = {lang.muxsaver.choice.few, lang.muxsaver.choice.normal, lang.muxsaver.choice.many};
static char *star_trails_options[] = {
    lang.muxsaver.choice.automatic, lang.muxsaver.choice.always, lang.muxsaver.choice.never
};
static char *matrix_colour_options[] = {lang.muxsaver.choice.standard, lang.muxsaver.choice.white,
                                        lang.muxsaver.choice.red,      lang.muxsaver.choice.orange,
                                        lang.muxsaver.choice.amber,    lang.muxsaver.choice.green,
                                        lang.muxsaver.choice.cyan,     lang.muxsaver.choice.blue,
                                        lang.muxsaver.choice.purple,   lang.muxsaver.choice.pink};
static char *matrix_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *firefly_count_options[] = {"24", "48", "96"};
static char *firefly_colour_options[] = {
    lang.muxsaver.choice.warm, lang.muxsaver.choice.cool, lang.muxsaver.choice.rainbow
};
static char *pulse_colour_options[] = {lang.muxsaver.choice.standard, lang.muxsaver.choice.white,
                                       lang.muxsaver.choice.red,      lang.muxsaver.choice.orange,
                                       lang.muxsaver.choice.amber,    lang.muxsaver.choice.green,
                                       lang.muxsaver.choice.cyan,     lang.muxsaver.choice.blue,
                                       lang.muxsaver.choice.purple,   lang.muxsaver.choice.pink};
static char *pulse_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *trace_colour_options[] = {lang.muxsaver.choice.standard, lang.muxsaver.choice.white,
                                       lang.muxsaver.choice.red,      lang.muxsaver.choice.orange,
                                       lang.muxsaver.choice.amber,    lang.muxsaver.choice.green,
                                       lang.muxsaver.choice.cyan,     lang.muxsaver.choice.blue,
                                       lang.muxsaver.choice.purple,   lang.muxsaver.choice.pink};
static char *trace_count_options[] = {"6", "12", "24"};
static char *trace_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *constellation_colour_options[] = {lang.muxsaver.choice.standard, lang.muxsaver.choice.white,
                                               lang.muxsaver.choice.red,      lang.muxsaver.choice.orange,
                                               lang.muxsaver.choice.amber,    lang.muxsaver.choice.green,
                                               lang.muxsaver.choice.cyan,     lang.muxsaver.choice.blue,
                                               lang.muxsaver.choice.purple,   lang.muxsaver.choice.pink};
static char *constellation_count_options[] = {"32", "64", "128"};
static char *constellation_link_options[] = {
    lang.muxsaver.choice.brief, lang.muxsaver.choice.normal, lang.muxsaver.choice.lengthy
};
static char *mystify_shapes_options[] = {lang.muxsaver.choice.automatic, "1", "2", "3", "4"};
static char *mystify_trail_options[] = {
    lang.muxsaver.choice.brief, lang.muxsaver.choice.normal, lang.muxsaver.choice.lengthy
};
static char *maze_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *maze_radar_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *blockfall_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *blockfall_colour_options[] = {lang.muxsaver.choice.standard, lang.muxsaver.choice.white,
                                           lang.muxsaver.choice.red,      lang.muxsaver.choice.orange,
                                           lang.muxsaver.choice.amber,    lang.muxsaver.choice.green,
                                           lang.muxsaver.choice.cyan,     lang.muxsaver.choice.blue,
                                           lang.muxsaver.choice.purple,   lang.muxsaver.choice.pink};
static char *datetime_date_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *datetime_seconds_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *datetime_colour_options[] = {lang.muxsaver.choice.standard, lang.muxsaver.choice.white,
                                          lang.muxsaver.choice.red,      lang.muxsaver.choice.orange,
                                          lang.muxsaver.choice.amber,    lang.muxsaver.choice.green,
                                          lang.muxsaver.choice.cyan,     lang.muxsaver.choice.blue,
                                          lang.muxsaver.choice.purple,   lang.muxsaver.choice.pink};
static char *video_shuffle_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *slideshow_time_options[] = {lang.muxsaver.choice.follow_speed, lang.muxsaver.choice.s5,
                                         lang.muxsaver.choice.s10,          lang.muxsaver.choice.s20,
                                         lang.muxsaver.choice.s30,          lang.muxsaver.choice.s60};
static char *slideshow_transition_options[] = {lang.muxsaver.choice.crossfade, lang.muxsaver.choice.cut};
static char *slideshow_order_options[] = {lang.muxsaver.choice.name, lang.muxsaver.choice.random};
static char *boxart_count_options[] = {"4", "8", "12"};
static char *boxart_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *bsod_screens_options[] = {
    lang.muxsaver.choice.crash_reboot, lang.muxsaver.choice.crash_only, lang.muxsaver.choice.reboot_only
};
static char *bsod_background_options[] = {lang.muxsaver.choice.blue, lang.muxsaver.choice.black};
static char *lavalamp_count_options[] = {lang.muxsaver.choice.random, "6", "9", "12"};
static char *lavalamp_colour_options[] = {
    lang.muxsaver.choice.random, lang.muxsaver.choice.amber, lang.muxsaver.choice.ocean, lang.muxsaver.choice.lime,
    lang.muxsaver.choice.magenta
};
static char *glitch_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *glitch_hold_options[] = {
    lang.muxsaver.choice.brief, lang.muxsaver.choice.normal, lang.muxsaver.choice.lengthy
};
static char *hallway_pixel_options[] = {
    lang.muxsaver.choice.fine, lang.muxsaver.choice.normal, lang.muxsaver.choice.chunky
};
static char *hallway_paintings_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *hallway_visitors_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *moss_size_options[] = {lang.muxsaver.choice.small, lang.generic.medium, lang.muxsaver.choice.large};
static char *moss_flowers_options[] = {lang.generic.disabled, lang.generic.enabled};
static char *moss_colour_options[] = {
    lang.muxsaver.choice.random, lang.muxsaver.choice.moss, lang.muxsaver.choice.lichen, lang.muxsaver.choice.autumn,
    lang.muxsaver.choice.frost
};

#define SAVER_OPTION(TYPE, KEY, GLYPH)                                                                                 \
    {saver_type_##TYPE,          #KEY, GLYPH, &config.settings.saver.KEY, lang.muxsaver.KEY, KEY##_options,            \
     (int) A_SIZE(KEY##_options)}

static const saver_option_def *option_defs(size_t *count) {
    static const saver_option_def defs[] = {
        SAVER_OPTION(dvd, dvd_colour, "colour"),
        SAVER_OPTION(dvd, dvd_size, "size"),
        SAVER_OPTION(star, star_count, "count"),
        SAVER_OPTION(star, star_trails, "trail"),
        SAVER_OPTION(matrix, matrix_colour, "colour"),
        SAVER_OPTION(matrix, matrix_size, "size"),
        SAVER_OPTION(firefly, firefly_count, "count"),
        SAVER_OPTION(firefly, firefly_colour, "colour"),
        SAVER_OPTION(pulse, pulse_colour, "colour"),
        SAVER_OPTION(pulse, pulse_size, "size"),
        SAVER_OPTION(trace, trace_colour, "colour"),
        SAVER_OPTION(trace, trace_count, "count"),
        SAVER_OPTION(trace, trace_size, "size"),
        SAVER_OPTION(constellation, constellation_colour, "colour"),
        SAVER_OPTION(constellation, constellation_count, "count"),
        SAVER_OPTION(constellation, constellation_link, "link"),
        SAVER_OPTION(mystify, mystify_shapes, "count"),
        SAVER_OPTION(mystify, mystify_trail, "trail"),
        SAVER_OPTION(maze, maze_size, "size"),
        SAVER_OPTION(maze, maze_radar, "radar"),
        SAVER_OPTION(blockfall, blockfall_size, "size"),
        SAVER_OPTION(blockfall, blockfall_colour, "colour"),
        SAVER_OPTION(datetime, datetime_date, "clock"),
        SAVER_OPTION(datetime, datetime_seconds, "clock"),
        SAVER_OPTION(datetime, datetime_colour, "colour"),
        SAVER_OPTION(video, video_shuffle, "shuffle"),
        SAVER_OPTION(slideshow, slideshow_time, "timer"),
        SAVER_OPTION(slideshow, slideshow_transition, "transition"),
        SAVER_OPTION(slideshow, slideshow_order, "shuffle"),
        SAVER_OPTION(boxart, boxart_count, "count"),
        SAVER_OPTION(boxart, boxart_size, "size"),
        SAVER_OPTION(bsod, bsod_screens, "screens"),
        SAVER_OPTION(bsod, bsod_background, "colour"),
        SAVER_OPTION(lavalamp, lavalamp_count, "count"),
        SAVER_OPTION(lavalamp, lavalamp_colour, "colour"),
        SAVER_OPTION(glitch, glitch_size, "size"),
        SAVER_OPTION(glitch, glitch_hold, "timer"),
        SAVER_OPTION(hallway, hallway_pixel, "size"),
        SAVER_OPTION(hallway, hallway_paintings, "paintings"),
        SAVER_OPTION(hallway, hallway_visitors, "visitors"),
        SAVER_OPTION(moss, moss_size, "size"),
        SAVER_OPTION(moss, moss_flowers, "flowers"),
        SAVER_OPTION(moss, moss_colour, "colour"),
    };

    *count = A_SIZE(defs);
    return defs;
}

static const char *saver_title(const int type) {
    const char *names[saver_type_count] = {
        lang.generic.disabled,
        lang.muxpower.saver.type.dvd,
        lang.muxpower.saver.type.star,
        lang.muxpower.saver.type.matrix,
        lang.muxpower.saver.type.firefly,
        lang.muxpower.saver.type.pulse,
        lang.muxpower.saver.type.trace,
        lang.muxpower.saver.type.constellation,
        lang.muxpower.saver.type.mystify,
        lang.muxpower.saver.type.maze,
        lang.muxpower.saver.type.blockfall,
        lang.muxpower.saver.type.datetime,
        lang.muxpower.saver.type.video,
        lang.muxpower.saver.type.slideshow,
        lang.muxpower.saver.type.boxart,
        lang.muxpower.saver.type.bsod,
        lang.muxpower.saver.type.lavalamp,
        lang.muxpower.saver.type.glitch,
        lang.muxpower.saver.type.hallway,
        lang.muxpower.saver.type.moss,
        lang.muxpower.saver.type.blank,
    };
    return type >= 0 && type < saver_type_count ? names[type] : lang.muxsaver.title;
}

static const char *option_help(const char *key) {
    const struct {
        const char *key;
        const char *text;
    } help[] = {
#define HELP(KEY) {#KEY, lang.muxsaver.help.KEY}
        HELP(dvd_colour),          HELP(dvd_size),
        HELP(star_count),          HELP(star_trails),
        HELP(matrix_colour),       HELP(matrix_size),
        HELP(firefly_count),       HELP(firefly_colour),
        HELP(pulse_colour),        HELP(pulse_size),
        HELP(trace_colour),        HELP(trace_count),
        HELP(trace_size),          HELP(constellation_colour),
        HELP(constellation_count), HELP(constellation_link),
        HELP(mystify_shapes),      HELP(mystify_trail),
        HELP(maze_size),           HELP(maze_radar),
        HELP(blockfall_size),      HELP(blockfall_colour),
        HELP(datetime_date),       HELP(datetime_seconds),
        HELP(datetime_colour),     HELP(video_shuffle),
        HELP(slideshow_time),      HELP(slideshow_transition),
        HELP(slideshow_order),     HELP(boxart_count),
        HELP(boxart_size),         HELP(bsod_screens),
        HELP(bsod_background),     HELP(lavalamp_count),
        HELP(lavalamp_colour),     HELP(glitch_size),
        HELP(glitch_hold),         HELP(hallway_pixel),
        HELP(hallway_paintings),   HELP(hallway_visitors),
        HELP(moss_size),           HELP(moss_flowers),
        HELP(moss_colour),
#undef HELP
    };

    for (size_t i = 0; i < A_SIZE(help); i++)
        if (strcmp(help[i].key, key) == 0) return help[i].text;
    return lang.muxsaver.overview;
}

static int any_option_modified(void) {
    for (int i = 0; i < row_count; i++)
        if ((int) lv_dropdown_get_selected(rows[i].value) != rows[i].original) return 1;
    return 0;
}

static void apply_selected_to_config(void) {
    for (int i = 0; i < row_count; i++)
        *rows[i].def->value = (int16_t) lv_dropdown_get_selected(rows[i].value);
}

static void restore_original_config(void) {
    for (int i = 0; i < row_count; i++)
        *rows[i].def->value = (int16_t) rows[i].original;
}

static int save_options(void) {
    int save_failed = 0;
    int modified = 0;

    for (int i = 0; i < row_count; i++) {
        const int selected = (int) lv_dropdown_get_selected(rows[i].value);
        if (selected == rows[i].original) continue;

        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), CONF_CONFIG_PATH "settings/saver/%s", rows[i].def->key);
        if (!write_text_to_file_atomic(path, INT, selected)) save_failed++;
        *rows[i].def->value = (int16_t) selected;
        rows[i].original = selected;
        modified++;
    }

    if (save_failed) {
        play_sound(snd_error);
        toast_message(lang.generic.save_fail, tst_wait_m);
        return 0;
    }

    if (modified) display_saver_reload();
    return 1;
}

static void close_screen(void) {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "saver");
    load_mux("power");
    mux_input_stop();
}

static void show_help(void) {
    const lv_obj_t *focused = lv_group_get_focused(ui_group);
    for (int i = 0; i < row_count; i++) {
        if (rows[i].label != focused) continue;
        show_info_box(rows[i].def->label, option_help(rows[i].def->key), 0);
        return;
    }
    show_info_box(lang.muxsaver.title, lang.muxsaver.overview, 0);
}

static void create_rows(void) {
    static lv_obj_t *ui_objects[SAVER_OPTION_MAX];
    static lv_obj_t *ui_objects_value[SAVER_OPTION_MAX];
    static lv_obj_t *ui_objects_glyph[SAVER_OPTION_MAX];
    static lv_obj_t *ui_objects_panel[SAVER_OPTION_MAX];

    row_count = 0;
    ui_count_static = 0;

    size_t def_count = 0;
    const saver_option_def *defs = option_defs(&def_count);
    for (size_t i = 0; i < def_count && row_count < SAVER_OPTION_MAX; i++) {
        if (defs[i].saver != active_type) continue;

        saver_option_row *row = &rows[row_count];
        row->def = &defs[i];
        row->panel = lv_obj_create(ui_pnl_content);
        row->label = lv_label_create(row->panel);
        lv_label_set_text(row->label, "");
        row->glyph = lv_img_create(row->panel);
        row->value = lv_dropdown_create(row->panel);
        lv_dropdown_clear_options(row->value);
        lv_obj_set_style_text_opa(row->value, LV_OPA_TRANSP, MU_OBJ_INDI_DEFAULT);

        apply_theme_list_panel(row->panel);
        apply_theme_option_item_label(&theme, row->label, row->def->label, 1);
        apply_theme_list_glyph(&theme, row->glyph, mux_module, row->def->glyph);
        apply_theme_list_drop_down(&theme, row->label, row->value, NULL);
        add_drop_down_options(row->value, row->def->choices, row->def->count);

        int current = *row->def->value;
        if (current < 0 || current >= row->def->count) current = 0;
        lv_dropdown_set_selected(row->value, current);
        row->original = current;

        lv_obj_set_user_data(row->label, (void *) row->def->key);

        ui_objects[row_count] = row->label;
        ui_objects_value[row_count] = row->value;
        ui_objects_glyph[row_count] = row->glyph;
        ui_objects_panel[row_count] = row->panel;

        row_count++;
        ui_count_static++;
    }

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
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

static void handle_option_prev(void) {
    if (msgbox_active || !row_count) return;
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active || !row_count) return;
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (dialogue_active(&save_dlg)) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        dialogue_dismiss(&save_dlg);

        if (opt == mux_unsaved_save) {
            if (!save_options()) return;
        } else {
            restore_original_config();
        }

        close_screen();
        return;
    }

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&save_dlg)) {
        dialogue_mark_cancelled(&save_dlg);
        dialogue_dismiss(&save_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (dialogue_guard_unsaved(&save_dlg, &theme, any_option_modified())) return;

    play_sound(snd_back);
    if (save_options()) close_screen();
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;
    if (msgbox_active || hold_call || dialogue_active(&save_dlg)) return;

    play_sound(snd_confirm);
    apply_selected_to_config();
    preview_saver(active_type, config.settings.power.saver_speed > 0 ? config.settings.power.saver_speed : 90);
    refresh_screen(ui_screen, 1);
}

static void handle_dpad_up(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || dialogue_active(&save_dlg)) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.preview, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxsaver_main(void) {
    init_module(__func__);

    active_type = read_line_int_from(SAVER_OPTIONS_TYPE, 1);
    if (active_type <= saver_type_disabled || active_type >= saver_type_blank)
        active_type = config.settings.power.saver_type;

    init_theme(1, 0);
    init_ui_common_screen(&theme, &device, &lang, saver_title(active_type));
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_rows();

    if (!row_count) lv_label_set_text(ui_lbl_screen_message, lang.muxsaver.overview);

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

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
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxsaver.title, lang.muxsaver.overview);

    mux_input_task(&input_opts);

    restore_original_config();
    remove(SAVER_OPTIONS_TYPE);

    return 0;
}
