#include "muxshare.h"
#include "ui/ui_muxtext.h"

#define TEXT_FILE   "/tmp/mux_text_read"
#define TEXT_MAX_SZ (2 * 1024 * 1024)

static void scroll_textarea(const int step) {
    const lv_coord_t y = lv_obj_get_scroll_y(ui_txt_document_text);
    lv_coord_t max =
        lv_obj_get_height(lv_textarea_get_label(ui_txt_document_text)) - lv_obj_get_height(ui_txt_document_text);

    if (max < 0) max = 0;

    // Clamp to top if we overshoot
    if (step > 0 && y - step < 0) {
        lv_obj_scroll_to_y(ui_txt_document_text, 0, LV_ANIM_ON);
        return;
    }

    // Clamp to bottom... hehe
    if (step < 0 && y - step > max) {
        lv_obj_scroll_to_y(ui_txt_document_text, max, LV_ANIM_ON);
        return;
    }

    // Otherwise scroll normally
    if ((step > 0 && y > step - 5) || (step < 0 && y < max)) {
        lv_obj_scroll_by(ui_txt_document_text, 0, step, LV_ANIM_ON);
    }
}

static void handle_up(void) {
    scroll_textarea(+config.settings.advanced.accelerate);
}

static void handle_down(void) {
    scroll_textarea(-config.settings.advanced.accelerate);
}

static void handle_up_page(void) {
    scroll_textarea(+config.settings.advanced.accelerate * 3);
}

static void handle_down_page(void) {
    scroll_textarea(-config.settings.advanced.accelerate * 3);
}

static void handle_x(void) {
    if (hold_call) return;

    lv_obj_scroll_to_y(ui_txt_document_text, 0, LV_ANIM_ON);
}

static void handle_b(void) {
    if (hold_call) return;

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "text");

    mux_input_stop();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.top, 0},
                                  {NULL, NULL, 0}});

    lv_obj_set_user_data(ui_txt_document_text, "document");

    overlay_display();
}

int muxtext_main(void) {
    if (!file_exist(TEXT_FILE)) return 1;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, read_line_char_from(TEXT_FILE, 1));
    init_muxtext(ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_timer(ui_gen_refresh_task, NULL);

    char *raw_text_path = read_line_char_from(TEXT_FILE, 2);
    char text_path[PATH_MAX];
    const int path_ok = *raw_text_path && realpath(raw_text_path, text_path);

    if (*raw_text_path) free(raw_text_path);
    if (!path_ok) return 1;

    FILE *tf = fopen(text_path, "r");
    if (!tf) return 1;

    struct stat tst;
    size_t read_size = TEXT_MAX_SZ;
    int truncated = 0;

    if (fstat(fileno(tf), &tst) == 0) {
        if ((size_t) tst.st_size > TEXT_MAX_SZ) {
            truncated = 1;
        } else {
            read_size = (size_t) tst.st_size;
        }
    }

    char *text = malloc(read_size + 1);
    if (!text) {
        fclose(tf);
        return 1;
    }

    const size_t bytes_read = fread(text, 1, read_size, tf);
    text[bytes_read] = '\0';
    fclose(tf);

    if (truncated) {
        LOG_WARN(
            mux_module, "File exceeds %d MB; showing first %d MB: %s", TEXT_MAX_SZ / (1024 * 1024),
            TEXT_MAX_SZ / (1024 * 1024), text_path
        );
        toast_message(lang.generic.warning, tst_wait_s);
    }

    lv_textarea_set_text(ui_txt_document_text, text);
    free(text);
    handle_x();

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_l1] = handle_up_page,
                [mux_input_r1] = handle_down_page,
            },
        .release_handler = {},
        .hold_handler = {
            [mux_input_dpad_up] = handle_up,
            [mux_input_dpad_down] = handle_down,
            [mux_input_l1] = handle_up_page,
            [mux_input_r1] = handle_down_page,
        },

    };

    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
