#include "muxshare.h"

#define STICK_AXE_MAX 32767
#define STICK_RAD_MIN 80
#define STICK_RAD_MAX 224
#define STICK_LBL_MIN 76
#define STICK_LBL_MAX 132
#define STICK_SQR_RAD 5
#define STICK_CVS_MAX (STICK_RAD_MAX * 2)

static lv_coord_t stick_radius_px;
static lv_coord_t stick_deadzone_px;
static lv_coord_t stick_dot_px;
static lv_coord_t stick_edge_gap_px;
static lv_coord_t stick_target_px;
static lv_coord_t stick_label_gap_px;

lv_obj_t *ui_imgButton;

static lv_obj_t *ui_pnlStickL = NULL, *ui_pnlStickR = NULL;
static lv_obj_t *ui_cvsStickL = NULL, *ui_cvsStickR = NULL;
static lv_obj_t *ui_lblStickL = NULL, *ui_lblStickR = NULL;

static LV_ATTRIBUTE_MEM_ALIGN uint8_t stick_buf_l[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(STICK_CVS_MAX, STICK_CVS_MAX)];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t stick_buf_r[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(STICK_CVS_MAX, STICK_CVS_MAX)];

static int16_t stick_l_x = 0;
static int16_t stick_l_y = 0;
static int16_t stick_r_x = 0;
static int16_t stick_r_y = 0;

static int menu_icon_active = 0;

static const char *glyph[MUX_INPUT_COUNT] = {
        // Gamepad buttons:
        [MUX_INPUT_A] = "btn_a",
        [MUX_INPUT_B] = "btn_b",
        [MUX_INPUT_C] = "btn_c",
        [MUX_INPUT_X] = "btn_x",
        [MUX_INPUT_Y] = "btn_y",
        [MUX_INPUT_Z] = "btn_z",
        [MUX_INPUT_L1] = "btn_l1",
        [MUX_INPUT_L2] = "btn_l2",
        [MUX_INPUT_L3] = "btn_l3",
        [MUX_INPUT_R1] = "btn_r1",
        [MUX_INPUT_R2] = "btn_r2",
        [MUX_INPUT_R3] = "btn_r3",
        [MUX_INPUT_SELECT] = "btn_select",
        [MUX_INPUT_START] = "btn_start",
        [MUX_INPUT_SWITCH] = "btn_switch",
        [MUX_INPUT_MENU] = "btn_menu",

        // D-pad:
        [MUX_INPUT_DPAD_UP] = "dpad_up",
        [MUX_INPUT_DPAD_DOWN] = "dpad_down",
        [MUX_INPUT_DPAD_LEFT] = "dpad_left",
        [MUX_INPUT_DPAD_RIGHT] = "dpad_right",

        [MUX_INPUT_LS_UP] = NULL,
        [MUX_INPUT_LS_DOWN] = NULL,
        [MUX_INPUT_LS_LEFT] = NULL,
        [MUX_INPUT_LS_RIGHT] = NULL,
        [MUX_INPUT_RS_UP] = NULL,
        [MUX_INPUT_RS_DOWN] = NULL,
        [MUX_INPUT_RS_LEFT] = NULL,
        [MUX_INPUT_RS_RIGHT] = NULL,

        // Volume buttons:
        [MUX_INPUT_VOL_UP] = "vol_up",
        [MUX_INPUT_VOL_DOWN] = "vol_down",
};

static void show_icon(const char *name) {
    char path[MAX_BUFFER_SIZE];
    char embed[MAX_BUFFER_SIZE];

    generate_image_embed(mux_dim, mux_module, name, path, sizeof(path), embed, sizeof(embed));

    if (file_exist(path)) {
        lv_img_set_src(ui_imgButton, embed);
    } else {
        lv_img_set_src(ui_imgButton, &ui_image_Nothing);
    }
}

static void clear_icon(void) {
    lv_img_set_src(ui_imgButton, &ui_image_Nothing);
}

static void handle_input(mux_input_type type, mux_input_action action) {
    if (action == MUX_INPUT_PRESS) {
        if (!glyph[type]) return;
        show_icon(glyph[type]);
    } else if (action == MUX_INPUT_RELEASE) {
        if (glyph[type]) clear_icon();
    }
}

static inline lv_coord_t axis_to_px(int16_t value) {
    int32_t px = ((int32_t) value * stick_radius_px) / STICK_AXE_MAX;

    if (px > stick_radius_px) px = stick_radius_px;
    if (px < -stick_radius_px) px = -stick_radius_px;

    return (lv_coord_t) px;
}

