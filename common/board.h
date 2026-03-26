#pragma once

typedef enum {
    BOARD_SPECIAL_NONE = 0,
    BOARD_SPECIAL_G350,
    BOARD_SPECIAL_TUI_BRICK,
    BOARD_SPECIAL_TUI_SPOON,
} board_special_t;

typedef struct {
    const char *name;
    const char *code;
    board_special_t special;
} board_info_t;

void board_init(const char *code);

const board_info_t *board_current(void);

const char *board_name(void);

board_special_t board_special(void);

int board_is(board_special_t type);

int board_is_special(void);

int board_volume_event_index(void);
