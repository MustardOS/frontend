#include "muxshare.h"
#include "ui/ui_muxchrony.h"

#define UI_COUNT 11
#define CHRONY_BUFFER 2048

static char chrony_raw[CHRONY_BUFFER];
static time_t chrony_last = 0;

static void show_help() {
    struct help_msg help_messages[] = {
#define CHRONY(NAME, ENUM, UDATA) { ui_lbl##NAME##_chrony, lang.MUXCHRONY.HELP.ENUM },
            CHRONY_ELEMENTS
#undef CHRONY
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static void chrony_normalise(char *output) {
    char *mod;

    while ((mod = strstr(output, " seconds")) || (mod = strstr(output, " second"))) {
        memmove(mod, mod + (*mod == ' ' ? 8 : 7), strlen(mod + 8) + 1);
    }

    if ((mod = strstr(output, " fast of NTP time"))) {
        *mod = '\0';
        strcat(output, " fast");
        return;
    }

    if ((mod = strstr(output, " slow of NTP time"))) {
        *mod = '\0';
        strcat(output, " slow");
        return;
    }
}

static void chrony_refresh(void) {
    time_t now = time(NULL);
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
    if (chrony_raw[0] == '\0') return lang.GENERIC.UNKNOWN;

    char *line = strstr(chrony_raw, key);
    if (!line) return lang.GENERIC.UNKNOWN;

    char *colon = strchr(line, ':');
    if (!colon) return lang.GENERIC.UNKNOWN;

    colon++;
    while (*colon == ' ') colon++;

    size_t len = strcspn(colon, "\n");
    snprintf(value, sizeof(value), "%.*s", (int) len, colon);
    chrony_normalise(value);

    return value;
}

static void update_chrony_info(void) {
    lv_label_set_text(ui_lblReferenceValue_chrony, chrony_field("Reference ID"));
    lv_label_set_text(ui_lblStratumValue_chrony, chrony_field("Stratum"));
    lv_label_set_text(ui_lblRefTimeValue_chrony, chrony_field("Ref time"));
    lv_label_set_text(ui_lblSystemTimeValue_chrony, chrony_field("System time"));
    lv_label_set_text(ui_lblLastOffsetValue_chrony, chrony_field("Last offset"));
    lv_label_set_text(ui_lblRmsOffsetValue_chrony, chrony_field("RMS offset"));
    lv_label_set_text(ui_lblFrequencyValue_chrony, chrony_field("Frequency"));
    lv_label_set_text(ui_lblRootDelayValue_chrony, chrony_field("Root delay"));
    lv_label_set_text(ui_lblRootDispValue_chrony, chrony_field("Root dispersion"));
    lv_label_set_text(ui_lblUpdateIntValue_chrony, chrony_field("Update interval"));
    lv_label_set_text(ui_lblLeapValue_chrony, chrony_field("Leap status"));
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, chrony, Reference, lang.MUXCHRONY.REFERENCE, "reference", chrony_field("Reference ID"));
    INIT_VALUE_ITEM(-1, chrony, Stratum, lang.MUXCHRONY.STRATUM, "stratum", chrony_field("Stratum"));
    INIT_VALUE_ITEM(-1, chrony, RefTime, lang.MUXCHRONY.REFTIME, "reftime", chrony_field("Ref time"));
    INIT_VALUE_ITEM(-1, chrony, SystemTime, lang.MUXCHRONY.SYSTEMTIME, "system", chrony_field("System time"));
    INIT_VALUE_ITEM(-1, chrony, LastOffset, lang.MUXCHRONY.LASTOFFSET, "last", chrony_field("Last offset"));
    INIT_VALUE_ITEM(-1, chrony, RmsOffset, lang.MUXCHRONY.RMSOFFSET, "rms", chrony_field("RMS offset"));
    INIT_VALUE_ITEM(-1, chrony, Frequency, lang.MUXCHRONY.FREQUENCY, "freq", chrony_field("Frequency"));
    INIT_VALUE_ITEM(-1, chrony, RootDelay, lang.MUXCHRONY.ROOTDELAY, "delay", chrony_field("Root delay"));
    INIT_VALUE_ITEM(-1, chrony, RootDisp, lang.MUXCHRONY.ROOTDISP, "disp", chrony_field("Root dispersion"));
    INIT_VALUE_ITEM(-1, chrony, UpdateInt, lang.MUXCHRONY.UPDATEINT, "update", chrony_field("Update interval"));
    INIT_VALUE_ITEM(-1, chrony, Leap, lang.MUXCHRONY.LEAP, "leap", chrony_field("Leap status"));

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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "chrony");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);

    chrony_refresh();
    update_chrony_info();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavBGlyph, "",                   0},
            {ui_lblNavB,      lang.GENERIC.BACK,    0},
            {ui_lblNavXGlyph, "",                   0},
            {ui_lblNavX,      lang.GENERIC.REFRESH, 0},
            {NULL, NULL,                            0}
    });

#define CHRONY(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_chrony, UDATA);
    CHRONY_ELEMENTS
#undef CHRONY

    overlay_display();
}

int muxchrony_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCHRONY.TITLE);
    init_muxchrony(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    chrony_refresh();

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    return 0;
}
