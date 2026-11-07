#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/base/options.h>
#include <common/display/theme.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/grid.h>
#include <common/ui/image.h>
#include <module/muxshare.h>
#include <common/ui/nav/grid_layout.h>
#include <common/ui/nav/mascot.h>
#include <json/json.h>

#define MASCOT_FRAMES 32

static lv_obj_t *mascot = NULL;
static lv_timer_t *frame_timer = NULL;
static char frame_paths[MASCOT_FRAMES][MAX_BUFFER_SIZE];
static int frame_count = 0;
static int frame_index = 0;
static int below = 1;

static char walk_paths[2][MASCOT_FRAMES][MAX_BUFFER_SIZE];
static int walk_count[2] = {0, 0};
static int walking = -1;
static int sprite_w = 0;
static int sprite_h = 0;

static lv_img_dsc_t pointer_dsc;
static uint8_t *pointer_pixels = NULL;

static int layout_number(const char *key, const int index) {
    const char *data = grid_layout_json();
    if (!data) return 0;

    const struct json value = json_object_get(json_parse(data), key);
    if (json_type(value) == JSON_ARRAY) return json_int(json_array_get(value, (size_t) index));
    return json_exists(value) && index == 0 ? json_int(value) : 0;
}

static int mascot_scale(void) {
    const int scale = layout_number("mascot_scale", 0);
    return scale > 0 ? scale : 100;
}

static int mascot_size(void) {
    return layout_number("mascot", 0) * mascot_scale() / 100;
}

static int layout_side(void) {
    const char *data = grid_layout_json();
    if (!data) return 0;

    const struct json side = json_object_get(json_parse(data), "mascot_side");
    if (json_string_compare(side, "above") == 0) return 1;
    if (json_string_compare(side, "below") == 0) return 2;
    return 0;
}

static int mascot_has_offset(void) {
    const char *data = grid_layout_json();
    return data && json_exists(json_object_get(json_parse(data), "mascot_offset"));
}

static int mascot_offset(const char *item, const int axis) {
    const char *data = grid_layout_json();
    if (!data) return 0;

    struct json offset = json_object_get(json_parse(data), "mascot_offset");
    if (json_type(offset) == JSON_OBJECT) offset = json_object_get(offset, item ? item : "");
    return json_type(offset) == JSON_ARRAY ? json_int(json_array_get(offset, (size_t) axis)) : 0;
}

static int find_frame(const char *element, char *embed) {
    char path[MAX_BUFFER_SIZE];
    if (!load_element_image_specifics(mux_dim, mux_module, "menu", element, element, "png", path, sizeof(path))) {
        return 0;
    }

    snprintf(embed, MAX_BUFFER_SIZE, "M:%s", path);
    return 1;
}

static int load_set(const char *folder, char paths[][MAX_BUFFER_SIZE]) {
    int count = 0;
    for (int i = 1; i <= MASCOT_FRAMES; i++) {
        char element[64];
        snprintf(element, sizeof(element), "%s/%d", folder, i);
        if (!find_frame(element, paths[count])) break;
        count++;
    }
    return count;
}

int mascot_sprite_height(void) {
    char path[MAX_BUFFER_SIZE];
    if (!find_frame("mascot/1", path) && !find_frame("mascot", path)) return 0;

    lv_img_header_t header;
    return lv_img_decoder_get_info(path, &header) == LV_RES_OK ? header.h : 0;
}

static void load_frames(void) {
    frame_count = load_set("mascot", frame_paths);
    if (!frame_count && find_frame("mascot", frame_paths[0])) frame_count = 1;

    walk_count[0] = load_set("walk_left", walk_paths[0]);
    walk_count[1] = load_set("walk_right", walk_paths[1]);
}

