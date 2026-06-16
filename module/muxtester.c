#include "muxshare.h"

#define STICK_AXE_MAX 32767
#define STICK_RAD_MIN 56
#define STICK_RAD_MAX 192
#define STICK_LBL_MIN 64
#define STICK_LBL_MAX 256
#define STICK_TRL_NUM 8

#define STICK_REDRAW_DELTA 128

#define STICK_OPA_OUT_SQR 20
#define STICK_OPA_OUT_CIR 90
#define STICK_OPA_AXIS    25
#define STICK_OPA_DZ_FILL 10
#define STICK_OPA_DZ_SQR  55
#define STICK_OPA_DZ_CIR  70
#define STICK_OPA_CENTRE  45

#define STICK_TRAIL_OPA_BASE  24
#define STICK_TRAIL_OPA_RANGE 96

#define STICK_PEAK_MIN_PCT 5
#define STICK_PEAK_MARKER  6

typedef struct {
    int16_t x;
    int16_t y;
} stick_sample_t;

typedef struct {
    lv_obj_t *static_canvas;
    lv_obj_t *dynamic_canvas;
    lv_obj_t *label;
    uint8_t *static_buf;
    uint8_t *dynamic_buf;

    int16_t x;
    int16_t y;
    int peak_x_pct;
    int peak_y_pct;

    stick_sample_t trail[STICK_TRL_NUM];
    int trail_count;
    int trail_index;
} stick_state_t;

static lv_coord_t stick_radius_px;
static lv_coord_t stick_deadzone_px;
static lv_coord_t stick_dot_px;
static lv_coord_t stick_edge_gap_px;
static lv_coord_t stick_target_px;
static lv_coord_t stick_label_gap_px;
static lv_coord_t stick_panel_y_offset_px;
static lv_coord_t stick_square_radius_px;

lv_obj_t *ui_imgButton;

static lv_obj_t *ui_pnlInputPreview = NULL;
static lv_obj_t *ui_pnlStickL = NULL;
static lv_obj_t *ui_pnlStickR = NULL;

static stick_state_t stick_l = {0};
static stick_state_t stick_r = {0};

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

static int clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;

    return value;
}

static int axis_abs(int16_t value) {
    int v = (int) value;

    return v < 0 ? -v : v;
}

static int axis_pct_signed(int16_t value) {
    int pct = ((int) value * 100) / STICK_AXE_MAX;

    return clamp_int(pct, -100, 100);
}

static int axis_pct_abs(int16_t value) {
    int pct = (axis_abs(value) * 100) / STICK_AXE_MAX;

    return clamp_int(pct, 0, 100);
}

static inline lv_coord_t axis_to_px(int16_t value) {
    int32_t px = ((int32_t) value * stick_radius_px) / STICK_AXE_MAX;

    if (px > stick_radius_px) px = stick_radius_px;
    if (px < -stick_radius_px) px = -stick_radius_px;

    return (lv_coord_t) px;
}

static uint32_t isqrt_u32(uint32_t value) {
    uint32_t bit = UINT32_C(1) << 30;
    uint32_t root = 0;

    while (bit > value) bit >>= 2;

    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }

        bit >>= 2;
    }

    return root;
}

static int stick_deadzone_raw(void) {
    if (stick_radius_px <= 0) return 0;

    return ((int) stick_deadzone_px * STICK_AXE_MAX) / (int) stick_radius_px;
}

static int stick_magnitude_raw(int16_t x, int16_t y) {
    uint32_t ax = (uint32_t) axis_abs(x);
    uint32_t ay = (uint32_t) axis_abs(y);

    return (int) isqrt_u32(ax * ax + ay * ay);
}

static int stick_magnitude_pct(int16_t x, int16_t y) {
    int pct = (stick_magnitude_raw(x, y) * 100) / STICK_AXE_MAX;

    return clamp_int(pct, 0, 142);
}

