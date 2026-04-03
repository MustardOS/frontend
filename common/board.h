#pragma once

typedef enum {
    BOARD_SPECIAL_NONE = 0,
    BOARD_SPECIAL_G350,
    BOARD_SPECIAL_TUI_BRICK,
    BOARD_SPECIAL_TUI_SPOON,
    BOARD_SPECIAL_VITA_PRO,
} board_special_t;

typedef struct {
    const char *name;
    const char *code;
    board_special_t special;
    int vol_event;
    int pwr_event;
    int lid_event;
} board_info_t;

enum {
    NOP = -1,
    EV0 = 0,
    EV1 = 1,
    EV2 = 2,
    EV3 = 3,
    EV4 = 4,
    EV5 = 5,
    EV6 = 6,
    EV7 = 7,
    EV8 = 8,
};

void board_init(const char *code);

const board_info_t *board_current(void);

const char *board_name(void);

board_special_t board_special(void);

int board_is(board_special_t type);

int board_is_special(void);

int board_volume_event_index(void);

int board_power_event_index(void);

int board_lid_event_index(void);
