#include <string.h>
#include <SDL2/SDL.h>
#include <common/platform/input.h>
#include "rumble.h"
#include "../settings/settings.h"

static int rumble_suppressed = 0;
static uint16_t rumble_strength[MUX_INPUT_PORT_COUNT][2];
static uint16_t rumble_applied[MUX_INPUT_PORT_COUNT][2];
static int rumble_applied_source[MUX_INPUT_PORT_COUNT] = {-1, -1, -1, -1};
static uint32_t rumble_refresh_deadline = 0;
static const uint8_t test_pattern[] = {3, 1, 3, 3, 1, 1, 1, 1, 3, 3, 3, 1, 3, 1, 3, 3, 1, 1, 1, 1, 1};
static size_t test_pattern_index = 0;
static uint32_t test_deadline = 0;
static int test_active = 0;
static int test_source = -1;

static int rumble_requested(void) {
    for (int port = 0; port < MUX_INPUT_PORT_COUNT; port++)
        for (int effect = 0; effect < 2; effect++)
            if (rumble_strength[port][effect] > 0) return 1;

    return 0;
}

static int rumble_apply(const int force) {
    int resolved[MUX_INPUT_PORT_COUNT];
    session_settings_resolve_port_sources(resolved);

    int applied = 0;
    const int enabled = !rumble_suppressed && session_settings.rumble_enabled;

    for (int port = 0; port < MUX_INPUT_PORT_COUNT; port++) {
        const int source = resolved[port];
        const uint16_t strong = enabled ? rumble_strength[port][RETRO_RUMBLE_STRONG] : 0;
        const uint16_t weak = enabled ? rumble_strength[port][RETRO_RUMBLE_WEAK] : 0;

        if (rumble_applied_source[port] >= 0 && rumble_applied_source[port] != source)
            mux_input_source_rumble(rumble_applied_source[port], 0, 0);

        if (source >= 0
            && (force || rumble_applied_source[port] != source || rumble_applied[port][RETRO_RUMBLE_STRONG] != strong
                || rumble_applied[port][RETRO_RUMBLE_WEAK] != weak))
            applied |= mux_input_source_rumble(source, strong, weak);

        rumble_applied_source[port] = source;
        rumble_applied[port][RETRO_RUMBLE_STRONG] = strong;
        rumble_applied[port][RETRO_RUMBLE_WEAK] = weak;
    }

    rumble_refresh_deadline = SDL_GetTicks() + 500;
    return applied;
}

void rumble_bridge_refresh(void) {
    if (!test_active) rumble_apply(0);
}

void rumble_bridge_tick(const uint32_t now) {
    if (!test_active && !rumble_suppressed && session_settings.rumble_enabled && rumble_requested()
        && SDL_TICKS_PASSED(now, rumble_refresh_deadline))
        rumble_apply(1);
}

bool rumble_bridge_test_start(void) {
    test_source = session_settings_resolve_port_source(0);
    if (test_source < 0 || !mux_input_source_rumble(test_source, UINT16_MAX, UINT16_MAX)) {
        test_source = -1;
        return false;
    }

    test_active = 1;
    test_pattern_index = 0;
    test_deadline = SDL_GetTicks() + (uint32_t) test_pattern[0] * 120;
    return true;
}

void rumble_bridge_test_tick(void) {
    if (!test_active || !SDL_TICKS_PASSED(SDL_GetTicks(), test_deadline)) return;

    test_pattern_index++;
    if (test_pattern_index >= sizeof(test_pattern)) {
        test_active = 0;
        mux_input_source_rumble(test_source, 0, 0);
        test_source = -1;
        rumble_apply(1);
        return;
    }

    const uint16_t strength = (test_pattern_index & 1u) == 0 ? UINT16_MAX : 0;
    mux_input_source_rumble(test_source, strength, strength);
    test_deadline += (uint32_t) test_pattern[test_pattern_index] * 120;
}

void rumble_bridge_test_cancel(void) {
    if (!test_active) return;

    test_active = 0;
    mux_input_source_rumble(test_source, 0, 0);
    test_source = -1;
    rumble_apply(1);
}

void rumble_bridge_set_suppressed(const int suppressed) {
    rumble_suppressed = !!suppressed;
    rumble_bridge_refresh();
}

static bool rumble_set_state(const unsigned port, const enum retro_rumble_effect effect, const uint16_t strength) {
    if (port >= MUX_INPUT_PORT_COUNT || (effect != RETRO_RUMBLE_STRONG && effect != RETRO_RUMBLE_WEAK)) return false;

    if (rumble_strength[port][effect] == strength) return true;
    rumble_strength[port][effect] = strength;
    rumble_bridge_refresh();
    return true;
}

static struct retro_rumble_interface rumble_iface = {
    .set_rumble_state = rumble_set_state,
};

bool rumble_bridge_get_interface(struct retro_rumble_interface *iface) {
    if (!iface) return false;
    *iface = rumble_iface;
    return true;
}

void rumble_bridge_shutdown(void) {
    if (test_source >= 0) mux_input_source_rumble(test_source, 0, 0);
    test_source = -1;
    test_active = 0;
    memset(rumble_strength, 0, sizeof(rumble_strength));
    rumble_suppressed = 0;
    rumble_apply(1);
    memset(rumble_applied, 0, sizeof(rumble_applied));
    for (int port = 0; port < MUX_INPUT_PORT_COUNT; port++)
        rumble_applied_source[port] = -1;
}