static const lv_img_dsc_t *pointer_image(const int size) {
    const int w = size, h = size * 3 / 4;
    free(pointer_pixels);
    pointer_pixels = calloc((size_t) w * h, 4);
    if (!pointer_pixels) return NULL;

    const uint32_t colour = theme.grid.cell_focus.background & 0xFFFFFF;
    uint32_t *out = (uint32_t *) pointer_pixels;
    for (int y = 0; y < h; y++) {
        const float half = (float) w / 2.0f * (float) (y + 1) / (float) h;
        for (int x = 0; x < w; x++) {
            const float from_centre = (float) x + 0.5f - (float) w / 2.0f;
            float cover = half - (from_centre < 0 ? -from_centre : from_centre) + 0.5f;
            cover = cover < 0.0f ? 0.0f : cover > 1.0f ? 1.0f : cover;
            out[(size_t) y * w + x] = colour | ((uint32_t) (255.0f * cover) << 24);
        }
    }

    pointer_dsc.header.always_zero = 0;
    pointer_dsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    pointer_dsc.header.w = (uint32_t) w;
    pointer_dsc.header.h = (uint32_t) h;
    pointer_dsc.data_size = (uint32_t) (w * h * 4);
    pointer_dsc.data = pointer_pixels;
    return &pointer_dsc;
}

static void frame_step(lv_timer_t *timer) {
    if (!mascot || !lv_obj_is_valid(mascot)) {
        lv_timer_del(timer);
        frame_timer = NULL;
        mascot = NULL;
        return;
    }

    if (walking >= 0 && walk_count[walking]) {
        frame_index = (frame_index + 1) % walk_count[walking];
        lv_img_set_src(mascot, walk_paths[walking][frame_index]);
        return;
    }
    if (frame_count < 2) return;

    frame_index = (frame_index + 1) % frame_count;
    lv_img_set_src(mascot, frame_paths[frame_index]);
}

static void bob_exec(void *var, const int32_t v) {
    lv_obj_set_style_translate_y(var, (lv_coord_t) v, MU_OBJ_MAIN_DEFAULT);
}

static void start_bob(const int size) {
    const char *data = grid_layout_json();
    const struct json bounce = data ? json_object_get(json_parse(data), "mascot_bounce") : (struct json) {0};
    const int set = json_type(bounce) == JSON_ARRAY;
    const int height = set ? size * json_int(json_array_get(bounce, 0)) / 100 : size / 12 + 1;
    const int speed = set ? json_int(json_array_get(bounce, 1)) : 420;
    if (height <= 0 || speed <= 0) return;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, mascot);
    lv_anim_set_exec_cb(&anim, bob_exec);
    lv_anim_set_values(&anim, 0, -height);
    lv_anim_set_time(&anim, (uint32_t) speed);
    lv_anim_set_playback_time(&anim, (uint32_t) speed);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_start(&anim);
}

