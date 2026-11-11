#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <common/ui/nav/cell_art.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/grid.h>
#include <common/runtime/log.h>
#include <common/display/language.h>
#include <common/display/theme.h>
#include <common/config/config.h>
#include <common/content/core/common.h>
#include <module/muxshare.h>
#include <common/storage/fileio.h>
#include <json/json.h>
#include <common/ui/nav/grid_layout.h>
#include <common/ui/nav/grid_motion.h>

#define GRID_ANIM_SLOTS 16

#define GRID_ANIM_STEPS 1024

typedef struct {
    int cx, cy, w, h, rx, ry, angle, opa;
} grid_slot_pose;

static grid_slot_pose slot_rest[GRID_ANIM_SLOTS];
static grid_slot_pose slot_from[GRID_ANIM_SLOTS];
static grid_slot_pose slot_now[GRID_ANIM_SLOTS];
static int slot_rest_count = 0;
static int32_t slot_last_v = GRID_ANIM_STEPS;
static int slot_orbit = 0;
static int slot_swing = 0;
static float slot_orbit_cx, slot_orbit_cy, slot_orbit_rx, slot_orbit_ry;

static int lerp(const int from, const int to, const int32_t v) {
    return from + (to - from) * v / GRID_ANIM_STEPS;
}

static void
slot_orbit_point(const grid_slot_pose *from, const grid_slot_pose *rest, const int32_t v, int *cx, int *cy) {
    const float fx = ((float) from->cx - slot_orbit_cx) / slot_orbit_rx;
    const float fy = ((float) from->cy - slot_orbit_cy) / slot_orbit_ry;
    const float tx = ((float) rest->cx - slot_orbit_cx) / slot_orbit_rx;
    const float ty = ((float) rest->cy - slot_orbit_cy) / slot_orbit_ry;

    const float from_angle = atan2f(fy, fx);
    float turn = atan2f(ty, tx) - from_angle;
    if (turn > (float) M_PI) turn -= 2.0f * (float) M_PI;
    if (turn < -(float) M_PI) turn += 2.0f * (float) M_PI;

    const float p = (float) v / GRID_ANIM_STEPS;
    const float from_radius = sqrtf(fx * fx + fy * fy);
    const float radius = from_radius + (sqrtf(tx * tx + ty * ty) - from_radius) * p;
    const float angle = from_angle + turn * p;

    *cx = (int) lroundf(slot_orbit_cx + cosf(angle) * radius * slot_orbit_rx);
    *cy = (int) lroundf(slot_orbit_cy + sinf(angle) * radius * slot_orbit_ry);
}

static grid_slot_pose slot_pose_at(const grid_slot_pose *from, const grid_slot_pose *rest, const int32_t v) {
    if (v == GRID_ANIM_STEPS) return *rest;

    grid_slot_pose pose;
    pose.cx = lerp(from->cx, rest->cx, v);
    pose.cy = lerp(from->cy, rest->cy, v);
    if (slot_orbit) slot_orbit_point(from, rest, v, &pose.cx, &pose.cy);

    pose.w = lerp(from->w, rest->w, v);
    pose.h = lerp(from->h, rest->h, v);
    pose.rx = lerp(from->rx, rest->rx, v);
    pose.ry = lerp(from->ry, rest->ry, v);
    pose.angle = lerp(from->angle, rest->angle, v);

    const int opa = lerp(from->opa, rest->opa, v);
    pose.opa = opa < 0 ? 0 : opa > 255 ? 255 : opa;
    return pose;
}

