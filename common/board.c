#include <string.h>
#include "board.h"
#include "device.h"
#include "log.h"

#define NO_EVENT_OFFSET {BOARD_EVENT_OFFSET_NONE, NOP, 0}

#define TOUCH_EVENT_OFFSET(FROM, ADD) {BOARD_EVENT_OFFSET_TOUCH, FROM, ADD}

static const board_info_t board_table[] = {
        {"Generic H36S",       "gcs-h36s",    BOARD_SPECIAL_NONE,      NOP, NOP, NOP, NO_EVENT_OFFSET},
        {"MagicX Zero28",      "mgx-zero28",  BOARD_SPECIAL_NONE,      NOP, NOP, NOP, NO_EVENT_OFFSET},

        {"Anbernic RG28-H",    "rg28xx-h",    BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG34-H",    "rg34xx-h",    BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG34-SP",   "rg34xx-sp",   BOARD_SPECIAL_NONE,      NOP, EV0, EV0, NO_EVENT_OFFSET},

        {"Anbernic RG35-2024", "rg35xx-2024", BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG35-H",    "rg35xx-h",    BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG35-PLUS", "rg35xx-plus", BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG35-PRO",  "rg35xx-pro",  BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG35-SP",   "rg35xx-sp",   BOARD_SPECIAL_NONE,      NOP, EV0, EV0, NO_EVENT_OFFSET},

        {"Anbernic RG40-H",    "rg40xx-h",    BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic RG40-V",    "rg40xx-v",    BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},

        {"Anbernic RGCUBE-H",  "rgcubexx-h",  BOARD_SPECIAL_NONE,      NOP, EV0, NOP, NO_EVENT_OFFSET},
        {"Anbernic Vita Pro",  "rg-vita-pro", BOARD_SPECIAL_VITA_PRO,  EV7, EV0, NOP, TOUCH_EVENT_OFFSET(EV7, 1)},

        {"Batlexp G350",       "rk-g350-v",   BOARD_SPECIAL_G350,      NOP, NOP, NOP, NO_EVENT_OFFSET},
        {"GKD Pixel 2",        "rk-pixel-2",  BOARD_SPECIAL_NONE,      NOP, NOP, NOP, NO_EVENT_OFFSET},

        {"TrimUI Brick",       "tui-brick",   BOARD_SPECIAL_TUI_BRICK, NOP, EV1, NOP, NO_EVENT_OFFSET},
        {"TrimUI Smart Pro",   "tui-spoon",   BOARD_SPECIAL_TUI_SPOON, EV0, EV1, NOP, NO_EVENT_OFFSET},
};

#define BOARD_TABLE_SIZE (sizeof(board_table) / sizeof(board_table[0]))

static const board_info_t *current_board = NULL;

static int board_event_offset_enabled(const board_event_offset_t *event_offset) {
    switch (event_offset->condition) {
        case BOARD_EVENT_OFFSET_TOUCH:
            return device.BOARD.HASTOUCH != 0;
        case BOARD_EVENT_OFFSET_NONE:
        default:
            return 0;
    }
}

static int board_adjust_event_index(int event_index) {
    if (event_index == NOP) return event_index;

    const board_event_offset_t *event_offset = &current_board->event_offset;

    if (!board_event_offset_enabled(event_offset)) return event_index;
    if (event_offset->from_event == NOP) return event_index;
    if (event_index < event_offset->from_event) return event_index;

    return event_index + event_offset->offset;
}

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
    return current_board ? board_adjust_event_index(current_board->vol_event) : NOP;
}

int board_power_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->pwr_event) : NOP;
}

int board_lid_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->lid_event) : NOP;
}