static void create(lv_obj_t *screen, const int size) {
    sprite_w = sprite_h = 0;
    mascot = lv_img_create(screen);
    lv_obj_add_flag(mascot, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(mascot, LV_OBJ_FLAG_CLICKABLE);
    load_frames();
    frame_index = 0;

    if (frame_count) {
        lv_img_header_t header;
        lv_img_set_src(mascot, frame_paths[0]);
        if (lv_img_decoder_get_info(frame_paths[0], &header) == LV_RES_OK && header.w && header.h) {
            sprite_w = (int) header.w * mascot_scale() / 100;
            sprite_h = (int) header.h * mascot_scale() / 100;
        }
        lv_img_set_antialias(mascot, false);
        lv_img_set_pivot(mascot, 0, 0);
        lv_img_set_zoom(mascot, (uint16_t) (LV_IMG_ZOOM_NONE * mascot_scale() / 100));
        frame_timer = lv_timer_create(frame_step, 120, NULL);
    } else {
        lv_img_set_src(mascot, pointer_image(size));
        lv_obj_set_style_transform_pivot_x(mascot, size / 2, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_transform_pivot_y(mascot, size * 3 / 8, MU_OBJ_MAIN_DEFAULT);
    }

    start_bob(size);
}

static void move_exec_x(void *var, const int32_t v) {
    lv_obj_set_x(var, (lv_coord_t) v);
}

static void move_exec_y(void *var, const int32_t v) {
    lv_obj_set_y(var, (lv_coord_t) v);
}

static void walk_done(lv_anim_t *anim) {
    LV_UNUSED(anim);
    walking = -1;
    frame_index = 0;
    if (mascot && lv_obj_is_valid(mascot) && frame_count) lv_img_set_src(mascot, frame_paths[0]);
}

static void move_to(const lv_coord_t x, const lv_coord_t y, const int animate) {
    lv_anim_del(mascot, move_exec_x);
    lv_anim_del(mascot, move_exec_y);
    if (!animate) {
        lv_obj_set_pos(mascot, x, y);
        return;
    }

    const lv_coord_t from[] = {lv_obj_get_x(mascot), lv_obj_get_y(mascot)};
    if (x != from[0]) {
        walking = x < from[0] ? 0 : 1;
        frame_index = 0;
    }
    const lv_coord_t to[] = {x, y};
    const lv_anim_exec_xcb_t exec[] = {move_exec_x, move_exec_y};
    for (int axis = 0; axis < 2; axis++) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, mascot);
        lv_anim_set_exec_cb(&anim, exec[axis]);
        lv_anim_set_values(&anim, from[axis], to[axis]);
        lv_anim_set_time(&anim, (uint32_t) (theme.grid.animation_time > 0 ? theme.grid.animation_time : 300));
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
        if (axis == 0) lv_anim_set_ready_cb(&anim, walk_done);
        lv_anim_start(&anim);
    }
}

void mascot_remove(void) {
    if (frame_timer) lv_timer_del(frame_timer);
    frame_timer = NULL;
    if (mascot && lv_obj_is_valid(mascot)) lv_obj_del(mascot);
    mascot = NULL;
}

void mascot_follow(lv_obj_t *screen, lv_obj_t *target) {
    const int size = mascot_size();
    if (size <= 0 || !target) {
        mascot_remove();
        return;
    }

    const int created = !mascot || !lv_obj_is_valid(mascot) || lv_obj_get_screen(mascot) != screen;
    if (created) {
        mascot_remove();
        create(screen, size);
    }
    if (!created) {
        lv_anim_del(mascot, bob_exec);
        lv_obj_set_style_translate_y(mascot, 0, MU_OBJ_MAIN_DEFAULT);
        start_bob(size);
    }

    if (ui_pnl_grid) lv_obj_move_to_index(mascot, lv_obj_get_index(ui_pnl_grid) + 1);

    lv_obj_update_layout(target);
    lv_area_t area;
    lv_obj_get_coords(target, &area);
    const int centre_x = (area.x1 + area.x2) / 2;
    const int centre_y = (area.y1 + area.y2) / 2;
    const int side = layout_side();
    below = side ? side == 2 : centre_y < lv_obj_get_height(screen) / 2;

    if (!frame_count) {
        lv_obj_set_style_transform_angle(mascot, below ? 0 : 1800, MU_OBJ_MAIN_DEFAULT);
    }

    const int gap = size / 8;
    const int item = (int) IFU(lv_obj_get_user_data(target));
    const char *name = item >= 0 && (size_t) item < item_count ? items[item].glyph_icon : NULL;
    const int sprite = frame_count && sprite_w && !mascot_has_offset();
    const int width = sprite ? sprite_w : size;
    const int height = sprite ? sprite_h : frame_count ? size : size * 3 / 4;
    const int perched = sprite && side;
    const int top = below ? area.y2 + (perched ? -height / 4 : gap) : area.y1 - height + (perched ? height / 4 : -gap);
    const int nudge_x = layout_number("mascot_nudge", 0);
    const int nudge_y = layout_number("mascot_nudge", 1);
    const lv_coord_t x = (lv_coord_t) (centre_x - width / 2 + mascot_offset(name, 0) + nudge_x);
    const lv_coord_t y = (lv_coord_t) (top + mascot_offset(name, 1) + nudge_y);
    move_to(x, y, !created);
}
