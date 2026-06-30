#include <string.h>
#include "board.h"
#include "device.h"
#include "log.h"

#define NO_EVENT_OFFSET {board_event_offset_none, nop, 0}

#define TOUCH_EVENT_OFFSET(FROM, ADD) {board_event_offset_touch, FROM, ADD}

static const board_info_t board_table[] = {
    {"Generic H36S", "gcs-h36s", board_special_none, nop, nop, nop, NO_EVENT_OFFSET, regular},
    {"MagicX Zero28", "mgx-zero28", board_special_none, nop, nop, nop, NO_EVENT_OFFSET, regular},

    {"Anbernic RG28-H", "rg28xx-h", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG34-H", "rg34xx-h", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG34-SP", "rg34xx-sp", board_special_none, nop, ev0, ev0, NO_EVENT_OFFSET, regular},

    {"Anbernic RG35-2024", "rg35xx-2024", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-H", "rg35xx-h", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-PLUS", "rg35xx-plus", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-PRO", "rg35xx-pro", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-SP", "rg35xx-sp", board_special_none, nop, ev0, ev0, NO_EVENT_OFFSET, regular},

    {"Anbernic RG40-H", "rg40xx-h", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG40-V", "rg40xx-v", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},

    {"Anbernic RGCUBE-H", "rgcubexx-h", board_special_none, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic Vita Pro", "rg-vita-pro", board_special_vita_pro, ev7, ev0, nop, TOUCH_EVENT_OFFSET(ev7, 1), regular},

    {"Batlexp G350", "rk-g350-v", board_special_g350, ev3, ev0, nop, NO_EVENT_OFFSET, regular},
    {"GKD Pixel 2", "rk-pixel-2", board_special_none, nop, nop, nop, NO_EVENT_OFFSET, regular},

    {"TrimUI Brick", "tui-brick", board_special_tui_brick, nop, ev1, nop, NO_EVENT_OFFSET, goofy},
    {"TrimUI Smart Pro", "tui-spoon", board_special_tui_spoon, ev0, ev1, nop, NO_EVENT_OFFSET, goofy},
};

#define BOARD_TABLE_SIZE (sizeof(board_table) / sizeof(board_table[0]))

static const board_info_t *current_board = NULL;

static int board_event_offset_enabled(const board_event_offset_t *event_offset) {
    switch (event_offset->condition) {
        case board_event_offset_touch:
            return device.board.has_touch != 0;
        case board_event_offset_none:
        default:
            return 0;
    }
}

static int board_adjust_event_index(const int event_index) {
    if (event_index == nop) return event_index;

    const board_event_offset_t *event_offset = &current_board->event_offset;

    if (!board_event_offset_enabled(event_offset)) return event_index;
    if (event_offset->from_event == nop) return event_index;
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

    LOG_INFO(
        "board", "Detected Board: %s (%s) (Special: %d)", current_board->name, current_board->code,
        current_board->special
    );
}

const board_info_t *board_current(void) {
    return current_board;
}

const char *board_name(void) {
    return current_board ? current_board->name : "Unknown";
}

board_special_t board_special(void) {
    return current_board ? current_board->special : board_special_none;
}

int board_is(const board_special_t type) {
    return board_special() == type;
}

int board_is_special(void) {
    return board_special() != board_special_none;
}

int board_layout_map_swap(void) {
    return current_board ? current_board->layout_map_swap : 0;
}

int board_volume_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->vol_event) : nop;
}

int board_power_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->pwr_event) : nop;
}

int board_lid_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->lid_event) : nop;
}
