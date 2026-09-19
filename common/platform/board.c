#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <common/platform/board.h>
#include <common/platform/device.h>
#include <common/runtime/log.h>

#define NO_EVENT_OFFSET {board_event_offset_none, nop, 0}

#define TOUCH_EVENT_OFFSET(FROM, ADD) {board_event_offset_touch, FROM, ADD}

static const board_info_t board_table[] = {
    {"Generic H36S", "gcs-h36s", board_special_none, nop, nop, nop, NO_EVENT_OFFSET, regular},
    {"MagicX Zero28", "mgx-zero28", board_special_none, nop, nop, nop, NO_EVENT_OFFSET, regular},

    {"Anbernic RG28-H", "rg28xx-h", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG34-H", "rg34xx-h", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG34-SP", "rg34xx-sp", board_special_h700, nop, ev0, ev0, NO_EVENT_OFFSET, regular},

    {"Anbernic RG35-2024", "rg35xx-2024", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-H", "rg35xx-h", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-PLUS", "rg35xx-plus", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-PRO", "rg35xx-pro", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG35-SP", "rg35xx-sp", board_special_h700, nop, ev0, ev0, NO_EVENT_OFFSET, regular},

    {"Anbernic RG40-H", "rg40xx-h", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RG40-V", "rg40xx-v", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},

    {"Anbernic RGCUBE-H", "rgcubexx-h", board_special_h700, nop, ev0, nop, NO_EVENT_OFFSET, regular},
    {"Anbernic RGSP", "rgsp", board_special_h700, nop, ev0, ev0, NO_EVENT_OFFSET, regular},
    {"Anbernic Vita Pro", "rg-vita-pro", board_special_vita_pro, ev7, ev0, nop, TOUCH_EVENT_OFFSET(ev7, 1), regular},

    {"Batlexp G350", "rk-g350-v", board_special_g350, ev3, ev0, nop, NO_EVENT_OFFSET, regular},
    {"GKD Pixel 2", "rk-pixel-2", board_special_none, ev1, nop, nop, NO_EVENT_OFFSET, regular},

    {"TrimUI Brick", "tui-brick", board_special_tui_brick, nop, ev1, nop, NO_EVENT_OFFSET, goofy},
    {"TrimUI Brick Pro", "tui-brick-pro", board_special_tui_brick_pro, nop, ev1, nop, NO_EVENT_OFFSET, goofy},
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

#define VOLUME_KEY_DEVICE_NAME "gpio-keys"
#define VOLUME_KEY_EVENT_SCAN  32

static int volume_event_probed = 0;
static int volume_event_cached = nop;

static int board_key_bit_set(const unsigned long *bits, const int bit) {
    const int word = bit / (int) (8 * sizeof(unsigned long));
    const int off = bit % (int) (8 * sizeof(unsigned long));

    return (bits[word] >> off) & 1UL;
}

/*
 * The volume keys do not live on a stable event node. On the GKD Pixel 2 the
 * gpio-keys and joypad drivers probe about 20 ms apart, so event0 and event1
 * swap between boots and a hardcoded index is right only some of the time.
 *
 * Find the node by name instead, and confirm it really carries both volume
 * keys before trusting it. Anything unexpected falls back to the board table.
 */
static int board_probe_volume_event_index(void) {
    for (int idx = 0; idx < VOLUME_KEY_EVENT_SCAN; idx++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", idx);

        const int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char name[128] = {0};
        unsigned long keys[(KEY_MAX / (8 * sizeof(unsigned long))) + 1] = {0};
        int match = 0;

        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
            strcmp(name, VOLUME_KEY_DEVICE_NAME) == 0 &&
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 &&
            board_key_bit_set(keys, KEY_VOLUMEUP) &&
            board_key_bit_set(keys, KEY_VOLUMEDOWN)) {
            match = 1;
        }

        close(fd);

        if (match) return idx;
    }

    return nop;
}

int board_volume_event_index(void) {
    const int table_index = current_board ? board_adjust_event_index(current_board->vol_event) : nop;

    /* Boards with no raw volume device stay opted out - do not probe for one. */
    if (table_index == nop) return nop;

    if (!volume_event_probed) {
        volume_event_probed = 1;
        volume_event_cached = board_probe_volume_event_index();

        if (volume_event_cached != nop && volume_event_cached != table_index) {
            LOG_INFO("board", "Volume keys found on event%d, board table said event%d",
                     volume_event_cached, table_index);
        } else if (volume_event_cached == nop) {
            LOG_WARN("board", "No '%s' device with volume keys, using board table event%d",
                     VOLUME_KEY_DEVICE_NAME, table_index);
        }
    }

    return volume_event_cached != nop ? volume_event_cached : table_index;
}

int board_power_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->pwr_event) : nop;
}

int board_lid_event_index(void) {
    return current_board ? board_adjust_event_index(current_board->lid_event) : nop;
}