static void draw_stick_canvas(lv_obj_t *canvas, int16_t x, int16_t y) {
    if (!canvas) return;

    int target = (int) stick_target_px;
    int radius = (int) stick_radius_px;
    int deadzone = (int) stick_deadzone_px;
    int dot = (int) stick_dot_px;
    int centre = target / 2;

    if (target <= 0 || radius <= 0 || deadzone <= 0 || dot <= 0) return;

    lv_color_t ring_col = lv_color_hex(theme.LIST_DEFAULT.GLYPH_RECOLOUR);
    lv_color_t dot_col = lv_color_hex(theme.LIST_FOCUS.GLYPH_RECOLOUR);

    lv_canvas_fill_bg(canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);

    lv_draw_rect_dsc_t rect_dsc;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_width = 2;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = LV_OPA_30;
    rect_dsc.radius = STICK_SQR_RAD;
    lv_canvas_draw_rect(canvas, 1, 1, (lv_coord_t) (target - 2), (lv_coord_t) (target - 2), &rect_dsc);

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_width = 2;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = LV_OPA_COVER;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, 1, 1, (lv_coord_t) (target - 2), (lv_coord_t) (target - 2), &rect_dsc);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = ring_col;
    line_dsc.opa = LV_OPA_20;
    line_dsc.width = 1;

    lv_point_t h_line[] = {
            {0,                         (lv_coord_t) centre},
            {(lv_coord_t) (target - 1), (lv_coord_t) centre},
    };
    lv_canvas_draw_line(canvas, h_line, 2, &line_dsc);

    lv_point_t v_line[] = {
            {(lv_coord_t) centre, 0},
            {(lv_coord_t) centre, (lv_coord_t) (target - 1)},
    };
    lv_canvas_draw_line(canvas, v_line, 2, &line_dsc);

    int inner = deadzone * 2;
    int inner_pos = centre - deadzone;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = ring_col;
    rect_dsc.bg_opa = LV_OPA_10;
    rect_dsc.border_width = 1;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = LV_OPA_60;
    rect_dsc.radius = STICK_SQR_RAD;
    lv_canvas_draw_rect(canvas, (lv_coord_t) inner_pos, (lv_coord_t) inner_pos, (lv_coord_t) inner, (lv_coord_t) inner, &rect_dsc);

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = ring_col;
    rect_dsc.bg_opa = LV_OPA_10;
    rect_dsc.border_width = 1;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = LV_OPA_70;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, (lv_coord_t) inner_pos, (lv_coord_t) inner_pos, (lv_coord_t) inner, (lv_coord_t) inner, &rect_dsc);

    int centre_mark = deadzone / 3;
    if (centre_mark < 8) centre_mark = 8;

    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = ring_col;
    line_dsc.opa = LV_OPA_40;
    line_dsc.width = 1;

    lv_point_t dz_h_line[] = {
            {(lv_coord_t) (centre - centre_mark), (lv_coord_t) centre},
            {(lv_coord_t) (centre + centre_mark), (lv_coord_t) centre},
    };
    lv_canvas_draw_line(canvas, dz_h_line, 2, &line_dsc);

    lv_point_t dz_v_line[] = {
            {(lv_coord_t) centre, (lv_coord_t) (centre - centre_mark)},
            {(lv_coord_t) centre, (lv_coord_t) (centre + centre_mark)},
    };
    lv_canvas_draw_line(canvas, dz_v_line, 2, &line_dsc);

    int dot_x = centre + axis_to_px(x) - dot / 2;
    int dot_y = centre + axis_to_px(y) - dot / 2;

    if (dot_x < 0) dot_x = 0;
    if (dot_y < 0) dot_y = 0;
    if (dot_x > target - dot) dot_x = target - dot;
    if (dot_y > target - dot) dot_y = target - dot;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = dot_col;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_width = 0;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, (lv_coord_t) dot_x, (lv_coord_t) dot_y, (lv_coord_t) dot, (lv_coord_t) dot, &rect_dsc);

    lv_obj_invalidate(canvas);
}

static void set_stick_value_label(lv_obj_t *label, int16_t x, int16_t y) {
    if (!label) return;

    char value[32];
    snprintf(value, sizeof(value), "X:%6d\nY:%6d", x, y);

    lv_label_set_text(label, value);
}

static void update_stick_canvas(lv_obj_t *canvas, lv_obj_t *label, int16_t x, int16_t y, int16_t *last_x, int16_t *last_y) {
    if (!canvas) return;
    if (*last_x == x && *last_y == y) return;

    *last_x = x;
    *last_y = y;

    draw_stick_canvas(canvas, x, y);
    set_stick_value_label(label, x, y);
}

static void handle_analog(int16_t ls_x, int16_t ls_y, int16_t rs_x, int16_t rs_y) {
    update_stick_canvas(ui_cvsStickL, ui_lblStickL, ls_x, ls_y, &stick_l_x, &stick_l_y);
    update_stick_canvas(ui_cvsStickR, ui_lblStickR, rs_x, rs_y, &stick_r_x, &stick_r_y);
}

