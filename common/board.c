#include <string.h>
#include "board.h"
#include "log.h"

static const board_info_t board_table[] = {
        {"Generic H36S",       "gcs-h36s",    BOARD_SPECIAL_NONE,      -1, -1},
        {"MagicX Zero28",      "mgx-zero28",  BOARD_SPECIAL_NONE,      -1, -1},

        {"Anbernic RG28-H",    "rg28xx-h",    BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG34-H",    "rg34xx-h",    BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG34-SP",   "rg34xx-sp",   BOARD_SPECIAL_NONE,      -1, 0},

        {"Anbernic RG35-2024", "rg35xx-2024", BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG35-H",    "rg35xx-h",    BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG35-PLUS", "rg35xx-plus", BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG35-PRO",  "rg35xx-pro",  BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG35-SP",   "rg35xx-sp",   BOARD_SPECIAL_NONE,      -1, 0},

        {"Anbernic RG40-H",    "rg40xx-h",    BOARD_SPECIAL_NONE,      -1, 0},
        {"Anbernic RG40-V",    "rg40xx-v",    BOARD_SPECIAL_NONE,      -1, 0},

        {"Anbernic RGCUBE-H",  "rgcubexx-h",  BOARD_SPECIAL_NONE,      -1, 0},

        {"Batlexp G350",       "rk-g350-v",   BOARD_SPECIAL_G350,      -1, -1},
        {"GKD Pixel 2",        "rk-pixel-2",  BOARD_SPECIAL_NONE,      -1, -1},

        {"TrimUI Brick",       "tui-brick",   BOARD_SPECIAL_TUI_BRICK, -1, 1},
        {"TrimUI Smart Pro",   "tui-spoon",   BOARD_SPECIAL_TUI_SPOON, 0,  1},
};

#define BOARD_TABLE_SIZE (sizeof(board_table) / sizeof(board_table[0]))

static const board_info_t *current_board = NULL;

void board_init(const char *code) {
    current_board = NULL;

    for (size_t i = 0; i < BOARD_TABLE_SIZE; i++) {
        if (strcmp(code, board_table[i].code) == 0) {
            current_board = &board_table[i];
            break;
        }
    }

    if (!current_board) {
        LOG_WARN("board", "Unknown Board: %s", code);
        return;
    }

    LOG_INFO("board", "Detected Board: %s (%s) (Special: %d)", current_board->name, current_board->code, current_board->special);
}

const board_info_t *board_current(void) {
    return current_board;
}

const char *board_name(void) {
    return current_board ? current_board->name : "Unknown";
}

board_special_t board_special(void) {
    return current_board ? current_board->special : BOARD_SPECIAL_NONE;
}

int board_is(board_special_t type) {
    return board_special() == type;
}

int board_is_special(void) {
    return board_special() != BOARD_SPECIAL_NONE;
}

int board_volume_event_index(void) {
    return current_board ? current_board->vol_event : -1;
}

int board_power_event_index(void) {
    return current_board ? current_board->pwr_event : -1;
}