static const char *stick_state_text(int16_t x, int16_t y) {
    int ax = axis_abs(x);
    int ay = axis_abs(y);
    int dz = stick_deadzone_raw();
    int mag = stick_magnitude_raw(x, y);

    if (mag <= dz / 3) return TRS("CENTRE");
    if (mag <= dz) return TRS("DEADZONE");

    if (ax > dz * 2 && ay <= ax / 8) return TRS("SNAP-X");
    if (ay > dz * 2 && ax <= ay / 8) return TRS("SNAP-Y");

    int max_axis = ax > ay ? ax : ay;
    int diff = ax > ay ? ax - ay : ay - ax;

    if (ax > dz && ay > dz && diff <= max_axis / 5) return TRS("DIAG");
    if (max_axis >= (STICK_AXE_MAX * 95) / 100) return TRS("EDGE");

    return TRS("ACTIVE");
}

static void reset_stick_state(stick_state_t *stick) {
    if (!stick) return;

    stick->x = 0;
    stick->y = 0;

    stick->peak_x_pct = 0;
    stick->peak_y_pct = 0;

    stick->trail_count = 0;
    stick->trail_index = 0;
}

static void update_stick_peak(stick_state_t *stick) {
    int x_pct = axis_pct_abs(stick->x);
    int y_pct = axis_pct_abs(stick->y);

    if (x_pct > stick->peak_x_pct) stick->peak_x_pct = x_pct;
    if (y_pct > stick->peak_y_pct) stick->peak_y_pct = y_pct;
}

static void push_stick_sample(stick_state_t *stick) {
    if (stick->trail_count < STICK_TRL_NUM) {
        stick->trail[stick->trail_count].x = stick->x;
        stick->trail[stick->trail_count].y = stick->y;

        stick->trail_count++;

        stick->trail_index = stick->trail_count % STICK_TRL_NUM;
        return;
    }

    stick->trail[stick->trail_index].x = stick->x;
    stick->trail[stick->trail_index].y = stick->y;

    stick->trail_index = (stick->trail_index + 1) % STICK_TRL_NUM;
}

static void show_icon(const char *name) {
    char path[MAX_BUFFER_SIZE];
    char embed[MAX_BUFFER_SIZE];

    generate_image_embed(mux_dim, mux_module, name, path, sizeof(path), embed, sizeof(embed));

    if (file_exist(path)) {
        lv_img_set_src(ui_imgButton, embed);
    } else {
        lv_img_set_src(ui_imgButton, &ui_img_blank);
    }

    if (ui_pnlInputPreview) lv_obj_clear_flag(ui_pnlInputPreview, LV_OBJ_FLAG_HIDDEN);
}

static void clear_icon(void) {
    lv_img_set_src(ui_imgButton, &ui_img_blank);

    if (ui_pnlInputPreview) lv_obj_add_flag(ui_pnlInputPreview, LV_OBJ_FLAG_HIDDEN);
}

static void draw_outer_guides(lv_obj_t *canvas, int target, lv_color_t ring_col) {
    lv_draw_rect_dsc_t rect_dsc;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_width = 2;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = STICK_OPA_OUT_SQR;
    rect_dsc.radius = stick_square_radius_px;
    lv_canvas_draw_rect(canvas, 1, 1, (lv_coord_t) (target - 2), (lv_coord_t) (target - 2), &rect_dsc);

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_width = 2;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = STICK_OPA_OUT_CIR;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, 1, 1, (lv_coord_t) (target - 2), (lv_coord_t) (target - 2), &rect_dsc);
}

static void draw_stick_crosshair(lv_obj_t *canvas, int target, int centre, lv_color_t ring_col) {
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = ring_col;
    line_dsc.opa = STICK_OPA_AXIS;
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
}

