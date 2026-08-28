#pragma once

#include <stddef.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define MUOS_GAMEPAD_NAME "muOS-Keys"

enum muos_gamepad_identity {

    MUOS_INPUT_VENDOR = 0x756d,

    MUOS_INPUT_VERSION = 0x736f,
    MUOS_PRODUCT_TUI_SPOON = 0x0001,
    MUOS_PRODUCT_TUI_BRICK = 0x0002,
    MUOS_PRODUCT_TUI_BRICK_PRO = 0x0003,
    MUOS_PRODUCT_TUI_SMPRO_S = 0x0004,
    MUOS_PRODUCT_GCS_H36S = 0x0005,
    MUOS_PRODUCT_MGX_ZERO28 = 0x0006,
    MUOS_PRODUCT_RGSP = 0x0701,
    MUOS_PRODUCT_RG28XX_H = 0x0702,
    MUOS_PRODUCT_RG34XX_H = 0x0703,
    MUOS_PRODUCT_RG34XX_SP = 0x0704,
    MUOS_PRODUCT_RG35XX_2024 = 0x0705,
    MUOS_PRODUCT_RG35XX_H = 0x0706,
    MUOS_PRODUCT_RG35XX_PLUS = 0x0707,
    MUOS_PRODUCT_RG35XX_PRO = 0x0708,
    MUOS_PRODUCT_RG35XX_SP = 0x0709,
    MUOS_PRODUCT_RG40XX_H = 0x070a,
    MUOS_PRODUCT_RG40XX_V = 0x070b,
    MUOS_PRODUCT_RGCUBEXX_H = 0x070c,
    MUOS_PRODUCT_RK_G350_V = 0x3501,
    MUOS_PRODUCT_RK_PIXEL_2 = 0x3502,
    MUOS_PRODUCT_RG_VITA_PRO = 0x3503,
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
