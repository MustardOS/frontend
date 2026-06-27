#include "muxshare.h"
#include "ui/ui_muxchrony.h"

#define CHRONY(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(CHRONY_ELEMENTS) };
#undef CHRONY

#define CHRONY_BUFFER 2048

static char chrony_raw[CHRONY_BUFFER];
static time_t chrony_last = 0;

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define CHRONY(NAME, UDATA) {UDATA, lang.muxchrony.help.NAME},
        CHRONY_ELEMENTS
#undef CHRONY
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void chrony_normalise(const char *output) {
    char *mod;
    size_t skip;

    for (;;) {
        mod = strstr(output, " seconds");
        if (mod) {
            skip = 8;
        } else {
            mod = strstr(output, " second");
            if (!mod) break;
            skip = 7;
        }
        memmove(mod, mod + skip, strlen(mod + skip) + 1);
    }

    mod = strstr(output, " fast of NTP time");
    if (mod) {
        memcpy(mod, " fast", sizeof(" fast"));
        return;
    }

    mod = strstr(output, " slow of NTP time");
    if (mod) memcpy(mod, " slow", sizeof(" slow"));
}

static void chrony_refresh(void) {
    const time_t now = time(NULL);
    if (now == chrony_last) return;

    char *out = get_execute_result("/opt/muos/bin/chronyc tracking", -1);
    if (!out) {
        chrony_raw[0] = '\0';
    } else {
        snprintf(chrony_raw, sizeof(chrony_raw), "%s", out);
        free(out);
    }

    chrony_last = now;
}

static const char *chrony_field(const char *key) {
    static char value[MAX_BUFFER_SIZE];
    if (chrony_raw[0] == '\0') return lang.generic.unknown;

    char *line = strstr(chrony_raw, key);
    if (!line) return lang.generic.unknown;

    char *colon = strchr(line, ':');
    if (!colon) return lang.generic.unknown;

    colon++;
    while (*colon == ' ')
        colon++;

    const size_t len = strcspn(colon, "\n");
    snprintf(value, sizeof(value), "%.*s", (int) len, colon);
    chrony_normalise(value);

    return value;
}

static void update_chrony_info(void) {
    lv_label_set_text(ui_val_reference_chrony, chrony_field("Reference ID"));
    lv_label_set_text(ui_val_stratum_chrony, chrony_field("Stratum"));
    lv_label_set_text(ui_val_ref_time_chrony, chrony_field("Ref time"));
    lv_label_set_text(ui_val_system_time_chrony, chrony_field("System time"));
    lv_label_set_text(ui_val_last_offset_chrony, chrony_field("Last offset"));
    lv_label_set_text(ui_val_rms_offset_chrony, chrony_field("RMS offset"));
    lv_label_set_text(ui_val_frequency_chrony, chrony_field("Frequency"));
    lv_label_set_text(ui_val_root_delay_chrony, chrony_field("Root delay"));
    lv_label_set_text(ui_val_root_disp_chrony, chrony_field("Root dispersion"));
    lv_label_set_text(ui_val_update_int_chrony, chrony_field("Update interval"));
    lv_label_set_text(ui_val_leap_chrony, chrony_field("Leap status"));
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, chrony, reference, lang.muxchrony.reference, "reference", chrony_field("Reference ID"));
    INIT_VALUE_ITEM(-1, chrony, stratum, lang.muxchrony.stratum, "stratum", chrony_field("Stratum"));
    INIT_VALUE_ITEM(-1, chrony, ref_time, lang.muxchrony.ref_time, "reftime", chrony_field("Ref time"));
    INIT_VALUE_ITEM(-1, chrony, system_time, lang.muxchrony.system_time, "system", chrony_field("System time"));
    INIT_VALUE_ITEM(-1, chrony, last_offset, lang.muxchrony.last_offset, "last", chrony_field("Last offset"));
    INIT_VALUE_ITEM(-1, chrony, rms_offset, lang.muxchrony.rms_offset, "rms", chrony_field("RMS offset"));
    INIT_VALUE_ITEM(-1, chrony, frequency, lang.muxchrony.frequency, "freq", chrony_field("Frequency"));
    INIT_VALUE_ITEM(-1, chrony, root_delay, lang.muxchrony.root_delay, "delay", chrony_field("Root delay"));
    INIT_VALUE_ITEM(-1, chrony, root_disp, lang.muxchrony.root_disp, "disp", chrony_field("Root dispersion"));
    INIT_VALUE_ITEM(-1, chrony, update_int, lang.muxchrony.update_int, "update", chrony_field("Update interval"));
    INIT_VALUE_ITEM(-1, chrony, leap, lang.muxchrony.leap, "leap", chrony_field("Leap status"));

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "chrony");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);

    chrony_refresh();
    update_chrony_info();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.refresh, 0},
                                  {NULL, NULL, 0}});

#define CHRONY(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_chrony, UDATA);
    CHRONY_ELEMENTS
#undef CHRONY

    overlay_display();
}

int muxchrony_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxchrony.title);
    init_muxchrony(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    chrony_refresh();

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
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
        },
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