static void draw_deadzone_guides(lv_obj_t *canvas, int centre, int deadzone, lv_color_t ring_col) {
    int inner = deadzone * 2;
    int inner_pos = centre - deadzone;

    lv_draw_rect_dsc_t rect_dsc;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = ring_col;
    rect_dsc.bg_opa = STICK_OPA_DZ_FILL;
    rect_dsc.border_width = 1;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = STICK_OPA_DZ_SQR;
    rect_dsc.radius = stick_square_radius_px;
    lv_canvas_draw_rect(canvas, (lv_coord_t) inner_pos, (lv_coord_t) inner_pos, (lv_coord_t) inner, (lv_coord_t) inner, &rect_dsc);

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = ring_col;
    rect_dsc.bg_opa = STICK_OPA_DZ_FILL;
    rect_dsc.border_width = 1;
    rect_dsc.border_color = ring_col;
    rect_dsc.border_opa = STICK_OPA_DZ_CIR;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, (lv_coord_t) inner_pos, (lv_coord_t) inner_pos, (lv_coord_t) inner, (lv_coord_t) inner, &rect_dsc);

    int centre_mark = deadzone / 3;
    if (centre_mark < 8) centre_mark = 8;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = ring_col;
    line_dsc.opa = STICK_OPA_CENTRE;
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
}

static void draw_stick_static(stick_state_t *stick) {
    if (!stick->static_canvas) return;

    int target = (int) stick_target_px;
    int deadzone = (int) stick_deadzone_px;
    int centre = target / 2;

    if (target <= 0 || deadzone <= 0) return;

    lv_color_t ring_col = lv_color_hex(theme.MESSAGE.BORDER);

    lv_canvas_fill_bg(stick->static_canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);

    draw_outer_guides(stick->static_canvas, target, ring_col);
    draw_stick_crosshair(stick->static_canvas, target, centre, ring_col);
    draw_deadzone_guides(stick->static_canvas, centre, deadzone, ring_col);

    lv_obj_invalidate(stick->static_canvas);
}

static void draw_stick_trail(lv_obj_t *canvas, const stick_state_t *stick, int centre, int dot, lv_color_t dot_col) {
    if (stick->trail_count <= 1) return;

    int trail_dot = dot / 2;
    if (trail_dot < 6) trail_dot = 6;

    int start = stick->trail_count < STICK_TRL_NUM ? 0 : stick->trail_index;

    for (int i = 0; i < stick->trail_count; i++) {
        int idx = (start + i) % STICK_TRL_NUM;
        int opa = STICK_TRAIL_OPA_BASE + ((i + 1) * STICK_TRAIL_OPA_RANGE) / stick->trail_count;

        int x = centre + axis_to_px(stick->trail[idx].x) - trail_dot / 2;
        int y = centre + axis_to_px(stick->trail[idx].y) - trail_dot / 2;

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);

        rect_dsc.bg_color = dot_col;
        rect_dsc.bg_opa = (lv_opa_t) opa;
        rect_dsc.border_width = 0;
        rect_dsc.radius = LV_RADIUS_CIRCLE;

        lv_canvas_draw_rect(canvas, (lv_coord_t) x, (lv_coord_t) y, (lv_coord_t) trail_dot, (lv_coord_t) trail_dot, &rect_dsc);
    }
}