static void slot_pose_apply(lv_obj_t *panel, const grid_slot_pose *rest, const grid_slot_pose *pose) {
    const int side = rest->w > rest->h ? rest->w : rest->h;
    const int reshaped = abs(pose->w * rest->h - pose->h * rest->w) > side;
    const int w = reshaped ? pose->w : rest->w;
    const int h = reshaped ? pose->h : rest->h;
    if (lv_obj_get_width(panel) != w || lv_obj_get_height(panel) != h) {
        lv_obj_set_size(panel, (lv_coord_t) w, (lv_coord_t) h);
        cell_art_resize(panel, w, h);
    }

    lv_obj_set_style_translate_x(panel, (lv_coord_t) (pose->cx - rest->cx - (w - rest->w) / 2), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_y(panel, (lv_coord_t) (pose->cy - rest->cy - (h - rest->h) / 2), MU_OBJ_MAIN_DEFAULT);

    const int zoom = reshaped || rest->w <= 0 ? LV_IMG_ZOOM_NONE : pose->w * LV_IMG_ZOOM_NONE / rest->w;
    lv_obj_set_style_transform_zoom(panel, (lv_coord_t) (zoom > 0 ? zoom : 1), MU_OBJ_MAIN_DEFAULT);
    cell_set_opa(panel, (lv_opa_t) pose->opa);
    if (rest->angle || pose->angle) {
        lv_obj_set_style_transform_angle(panel, (lv_coord_t) pose->angle, MU_OBJ_MAIN_DEFAULT);
    }

    const _lv_obj_spec_attr_t *attr = panel->spec_attr;
    if (attr && (attr->rotate_x != pose->rx || attr->rotate_y != pose->ry)) {
        lv_obj_set_transform_3d(
            panel, (int16_t) pose->rx, (int16_t) pose->ry, attr->depth, attr->perspective, attr->reflect
        );
    }
}

static void slot_anim_exec(void *var, const int32_t v) {
    lv_obj_t *grid = var;
    slot_last_v = v;

    for (int i = 0; i < slot_rest_count; i++) {
        lv_obj_t *panel = lv_obj_get_child(grid, i);
        if (!panel) continue;

        const grid_slot_pose pose = slot_pose_at(&slot_from[i], &slot_rest[i], v);
        slot_pose_apply(panel, &slot_rest[i], &pose);
    }
}

static int32_t slot_path_swing(const lv_anim_t *a) {
    const uint32_t t = lv_map(a->act_time, 0, a->time, 0, LV_BEZIER_VAL_MAX);
    const int32_t step = (int32_t) lv_bezier3(t, 0, 600, 1150, LV_BEZIER_VAL_MAX);
    return a->start_value + ((step * (a->end_value - a->start_value)) >> LV_BEZIER_VAL_SHIFT);
}

static void slot_anim_settle(void) {
    const int running = lv_anim_get(ui_pnl_grid, slot_anim_exec) != NULL;

    for (int i = 0; i < slot_rest_count; i++) {
        slot_now[i] = running ? slot_pose_at(&slot_from[i], &slot_rest[i], slot_last_v) : slot_rest[i];
    }

    if (!running) return;
    lv_anim_del(ui_pnl_grid, slot_anim_exec);
    slot_anim_exec(ui_pnl_grid, GRID_ANIM_STEPS);
}

static float orbit_value(const struct json root, const char *key, const float fallback) {
    const struct json value = json_object_get(root, key);
    return json_exists(value) ? (float) json_int(value) : fallback;
}

static void slot_capture_rest(const int count) {
    lv_obj_update_layout(ui_pnl_grid);
    slot_rest_count = 0;

    for (int i = 0; i < count && i < GRID_ANIM_SLOTS; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, i);
        if (!panel) break;

        grid_slot_pose *pose = &slot_rest[i];
        pose->w = lv_obj_get_width(panel);
        pose->h = lv_obj_get_height(panel);
        pose->cx = lv_obj_get_x(panel) + pose->w / 2;
        pose->cy = lv_obj_get_y(panel) + pose->h / 2;
        pose->rx = panel->spec_attr ? panel->spec_attr->rotate_x : 0;
        pose->ry = panel->spec_attr ? panel->spec_attr->rotate_y : 0;
        pose->angle = lv_obj_get_style_transform_angle(panel, LV_PART_MAIN);
        pose->opa = cell_opa(panel);

        lv_obj_set_style_transform_pivot_x(panel, (lv_coord_t) (pose->w / 2), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_transform_pivot_y(panel, (lv_coord_t) (pose->h / 2), MU_OBJ_MAIN_DEFAULT);
        slot_rest_count = i + 1;
    }

    slot_orbit = 0;
    slot_swing = 0;
    const char *data = grid_layout_json();
    if (!data || slot_rest_count < 3) return;
    const struct json root = json_parse(data);
    const struct json motion = json_object_get(root, "motion");
    slot_swing = json_string_compare(motion, "swing") == 0;
    if (json_string_compare(motion, "orbit") != 0) return;

    int min_x = slot_rest[0].cx, max_x = min_x, min_y = slot_rest[0].cy, max_y = min_y;
    for (int i = 1; i < slot_rest_count; i++) {
        if (slot_rest[i].cx < min_x) min_x = slot_rest[i].cx;
        if (slot_rest[i].cx > max_x) max_x = slot_rest[i].cx;
        if (slot_rest[i].cy < min_y) min_y = slot_rest[i].cy;
        if (slot_rest[i].cy > max_y) max_y = slot_rest[i].cy;
    }
    if (max_x - min_x < 2 || max_y - min_y < 2) return;

    slot_orbit_cx = orbit_value(root, "centre_x", (float) (min_x + max_x) / 2.0f);
    slot_orbit_cy = orbit_value(root, "centre_y", (float) (min_y + max_y) / 2.0f);
    slot_orbit_rx = orbit_value(root, "radius_x", (float) (max_x - min_x) / 2.0f);
    slot_orbit_ry = orbit_value(root, "radius_y", (float) (max_y - min_y) / 2.0f);
    slot_orbit = 1;
    slot_swing = 1;
}

static void label_fade_exec(void *var, const int32_t v) {
    lv_obj_set_style_opa(var, (lv_opa_t) v, MU_OBJ_MAIN_DEFAULT);
}

static void fade_in_current_label(void) {
    if (!ui_lbl_grid_current_item) return;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ui_lbl_grid_current_item);
    lv_anim_set_exec_cb(&anim, label_fade_exec);
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_time(&anim, (uint32_t) theme.grid.animation_time);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static uint32_t step_tick = 0;
static uint32_t step_ms = 0;

static uint32_t paced_step_time(void) {
    const uint32_t full = (uint32_t) theme.grid.animation_time;
    const uint32_t since = lv_tick_elaps(step_tick);
    step_tick = lv_tick_get();
    return since < full ? LV_MAX(since, 60) : full;
}

static void animate_carousel_step(const int direction) {
    const int step = direction < 0 ? -1 : 1;
    step_ms = paced_step_time();

    for (int i = 0; i < slot_rest_count; i++) {
        const int source = i + step;
        if (source >= 0 && source < slot_rest_count) {
            slot_from[i] = slot_now[source];
            continue;
        }

        const int inner = i - step;
        slot_from[i] = slot_rest[i];
        if (!slot_orbit && inner >= 0 && inner < slot_rest_count) {
            slot_from[i].cx += slot_rest[i].cx - slot_rest[inner].cx;
            slot_from[i].cy += slot_rest[i].cy - slot_rest[inner].cy;
        }
        slot_from[i].opa = 0;
    }

    const int last = slot_rest_count - 1;
    if (slot_orbit && last > 0 && slot_rest[0].cx == slot_rest[last].cx && slot_rest[0].cy == slot_rest[last].cy) {
        const int incoming = step > 0 ? last : 0;
        slot_from[incoming] = slot_from[last - incoming];
    }

    slot_anim_exec(ui_pnl_grid, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ui_pnl_grid);
    lv_anim_set_exec_cb(&anim, slot_anim_exec);
    lv_anim_set_values(&anim, 0, GRID_ANIM_STEPS);
    lv_anim_set_time(&anim, step_ms);
    lv_anim_set_path_cb(&anim, slot_swing ? slot_path_swing : lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static lv_obj_t *ghosts[2];
static grid_slot_pose ghost_from[2], ghost_to[2];
static int ghost_slot[2];

static void ghost_exec(void *var, const int32_t v) {
    const int side = var == ghosts[1];
    if (ghosts[side] != var) return;

    const grid_slot_pose pose = slot_pose_at(&ghost_from[side], &ghost_to[side], v);
    slot_pose_apply(var, &slot_rest[ghost_slot[side]], &pose);
}

static void ghost_done(lv_anim_t *anim) {
    lv_obj_del_async(anim->var);
}

static void ghost_deleted(lv_event_t *e) {
    const lv_obj_t *ghost = lv_event_get_target(e);
    for (int side = 0; side < 2; side++) {
        if (ghosts[side] == ghost) ghosts[side] = NULL;
    }
}

void grid_motion_carousel_ghost(lv_obj_t *ghost, const int direction) {
    const int side = direction > 0 ? 0 : 1;
    const int leave = side ? slot_rest_count - 1 : 0;
    const int inner = side ? leave - 1 : leave + 1;
    if (!ghost) return;
    if (slot_orbit || inner < 0 || inner >= slot_rest_count || slot_now[leave].opa <= LV_OPA_MIN) {
        lv_obj_del(ghost);
        return;
    }

    if (ghosts[side]) lv_obj_del(ghosts[side]);
    ghosts[side] = ghost;
    ghost_slot[side] = leave;
    ghost_from[side] = slot_now[leave];
    ghost_to[side] = slot_rest[leave];
    ghost_to[side].cx += slot_rest[leave].cx - slot_rest[inner].cx;
    ghost_to[side].cy += slot_rest[leave].cy - slot_rest[inner].cy;
    ghost_to[side].opa = 0;
    lv_obj_add_event_cb(ghost, ghost_deleted, LV_EVENT_DELETE, NULL);
    ghost_exec(ghost, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ghost);
    lv_anim_set_exec_cb(&anim, ghost_exec);
    lv_anim_set_values(&anim, 0, GRID_ANIM_STEPS);
    lv_anim_set_time(&anim, step_ms);
    lv_anim_set_path_cb(&anim, slot_swing ? slot_path_swing : lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, ghost_done);
    lv_anim_start(&anim);
}

typedef struct {
    lv_obj_t *panel;
    lv_coord_t x, y, w, h, radius;
} swell_rest;

static swell_rest swell_cells[GRID_ANIM_SLOTS];
static int swell_count = 0;

static const swell_rest *swell_find(const lv_obj_t *panel) {
    for (int i = 0; i < swell_count; i++) {
        if (swell_cells[i].panel == panel) return &swell_cells[i];
    }
    return NULL;
}

static void swell_exec(void *var, const int32_t v) {
    const swell_rest *rest = swell_find(var);
    if (!rest) return;

    const lv_coord_t w = (lv_coord_t) (rest->w * v / LV_IMG_ZOOM_NONE);
    const lv_coord_t h = (lv_coord_t) (rest->h * v / LV_IMG_ZOOM_NONE);
    lv_obj_set_size(var, w, h);
    lv_obj_set_pos(var, (lv_coord_t) (rest->x - (w - rest->w) / 2), (lv_coord_t) (rest->y - (h - rest->h) / 2));
    lv_obj_set_style_radius(var, (lv_coord_t) (rest->radius * v / LV_IMG_ZOOM_NONE), MU_OBJ_MAIN_DEFAULT);
}

static int32_t swell_now(const lv_obj_t *panel, const swell_rest *rest) {
    return rest->w > 0 ? lv_obj_get_width(panel) * LV_IMG_ZOOM_NONE / rest->w : LV_IMG_ZOOM_NONE;
}

static void swell_panel(lv_obj_t *panel, const int32_t target, const uint32_t time, const lv_anim_path_cb_t path) {
    const swell_rest *rest = swell_find(panel);
    if (!rest) return;

    const int32_t current = swell_now(panel, rest);
    if (current == target && !lv_anim_get(panel, swell_exec)) return;

    lv_anim_del(panel, swell_exec);
    if (!time) {
        swell_exec(panel, target);
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, panel);
    lv_anim_set_exec_cb(&anim, swell_exec);
    lv_anim_set_values(&anim, current, target);
    lv_anim_set_time(&anim, time);
    lv_anim_set_path_cb(&anim, path);
    lv_anim_start(&anim);
}

static void swell_capture(void) {
    lv_obj_update_layout(ui_pnl_grid);
    const uint32_t count = lv_obj_get_child_cnt(ui_pnl_grid);

    swell_count = 0;
    for (uint32_t i = 0; i < count && swell_count < GRID_ANIM_SLOTS; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, (int32_t) i);
        swell_cells[swell_count++] = (swell_rest) {panel,
                                                   lv_obj_get_x(panel),
                                                   lv_obj_get_y(panel),
                                                   lv_obj_get_width(panel),
                                                   lv_obj_get_height(panel),
                                                   lv_obj_get_style_radius(panel, LV_PART_MAIN)};
    }
}

static int focus_zoom(void) {
    const char *data = grid_layout_json();
    if (!data) return 0;

    const struct json value = json_object_get(json_parse(data), "focus_zoom");
    return json_exists(value) ? json_int(value) : 0;
}

void grid_motion_reset(void) {
    swell_count = 0;
    slot_rest_count = 0;
    slot_last_v = GRID_ANIM_STEPS;
}

static int grid_motion_enabled(void) {
    return theme.grid.animation_time > 0 && !config.visual.reduce_motion;
}

int grid_motion_owns(const lv_obj_t *obj) {
    return obj && ui_pnl_grid && lv_obj_get_parent(obj) == ui_pnl_grid && is_carousel_grid_mode()
           && grid_motion_enabled();
}

int grid_motion_carousel_prepare(const int count) {
    if (!grid_motion_enabled()) return 0;

    slot_anim_settle();
    if (slot_rest_count != count) {
        slot_capture_rest(count);
        for (int i = 0; i < slot_rest_count; i++)
            slot_now[i] = slot_rest[i];
    }

    return slot_rest_count == count;
}

void grid_motion_carousel_step(const int direction) {
    animate_carousel_step(direction);
    fade_in_current_label();
}

void grid_motion_swell_focused(void) {
    const int zoom = focus_zoom();
    if (zoom <= 0 || !ui_pnl_grid || is_carousel_grid_mode()) return;

    if (!swell_count) swell_capture();
    const lv_obj_t *focused = lv_group_get_focused(ui_group_panel);
    const uint32_t time = grid_motion_enabled() ? (uint32_t) theme.grid.animation_time : 0;
    const uint32_t count = lv_obj_get_child_cnt(ui_pnl_grid);

    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, (int32_t) i);
        if (panel == focused) {
            swell_panel(panel, LV_IMG_ZOOM_NONE * zoom / 100, time, lv_anim_path_overshoot);
        } else {
            swell_panel(panel, LV_IMG_ZOOM_NONE, time, lv_anim_path_ease_out);
        }
    }
}
