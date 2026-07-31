#include <string.h>
#include <SDL2/SDL.h>
#include "../../common/device.h"
#include "../../common/fileio.h"
#include "rumble.h"
#include "../settings/settings.h"

static int rumble_on_value(void) {
    if (strncmp(device.board.name, "rg-vita", 7) == 0) return 0;
    return 1;
}

static int rumble_off_value(void) {
    if (strncmp(device.board.name, "rk", 2) == 0) return 1000000;
    if (strncmp(device.board.name, "rg-vita", 7) == 0) return 1;
    return 0;
}

static int rumble_last_on = -1;
static int rumble_suppressed = 0;
static uint16_t rumble_strength[MUX_INPUT_PORT_COUNT][2];
static const uint8_t test_pattern[] = {3, 1, 3, 3, 1, 1, 1, 1, 3, 3, 3, 1, 3, 1, 3, 3, 1, 1, 1, 1, 1};
static size_t test_pattern_index = 0;
static uint32_t test_deadline = 0;
static int test_active = 0;

static void rumble_write(const int want_on) {
    if (!device.board.rumble[0] || want_on == rumble_last_on) return;

    write_text_to_file(device.board.rumble, "w", INT, want_on ? rumble_on_value() : rumble_off_value());
    rumble_last_on = want_on;
}

static int rumble_requested(void) {
    for (int port = 0; port < MUX_INPUT_PORT_COUNT; port++)
        for (int effect = 0; effect < 2; effect++)
            if (rumble_strength[port][effect] > 0) return 1;

    return 0;
}

void rumble_bridge_refresh(void) {
    if (test_active) return;
    rumble_write(!rumble_suppressed && session_settings.rumble_enabled && rumble_requested());
}

bool rumble_bridge_test_start(void) {
    if (!device.board.rumble[0]) return false;

    test_active = 1;
    test_pattern_index = 0;
    rumble_write(1);
    test_deadline = SDL_GetTicks() + (uint32_t) test_pattern[0] * 120;
    return true;
}

void rumble_bridge_test_tick(void) {
    if (!test_active || !SDL_TICKS_PASSED(SDL_GetTicks(), test_deadline)) return;

    test_pattern_index++;
    if (test_pattern_index >= sizeof(test_pattern)) {
        test_active = 0;
        rumble_write(0);
        return;
    }

    rumble_write((test_pattern_index & 1u) == 0);
    test_deadline += (uint32_t) test_pattern[test_pattern_index] * 120;
}

void rumble_bridge_test_cancel(void) {
    if (!test_active) return;

    test_active = 0;
    rumble_bridge_refresh();
}

void rumble_bridge_set_suppressed(const int suppressed) {
    rumble_suppressed = !!suppressed;
    rumble_bridge_refresh();
}

static bool rumble_set_state(const unsigned port, const enum retro_rumble_effect effect, const uint16_t strength) {
    if (port >= MUX_INPUT_PORT_COUNT || (effect != RETRO_RUMBLE_STRONG && effect != RETRO_RUMBLE_WEAK)) return false;

    rumble_strength[port][effect] = strength;
    rumble_bridge_refresh();
    return true;
}

static struct retro_rumble_interface rumble_iface = {
    .set_rumble_state = rumble_set_state,
};

bool rumble_bridge_get_interface(struct retro_rumble_interface *iface) {
    if (!iface || !device.board.rumble[0]) return false;
    *iface = rumble_iface;
    return true;
}

void rumble_bridge_shutdown(void) {
    test_active = 0;
    memset(rumble_strength, 0, sizeof(rumble_strength));
    rumble_suppressed = 0;
    rumble_write(0);
}