static void draw_peak_markers(lv_obj_t *canvas, const stick_state_t *stick, int centre, int radius, lv_color_t peak_col) {
    if (stick->peak_x_pct < STICK_PEAK_MIN_PCT && stick->peak_y_pct < STICK_PEAK_MIN_PCT) return;

    lv_draw_rect_dsc_t marker;
    lv_draw_rect_dsc_init(&marker);

    marker.bg_color = peak_col;
    marker.bg_opa = LV_OPA_60;
    marker.border_width = 0;
    marker.radius = LV_RADIUS_CIRCLE;

    int sz = STICK_PEAK_MARKER;
    int half = sz / 2;

    if (stick->peak_x_pct >= STICK_PEAK_MIN_PCT) {
        int px = centre + (radius * stick->peak_x_pct) / 100 - half;
        int py = centre - half;
        lv_canvas_draw_rect(canvas, (lv_coord_t) px, (lv_coord_t) py, (lv_coord_t) sz, (lv_coord_t) sz, &marker);

        int px_neg = centre - (radius * stick->peak_x_pct) / 100 - half;
        lv_canvas_draw_rect(canvas, (lv_coord_t) px_neg, (lv_coord_t) py, (lv_coord_t) sz, (lv_coord_t) sz, &marker);
    }

    if (stick->peak_y_pct >= STICK_PEAK_MIN_PCT) {
        int px = centre - half;
        int py = centre + (radius * stick->peak_y_pct) / 100 - half;
        lv_canvas_draw_rect(canvas, (lv_coord_t) px, (lv_coord_t) py, (lv_coord_t) sz, (lv_coord_t) sz, &marker);

        int py_neg = centre - (radius * stick->peak_y_pct) / 100 - half;
        lv_canvas_draw_rect(canvas, (lv_coord_t) px, (lv_coord_t) py_neg, (lv_coord_t) sz, (lv_coord_t) sz, &marker);
    }
}

static void draw_stick_dot(lv_obj_t *canvas, int target, int centre, int dot, int16_t x, int16_t y, lv_color_t dot_col) {
    int dot_x = centre + axis_to_px(x) - dot / 2;
    int dot_y = centre + axis_to_px(y) - dot / 2;

    if (dot_x < 0) dot_x = 0;
    if (dot_y < 0) dot_y = 0;

    if (dot_x > target - dot) dot_x = target - dot;
    if (dot_y > target - dot) dot_y = target - dot;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);

    rect_dsc.bg_color = dot_col;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_width = 0;
    rect_dsc.radius = LV_RADIUS_CIRCLE;

    lv_canvas_draw_rect(canvas, (lv_coord_t) dot_x, (lv_coord_t) dot_y, (lv_coord_t) dot, (lv_coord_t) dot, &rect_dsc);
}

static void draw_stick_dynamic(stick_state_t *stick) {
    if (!stick->dynamic_canvas) return;

    int target = (int) stick_target_px;
    int radius = (int) stick_radius_px;
    int dot = (int) stick_dot_px;
    int centre = target / 2;

    if (target <= 0 || radius <= 0 || dot <= 0) return;

    lv_color_t dot_col = lv_color_hex(theme.LIST_DEFAULT.TEXT);
    lv_color_t peak_col = lv_color_hex(theme.MESSAGE.BORDER);

    lv_canvas_fill_bg(stick->dynamic_canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);

    draw_stick_trail(stick->dynamic_canvas, stick, centre, dot, dot_col);
    draw_peak_markers(stick->dynamic_canvas, stick, centre, radius, peak_col);
    draw_stick_dot(stick->dynamic_canvas, target, centre, dot, stick->x, stick->y, dot_col);

    lv_obj_invalidate(stick->dynamic_canvas);
}

static void set_stick_value_label(stick_state_t *stick) {
    if (!stick->label) return;

    char value[160];
    snprintf(value, sizeof(value),
             "X:%6d %4d%%\n"
             "Y:%6d %4d%%\n"
             "M:%3d%% %s\n"
             "PX:%3d%% PY:%3d%%",
             stick->x, axis_pct_signed(stick->x),
             stick->y, axis_pct_signed(stick->y),
             stick_magnitude_pct(stick->x, stick->y),
             stick_state_text(stick->x, stick->y),
             stick->peak_x_pct, stick->peak_y_pct);

    lv_label_set_text(stick->label, value);
}

static void update_stick_canvas(stick_state_t *stick, int16_t x, int16_t y) {
    if (!stick || !stick->dynamic_canvas) return;

    int dx = (int) x - (int) stick->x;
    int dy = (int) y - (int) stick->y;

    if (dx > -STICK_REDRAW_DELTA && dx < STICK_REDRAW_DELTA && dy > -STICK_REDRAW_DELTA && dy < STICK_REDRAW_DELTA) return;

    stick->x = x;
    stick->y = y;

    update_stick_peak(stick);
    push_stick_sample(stick);

    draw_stick_dynamic(stick);
    set_stick_value_label(stick);
}