static void handle_idle(void) {
    ui_common_handle_idle();

    if (board_is(BOARD_SPECIAL_G350) && g350_menu_pressed) {
        if (!menu_icon_active) {
            show_icon("btn_menu");
            menu_icon_active = 1;
        }
    } else if (menu_icon_active) {
        clear_icon();
        menu_icon_active = 0;
    }
}

static void handle_quit(void) {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "tester");

    mux_input_stop();
}

static lv_obj_t *create_stick_target(lv_obj_t *parent, lv_align_t align, lv_coord_t x_offset, lv_obj_t **out_canvas, lv_obj_t **out_label, uint8_t *buf) {
    lv_coord_t panel_h = (lv_coord_t) (stick_target_px + stick_label_gap_px);

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, stick_target_px, panel_h);
    lv_obj_align(panel, align, x_offset, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(canvas, buf, stick_target_px, stick_target_px, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(panel);
    lv_obj_set_width(label, stick_target_px);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(theme.LIST_DEFAULT.TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(label, theme.LIST_DEFAULT.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
    set_stick_value_label(label, 0, 0);

    *out_canvas = canvas;
    *out_label = label;

    draw_stick_canvas(canvas, 0, 0);

    return panel;
}

static void init_elements(void) {
    header_and_footer_setup();

    int w = device.MUX.WIDTH;
    int h = device.MUX.HEIGHT;
    int shorter = (w < h) ? w : h;

    int radius = shorter / 5;
    if (radius < STICK_RAD_MIN) radius = STICK_RAD_MIN;
    if (radius > STICK_RAD_MAX) radius = STICK_RAD_MAX;
    stick_radius_px = (lv_coord_t) radius;
    stick_target_px = (lv_coord_t) (radius * 2);

    int deadzone = radius / 4;
    if (deadzone < 18) deadzone = 18;
    stick_deadzone_px = (lv_coord_t) deadzone;

    int dot = radius / 5;
    if (dot < 14) dot = 14;
    stick_dot_px = (lv_coord_t) dot;

    int label_gap = radius / 2;
    if (label_gap < STICK_LBL_MIN) label_gap = STICK_LBL_MIN;
    if (label_gap > STICK_LBL_MAX) label_gap = STICK_LBL_MAX;
    stick_label_gap_px = (lv_coord_t) label_gap;

    int edge_gap = w / 40;
    if (edge_gap < 18) edge_gap = 18;
    if (edge_gap > 64) edge_gap = 64;
    stick_edge_gap_px = (lv_coord_t) edge_gap;

    ui_imgButton = lv_img_create(ui_screen);
    lv_img_set_src(ui_imgButton, &ui_image_Nothing);
    lv_obj_set_style_img_recolor(ui_imgButton, lv_color_hex(theme.LIST_DEFAULT.GLYPH_RECOLOUR), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_imgButton, theme.LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA, MU_OBJ_MAIN_DEFAULT);

    if (device.BOARD.HASSTICK) {
        lv_obj_align(ui_imgButton, LV_ALIGN_CENTER, 0, -32);

        stick_l_x = 0;
        stick_l_y = 0;
        stick_r_x = 0;
        stick_r_y = 0;

        ui_pnlStickL = create_stick_target(ui_screen, LV_ALIGN_LEFT_MID, stick_edge_gap_px, &ui_cvsStickL, &ui_lblStickL, stick_buf_l);
        ui_pnlStickR = create_stick_target(ui_screen, LV_ALIGN_RIGHT_MID, (lv_coord_t) -stick_edge_gap_px, &ui_cvsStickR, &ui_lblStickR, stick_buf_r);

        lv_obj_move_foreground(ui_pnlStickL);
        lv_obj_move_foreground(ui_pnlStickR);
        lv_obj_move_foreground(ui_imgButton);
    } else {
        lv_obj_align(ui_imgButton, LV_ALIGN_CENTER, 0, -32);
    }

    lv_label_set_text(ui_lblMessage, config.SETTINGS.ADVANCED.SWAP ? lang.MUXTESTER.QUIT_ALT : lang.MUXTESTER.QUIT);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(ui_pnlMessage, -12);

    lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);

    overlay_display();
}

int muxtester_main(void) {
    init_module(__func__);
    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTESTER.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_elements();

    init_timer(NULL, NULL);

    mux_input_options input_opts = {
            .input_handler = handle_input,
            .idle_handler = handle_idle,
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_DPAD_DOWN) |
                                         BIT(config.SETTINGS.ADVANCED.SWAP ? MUX_INPUT_A : MUX_INPUT_B),
                            .press_handler = handle_quit,
                            .hold_handler = handle_quit,
                    }
            },
            .combo_count = 1
    };

    init_input(&input_opts, false);

    input_opts.remap_to_dpad = false;
    input_opts.analog_handler = device.BOARD.HASSTICK ? handle_analog : NULL;

    mux_input_task(&input_opts);

    return 0;
}
