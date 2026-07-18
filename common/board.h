#pragma once

typedef enum {
    board_special_none = 0,
    board_special_g350,
    board_special_tui_brick,
    board_special_tui_brick_pro,
    board_special_tui_spoon,
    board_special_vita_pro,
} board_special_t;

typedef enum {
    board_event_offset_none = 0,
    board_event_offset_touch,
} board_event_offset_condition_t;

typedef struct {
    board_event_offset_condition_t condition;
    int from_event;
    int offset;
} board_event_offset_t;

typedef struct {
    const char *name;
    const char *code;
    board_special_t special;
    int vol_event;
    int pwr_event;
    int lid_event;
    board_event_offset_t event_offset;
    int layout_map_swap;
} board_info_t;

enum {
    nop = -1,
    ev0 = 0,
    ev1 = 1,
    ev2 = 2,
    ev3 = 3,
    ev4 = 4,
    ev5 = 5,
    ev6 = 6,
    ev7 = 7,
    ev8 = 8,
};

enum {
    regular = 0,
    goofy = 1,
};

void board_init(const char *code);

const board_info_t *board_current(void);

const char *board_name(void);

board_special_t board_special(void);

int board_is(board_special_t type);

int board_is_special(void);

int board_layout_map_swap(void);

int board_volume_event_index(void);

int board_power_event_index(void);

int board_lid_event_index(void);