static void handle_input(mux_input_type type, mux_input_action action) {
    if (action == MUX_INPUT_PRESS) {
        if (!glyph[type]) return;

        lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
        show_icon(glyph[type]);
    } else if (action == MUX_INPUT_RELEASE) {
        if (glyph[type]) clear_icon();
    }
}

static void handle_analog(int16_t ls_x, int16_t ls_y, int16_t rs_x, int16_t rs_y) {
    update_stick_canvas(&stick_l, ls_x, ls_y);
    if (device.BOARD.HASSTICK >= 2) update_stick_canvas(&stick_r, rs_x, rs_y);
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

static void create_input_preview_panel(lv_obj_t *parent, int shorter) {
    int preview = shorter / 4;

    if (preview < 96) preview = 96;
    if (preview > 240) preview = 240;

    ui_pnlInputPreview = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_pnlInputPreview);
    lv_obj_set_size(ui_pnlInputPreview, (lv_coord_t) preview, (lv_coord_t) preview);
    lv_obj_align(ui_pnlInputPreview, LV_ALIGN_CENTER, 0, -32);
    lv_obj_set_style_radius(ui_pnlInputPreview, (lv_coord_t) (stick_square_radius_px * 2), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlInputPreview, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlInputPreview, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlInputPreview, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlInputPreview, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(ui_pnlInputPreview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui_pnlInputPreview, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_pnlInputPreview, LV_OBJ_FLAG_HIDDEN);
}

static int alloc_stick_buffers(stick_state_t *stick) {
    size_t buf_size = (size_t) LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(stick_target_px, stick_target_px);

    stick->static_buf = lv_mem_alloc(buf_size);
    stick->dynamic_buf = lv_mem_alloc(buf_size);

    if (!stick->static_buf || !stick->dynamic_buf) {
        if (stick->static_buf) {
            lv_mem_free(stick->static_buf);
            stick->static_buf = NULL;
        }
        if (stick->dynamic_buf) {
            lv_mem_free(stick->dynamic_buf);
            stick->dynamic_buf = NULL;
        }
        return -1;
    }

    return 0;
}

static void free_stick_buffers(stick_state_t *stick) {
    if (stick->static_buf) {
        lv_mem_free(stick->static_buf);
        stick->static_buf = NULL;
    }
    if (stick->dynamic_buf) {
        lv_mem_free(stick->dynamic_buf);
        stick->dynamic_buf = NULL;
    }
}

static lv_obj_t *create_stick_target(lv_obj_t *parent, lv_align_t align, lv_coord_t x_offset, stick_state_t *stick) {
    if (!stick) return NULL;
    if (alloc_stick_buffers(stick) != 0) return NULL;

    lv_coord_t panel_h = (lv_coord_t) (stick_target_px + stick_label_gap_px);

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, stick_target_px, panel_h);
    lv_obj_align(panel, align, x_offset, stick_panel_y_offset_px);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    stick->static_canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(stick->static_canvas, stick->static_buf, stick_target_px, stick_target_px, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_align(stick->static_canvas, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(stick->static_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(stick->static_canvas, LV_OBJ_FLAG_CLICKABLE);

    stick->dynamic_canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(stick->dynamic_canvas, stick->dynamic_buf, stick_target_px, stick_target_px, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_align(stick->dynamic_canvas, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(stick->dynamic_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(stick->dynamic_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(stick->dynamic_canvas);

    stick->label = lv_label_create(panel);
    lv_obj_set_width(stick->label, stick_target_px);
    lv_label_set_long_mode(stick->label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(stick->label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(stick->label, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(stick->label, lv_color_hex(theme.LIST_DEFAULT.TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(stick->label, theme.LIST_DEFAULT.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_align(stick->label, LV_ALIGN_BOTTOM_MID, 0, 0);

    draw_stick_static(stick);
    draw_stick_dynamic(stick);
    set_stick_value_label(stick);

    return panel;
}

static void init_elements(void) {
    header_and_footer_setup();

    struct screen_dimension dims = get_device_dimensions();
    int w = dims.WIDTH;
    int h = dims.HEIGHT;
    int shorter = (w < h) ? w : h;

    int chrome_reserve = (h / 8) * 2;
    int label_reserve = STICK_LBL_MIN;
    int max_target_h = h - chrome_reserve - label_reserve;
    int radius_from_h = max_target_h / 2;
    int radius_from_w = shorter / 5;

    int radius = (radius_from_h < radius_from_w) ? radius_from_h : radius_from_w;
    radius = clamp_int(radius, STICK_RAD_MIN, STICK_RAD_MAX);

    stick_panel_y_offset_px = -16;

    stick_radius_px = (lv_coord_t) radius;
    stick_target_px = (lv_coord_t) (radius * 2);

    stick_square_radius_px = (lv_coord_t) clamp_int(radius / 12, 4, 16);
    stick_label_gap_px = (lv_coord_t) clamp_int((radius * 3) / 2, STICK_LBL_MIN, STICK_LBL_MAX);

    int deadzone = radius / 4;
    if (deadzone < 18) deadzone = 18;
    stick_deadzone_px = (lv_coord_t) deadzone;

    int dot = radius / 5;
    if (dot < 14) dot = 14;
    stick_dot_px = (lv_coord_t) dot;

    int edge_gap = w / 40;
    if (edge_gap < 18) edge_gap = 18;
    if (edge_gap > 64) edge_gap = 64;
    stick_edge_gap_px = (lv_coord_t) edge_gap;

    create_input_preview_panel(ui_screen, shorter);

    ui_imgButton = lv_img_create(ui_screen);
    lv_img_set_src(ui_imgButton, &ui_img_blank);
    lv_obj_align(ui_imgButton, LV_ALIGN_CENTER, 0, -32);
    lv_obj_set_style_img_recolor(ui_imgButton, lv_color_hex(theme.LIST_DEFAULT.GLYPH_RECOLOUR), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_imgButton, theme.LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA, MU_OBJ_MAIN_DEFAULT);

    if (device.BOARD.HASSTICK == 1) {
        reset_stick_state(&stick_l);

        ui_pnlStickL = create_stick_target(ui_screen, LV_ALIGN_LEFT_MID, stick_edge_gap_px, &stick_l);

        if (ui_pnlStickL) lv_obj_move_foreground(ui_pnlStickL);
    } else if (device.BOARD.HASSTICK >= 2) {
        reset_stick_state(&stick_l);
        reset_stick_state(&stick_r);

        ui_pnlStickL = create_stick_target(ui_screen, LV_ALIGN_LEFT_MID, stick_edge_gap_px, &stick_l);
        ui_pnlStickR = create_stick_target(ui_screen, LV_ALIGN_RIGHT_MID, (lv_coord_t) -stick_edge_gap_px, &stick_r);

        if (ui_pnlStickL) lv_obj_move_foreground(ui_pnlStickL);
        if (ui_pnlStickR) lv_obj_move_foreground(ui_pnlStickR);
    }

    if (ui_pnlInputPreview) lv_obj_move_foreground(ui_pnlInputPreview);
    lv_obj_move_foreground(ui_imgButton);

    lv_label_set_text(ui_lblMessage, config.SETTINGS.REMAP.LAYOUT ? lang.MUXTESTER.QUIT_ALT : lang.MUXTESTER.QUIT);
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
            .hold_disabled = 1,
            .input_handler = handle_input,
            .idle_handler = handle_idle,
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_DPAD_DOWN) | BIT(config.SETTINGS.REMAP.LAYOUT ? MUX_INPUT_A : MUX_INPUT_B),
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

    free_stick_buffers(&stick_l);
    free_stick_buffers(&stick_r);

    return 0;
}
