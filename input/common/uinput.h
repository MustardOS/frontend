#pragma once

#include <stddef.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define MUOS_GAMEPAD_NAME "muOS-Keys"

enum muos_gamepad_identity {
    muos_input_vendor = 0x756d,
    muos_input_version = 0x736f,

    muos_product_tui_spoon = 0x0001,
    muos_product_tui_brick = 0x0002,
    muos_product_tui_brick_pro = 0x0003,
    muos_product_tui_smpro_s = 0x0004,
    muos_product_gcs_h36s = 0x0005,
    muos_product_mgx_zero28 = 0x0006,
    muos_product_rgsp = 0x0701,
    muos_product_rg28xx_h = 0x0702,
    muos_product_rg34xx_h = 0x0703,
    muos_product_rg34xx_sp = 0x0704,
    muos_product_rg35xx_2024 = 0x0705,
    muos_product_rg35xx_h = 0x0706,
    muos_product_rg35xx_plus = 0x0707,
    muos_product_rg35xx_pro = 0x0708,
    muos_product_rg35xx_sp = 0x0709,
    muos_product_rg40xx_h = 0x070a,
    muos_product_rg40xx_v = 0x070b,
    muos_product_rgcubexx_h = 0x070c,
    muos_product_rk_g350_v = 0x3501,
    muos_product_rk_pixel_2 = 0x3502,
    muos_product_rg_vita_pro = 0x3503,
};

struct gamepad_abs_desc {
    unsigned int code;
    int min;
    int max;
    int flat;
    int fuzz;
    int resolution;
};

struct gamepad_desc {
    const char *name;
    struct input_id id;
    const unsigned short *keys;
    size_t key_count;
    const struct gamepad_abs_desc *axes;
    size_t axis_count;
    const unsigned short *switches;
    size_t switch_count;
    unsigned int ff_effects_max;
    int enable_ff_rumble;
    const unsigned short *ff_effects;
    size_t ff_effect_count;
};

typedef void (*gamepad_event_observer)(const struct input_event *event, void *context);

struct gamepad *gamepad_initialise(const struct gamepad_desc *desc);

void gamepad_emit_key(struct gamepad *gp, unsigned short code, int value);

void gamepad_emit_abs(struct gamepad *gp, unsigned short code, int value);

void gamepad_emit_sw(struct gamepad *gp, unsigned short code, int value);

void gamepad_sync(struct gamepad *gp);

void gamepad_destroy(struct gamepad *gp);

int gamepad_get_fd(struct gamepad *gp);

int gamepad_read_event(struct gamepad *gp, struct input_event *ev);

void gamepad_set_event_observer(struct gamepad *gp, gamepad_event_observer observer, void *context);

int gamepad_get_event_path(struct gamepad *gp, char *path, size_t path_size);
