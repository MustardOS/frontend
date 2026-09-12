#include <string.h>
#include <common/platform/input.h>
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../core/perf.h"
#include "../macro/runtime.h"
#include "../link/link.h"
#include "../netplay/netplay.h"
#include "../settings/settings.h"
#include "../video/hw_render.h"

#define MUX_RETRO_PORT_COUNT (1 + MUX_INPUT_MAX_EXTRA_PLAYERS)

static int suppress_until_released[mux_input_count];

static unsigned input_epoch = 0;
static unsigned input_polled_epoch = (unsigned) -1;

static uint16_t port_retropad_mask[MUX_RETRO_PORT_COUNT];
static int16_t port_stick_x[MUX_RETRO_PORT_COUNT][2];
static int16_t port_stick_y[MUX_RETRO_PORT_COUNT][2];

static int port_source_connected[MUX_RETRO_PORT_COUNT];
static int port_is_deck[MUX_RETRO_PORT_COUNT];
static int port_deck_route[MUX_RETRO_PORT_COUNT];
static unsigned netplay_player_count;
static int netplay_routes_input;
static netplay_pad_state netplay_local_input;

static int turbo_held_prev[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static uint32_t turbo_phase[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];

static int bound_stick_x[MUX_RETRO_PORT_COUNT][2];
static int bound_stick_y[MUX_RETRO_PORT_COUNT][2];
static int bound_stick_active[MUX_RETRO_PORT_COUNT][2];

typedef struct {
    int source;
    mux_input_type type;
    int target;
    int macro;
    int stick;
    int axis_x;
    int axis_y;
    uint32_t turbo_period;
    int turbo_ms;
} input_route_t;

typedef struct {
    int target[PORT_SOURCE_COUNT];
    int turbo[PORT_SOURCE_COUNT];
    int macro[PORT_SOURCE_COUNT];
} input_route_key_t;

static input_route_t input_routes[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int input_route_count[MUX_RETRO_PORT_COUNT];
static int input_route_stick_bound[MUX_RETRO_PORT_COUNT][2];
static input_route_key_t input_route_keys[MUX_RETRO_PORT_COUNT];
static int input_route_valid[MUX_RETRO_PORT_COUNT];
static double input_route_fps[MUX_RETRO_PORT_COUNT];

static int stick_transform_key[3] = {-1, -1, -1};
static double stick_deadzone_raw;
static double stick_transform_slope;
static double stick_transform_intercept;

static int16_t clamp_axis(const int value) {
    if (value > PORT_STICK_FULL) return PORT_STICK_FULL;
    if (value < -PORT_STICK_FULL) return -PORT_STICK_FULL;
    return (int16_t) value;
}

static int16_t merge_axis(const int16_t player, const int16_t deck, const int priority) {
    if (priority && deck != 0) return deck;

    const int player_magnitude = player < 0 ? -player : player;
    const int deck_magnitude = deck < 0 ? -deck : deck;

    return deck_magnitude > player_magnitude ? deck : player;
}

#define RETROPAD_UP    ((uint16_t) (1u << RETRO_DEVICE_ID_JOYPAD_UP))
#define RETROPAD_DOWN  ((uint16_t) (1u << RETRO_DEVICE_ID_JOYPAD_DOWN))
#define RETROPAD_LEFT  ((uint16_t) (1u << RETRO_DEVICE_ID_JOYPAD_LEFT))
#define RETROPAD_RIGHT ((uint16_t) (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT))

static uint16_t drop_opposite(uint16_t player, const uint16_t deck, const uint16_t one, const uint16_t other) {
    if (deck & one) player &= (uint16_t) ~other;
    if (deck & other) player &= (uint16_t) ~one;

    return player;
}

static uint16_t merge_buttons(uint16_t player, const uint16_t deck, const int priority) {
    if (priority) {
        player = drop_opposite(player, deck, RETROPAD_UP, RETROPAD_DOWN);
        player = drop_opposite(player, deck, RETROPAD_LEFT, RETROPAD_RIGHT);
    }

    return (uint16_t) (player | deck);
}

static uint32_t ms_to_frames(const int ms, const double fps) {
    return (uint32_t) (fps > 0.0 ? (double) ms / 1000.0 * fps + 0.5 : 6);
}

static void refresh_route_periods(const int port, const double fps) {
    if (input_route_fps[port] == fps) return;
    input_route_fps[port] = fps;

    for (int index = 0; index < input_route_count[port]; index++) {
        input_route_t *route = &input_routes[port][index];
        route->turbo_period = route->turbo_ms > 0 ? ms_to_frames(route->turbo_ms, fps) : 0;
        if (route->turbo_period > 0 && route->turbo_period < 2) route->turbo_period = 2;
    }
}

static void compile_input_routes(const int port, const double fps) {
    const int *target = session_settings_source_target(port);
    const int *turbo = session_settings_source_turbo(port);
    const int *macro = session_settings_source_macro(port);
    input_route_key_t *key = &input_route_keys[port];

    if (input_route_valid[port] && memcmp(key->target, target, sizeof(key->target)) == 0
        && memcmp(key->turbo, turbo, sizeof(key->turbo)) == 0
        && memcmp(key->macro, macro, sizeof(key->macro)) == 0) {
        refresh_route_periods(port, fps);
        return;
    }

    memcpy(key->target, target, sizeof(key->target));
    memcpy(key->turbo, turbo, sizeof(key->turbo));
    memcpy(key->macro, macro, sizeof(key->macro));
    input_route_count[port] = 0;
    input_route_stick_bound[port][0] = 0;
    input_route_stick_bound[port][1] = 0;

    for (int source = 0; source < PORT_SOURCE_COUNT; source++) {
        if (macro[source] < 0 && (target[source] < 0 || target[source] >= PORT_TARGET_COUNT)) continue;

        input_route_t *route = &input_routes[port][input_route_count[port]++];
        *route = (input_route_t) {
            .source = source,
            .type = (mux_input_type) session_settings_source_types[source],
            .target = target[source],
            .macro = macro[source],
            .stick = -1,
            .turbo_ms = turbo[source],
        };
        if (route->target >= PORT_DIGITAL_COUNT)
            session_settings_target_stick(route->target, &route->stick, &route->axis_x, &route->axis_y);
    }

    for (int stick = 0; stick < 2; stick++) {
        const int first = PORT_DIGITAL_COUNT + stick * 4;
        for (int source = first; source < first + 4; source++) {
            if (target[source] >= 0 || macro[source] >= 0) {
                input_route_stick_bound[port][stick] = 1;
                break;
            }
        }
    }

    input_route_valid[port] = 1;
    input_route_fps[port] = -1.0;
    refresh_route_periods(port, fps);
}

static int resolve_raw_held(const mux_input_type mux_type, const uint64_t mask, const int apply_suppress) {
    int raw_held = (mask & BIT(mux_type)) != 0;

    if (apply_suppress && suppress_until_released[mux_type]) {
        if (!raw_held) suppress_until_released[mux_type] = 0;
        raw_held = 0;
    }

    return raw_held;
}

static uint16_t build_retropad_mask(const int port, const uint64_t mask, const int apply_suppress) {
    uint16_t out = 0;
    const double fps = core_get_target_fps();

    macro_runtime_begin_port(port);
    compile_input_routes(port, fps);

    bound_stick_x[port][0] = bound_stick_y[port][0] = 0;
    bound_stick_x[port][1] = bound_stick_y[port][1] = 0;
    bound_stick_active[port][0] = 0;
    bound_stick_active[port][1] = 0;

    for (int index = 0; index < input_route_count[port]; index++) {
        const input_route_t *route = &input_routes[port][index];
        const int source = route->source;

        if (route->macro >= 0) {
            out |=
                macro_runtime_drive(port, source, route->macro, resolve_raw_held(route->type, mask, apply_suppress), mask, fps);
            continue;
        }

        const int raw_held = resolve_raw_held(route->type, mask, apply_suppress);

        int held = raw_held;
        if (route->turbo_period > 0) {
            if (raw_held) {
                if (!turbo_held_prev[port][source]) {
                    turbo_phase[port][source] = 0;
                } else {
                    turbo_phase[port][source]++;
                }

                held = turbo_phase[port][source] % route->turbo_period < route->turbo_period / 2;
            } else {
                turbo_phase[port][source] = 0;
            }
        }

        turbo_held_prev[port][source] = raw_held;

        if (!held) continue;

        if (route->target < PORT_DIGITAL_COUNT) {
            out |= (uint16_t) (1u << route->target);
        } else if (route->stick >= 0 && route->stick < 2) {
            bound_stick_x[port][route->stick] += route->axis_x;
            bound_stick_y[port][route->stick] += route->axis_y;
            bound_stick_active[port][route->stick] = 1;
        }
    }

    return out;
}

static int16_t apply_stick_transform(const int16_t raw) {
    if (stick_transform_key[0] != session_settings.stick_deadzone
        || stick_transform_key[1] != session_settings.stick_anti_deadzone
        || stick_transform_key[2] != session_settings.stick_sensitivity) {
        stick_transform_key[0] = session_settings.stick_deadzone;
        stick_transform_key[1] = session_settings.stick_anti_deadzone;
        stick_transform_key[2] = session_settings.stick_sensitivity;

        const double dz = (double) session_settings.stick_deadzone / 100.0;
        const double adz = (double) session_settings.stick_anti_deadzone / 100.0;
        const double sens = (double) session_settings.stick_sensitivity / 100.0;
        stick_deadzone_raw = dz * 32767.0;
        stick_transform_slope = dz >= 1.0 ? 0.0 : (1.0 - adz) * sens / ((1.0 - dz) * 32767.0);
        stick_transform_intercept = dz >= 1.0 ? 0.0 : (adz - dz * (1.0 - adz) / (1.0 - dz)) * sens;
    }

    int magnitude = raw < 0 ? -(int) raw : (int) raw;
    if (magnitude > PORT_STICK_FULL) magnitude = PORT_STICK_FULL;
    if ((double) magnitude < stick_deadzone_raw) return 0;

    double scaled = (double) magnitude * stick_transform_slope + stick_transform_intercept;
    if (scaled > 1.0) scaled = 1.0;

    return (int16_t) ((raw < 0 ? -scaled : scaled) * 32767.0);
}

static int16_t invert_y_if_needed(const int16_t y) {
    if (!session_settings.stick_invert_y) return y;
    return y == INT16_MIN ? INT16_MAX : (int16_t) -y;
}

static unsigned port_applied_device[MUX_RETRO_PORT_COUNT];
static int port_applied_ready;

static void apply_controller_ports(void) {
    if (!current_core.retro_set_controller_port_device) return;

    // Every port starts unknown so the first pass always tells the core, which a zero would not
    if (!port_applied_ready) {
        for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++)
            port_applied_device[port] = (unsigned) -1;
        port_applied_ready = 1;
    }

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        unsigned device = RETRO_DEVICE_NONE;

        const int connected =
            netplay_player_count ? (unsigned) port < netplay_player_count : port_source_connected[port];

        if (connected) {
            device = session_settings.port_device_id[port] ? (unsigned) session_settings.port_device_id[port]
                                                           : RETRO_DEVICE_JOYPAD;
        }

        if (device == port_applied_device[port]) continue;

        current_core.retro_set_controller_port_device((unsigned) port, device);
        port_applied_device[port] = device;
    }
}

void input_bridge_apply_controller_ports(void) {
    apply_controller_ports();
}

void input_bridge_set_netplay_state(unsigned player_count, const int routes_input) {
    if (player_count > MUX_RETRO_PORT_COUNT) player_count = MUX_RETRO_PORT_COUNT;
    const int count_changed = netplay_player_count != player_count;
    const int routed = player_count && routes_input;
    if (!count_changed && netplay_routes_input == routed) return;

    netplay_player_count = player_count;
    netplay_routes_input = routed;
    if (!count_changed) return;

    hw_render_bridge_enter_core_call();
    apply_controller_ports();
    hw_render_bridge_exit_core_call();
}

static int resolved_source[MUX_RETRO_PORT_COUNT];

static uint32_t resolve_cached_generation = (uint32_t) -1;
static int resolve_cached_assignment[MUX_RETRO_PORT_COUNT];
static char resolve_cached_keys[MUX_RETRO_PORT_COUNT][64];

static void resolve_port_assignments_cached(void) {
    const uint32_t generation = mux_input_source_generation();

    if (generation == resolve_cached_generation
        && memcmp(resolve_cached_assignment, session_settings.port_assignment, sizeof(resolve_cached_assignment)) == 0
        && memcmp(resolve_cached_keys, session_settings.port_device_key, sizeof(resolve_cached_keys)) == 0)
        return;

    session_settings_resolve_port_sources(resolved_source);

    resolve_cached_generation = generation;
    memcpy(resolve_cached_assignment, session_settings.port_assignment, sizeof(resolve_cached_assignment));
    memcpy(resolve_cached_keys, session_settings.port_device_key, sizeof(resolve_cached_keys));
}

static void input_bridge_build_snapshot(void) {
    const int track_latency = perf_is_enabled();
    const int frontend_modifier_held = mux_input_pressed(mux_input_menu);
    const uint64_t previous_signature = track_latency ? input_bridge_snapshot_signature() : 0;

    resolve_port_assignments_cached();

    int ports_changed = 0;
    int fed_by_deck[MUX_RETRO_PORT_COUNT] = {0};

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        port_is_deck[port] = resolved_source[port] >= 0 && session_settings_port_is_deck(port);
        port_deck_route[port] = port_is_deck[port] ? session_settings_port_ketchup_route(port) : -1;
        if (port_deck_route[port] >= 0) fed_by_deck[port_deck_route[port]] = 1;
    }

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        const int source = resolved_source[port];
        const int has_source = source >= 0;
        const int connected = (has_source && !port_is_deck[port]) || fed_by_deck[port];

        if (connected != port_source_connected[port]) ports_changed = 1;
        port_source_connected[port] = connected;

        if (!has_source || frontend_modifier_held) {
            port_retropad_mask[port] = 0;
            port_stick_x[port][0] = port_stick_y[port][0] = 0;
            port_stick_x[port][1] = port_stick_y[port][1] = 0;
            macro_runtime_reset_port(port);
            bound_stick_active[port][0] = 0;
            bound_stick_active[port][1] = 0;

            for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
                turbo_held_prev[port][s] = 0;
                turbo_phase[port][s] = 0;
            }
            continue;
        }

        port_retropad_mask[port] = build_retropad_mask(port, mux_input_source_pressed_mask(source), source == 0);

        for (int s = 0; s < 2; s++) {
            // A playing stick macro owns that stick outright and ignores any axis transform
            if (macro_runtime_stick(port, s, &port_stick_x[port][s], &port_stick_y[port][s])) continue;

            // A button bound to a axis direction takes the stick over only while it is actually held
            if (bound_stick_active[port][s]) {
                port_stick_x[port][s] = clamp_axis(bound_stick_x[port][s]);
                port_stick_y[port][s] = clamp_axis(bound_stick_y[port][s]);
                continue;
            }

            if (input_route_stick_bound[port][s]) {
                port_stick_x[port][s] = 0;
                port_stick_y[port][s] = 0;
                continue;
            }

            int16_t x, y;
            mux_input_source_stick(source, s, &x, &y);
            port_stick_x[port][s] = apply_stick_transform(x);
            port_stick_y[port][s] = invert_y_if_needed(apply_stick_transform(y));
        }
    }

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        if (!port_is_deck[port]) continue;

        const int route = port_deck_route[port];
        if (route >= 0) {
            const int priority = session_settings_port_deck_priority(port);

            port_retropad_mask[route] = merge_buttons(port_retropad_mask[route], port_retropad_mask[port], priority);

            for (int stick = 0; stick < 2; stick++) {
                port_stick_x[route][stick] =
                    merge_axis(port_stick_x[route][stick], port_stick_x[port][stick], priority);
                port_stick_y[route][stick] =
                    merge_axis(port_stick_y[route][stick], port_stick_y[port][stick], priority);
            }
        }

        port_retropad_mask[port] = 0;

        port_stick_x[port][0] = port_stick_y[port][0] = 0;
        port_stick_x[port][1] = port_stick_y[port][1] = 0;
    }

    memset(&netplay_local_input, 0, sizeof(netplay_local_input));
    for (unsigned port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        if (!port_source_connected[port]) continue;

        netplay_local_input.buttons = port_retropad_mask[port];
        netplay_local_input.axes[0] = port_stick_x[port][0];
        netplay_local_input.axes[1] = port_stick_y[port][0];
        netplay_local_input.axes[2] = port_stick_x[port][1];
        netplay_local_input.axes[3] = port_stick_y[port][1];
        netplay_local_input.connected = 1;
        break;
    }

    if (netplay_routes_input) {
        memset(port_retropad_mask, 0, sizeof(port_retropad_mask));
        memset(port_stick_x, 0, sizeof(port_stick_x));
        memset(port_stick_y, 0, sizeof(port_stick_y));
    }

    if (ports_changed) apply_controller_ports();
    if (track_latency && input_bridge_snapshot_signature() != previous_signature) perf_note_input_change();
}

void input_bridge_begin_run(void) {
    input_epoch++;

    perf_note_poll();
    mux_input_poll();
    input_bridge_build_snapshot();
    input_polled_epoch = input_epoch;
}

void mux_retro_input_poll_cb(void) {
    if (input_polled_epoch == input_epoch) return;

    mux_input_poll();
    input_bridge_build_snapshot();
    input_polled_epoch = input_epoch;
}

uint64_t input_bridge_snapshot_signature(void) {
    uint64_t sig = 1469598103934665603ull;

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        sig = (sig ^ port_retropad_mask[port]) * 1099511628211ull;
        for (int index = 0; index < 2; index++) {
            sig = (sig ^ (uint16_t) port_stick_x[port][index]) * 1099511628211ull;
            sig = (sig ^ (uint16_t) port_stick_y[port][index]) * 1099511628211ull;
        }
    }

    return sig;
}

void input_bridge_suppress_held(void) {
    for (int i = 0; i < mux_input_count; i++) {
        suppress_until_released[i] = mux_input_pressed((mux_input_type) i) ? 1 : 0;
    }
}

void input_bridge_suppress(const mux_input_type type) {
    suppress_until_released[type] = 1;
}

int16_t mux_retro_input_state_cb(unsigned port, const unsigned device, const unsigned index, const unsigned id) {
    if (port >= MUX_RETRO_PORT_COUNT) return 0;
    if (pause_menu_is_active()) return 0;

    if (link_local_active()) {
        if (port != (unsigned) link_get_focus()) return 0;
        port = 0;
    }

    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t) port_retropad_mask[port];
        if (id >= 16) return 0;

        return port_retropad_mask[port] & (uint16_t) (1u << id) ? 1 : 0;
    }

    if (device == RETRO_DEVICE_ANALOG) {
        if (index != RETRO_DEVICE_INDEX_ANALOG_LEFT && index != RETRO_DEVICE_INDEX_ANALOG_RIGHT) return 0;
        if (session_settings_stick_forced(port, index)) return 0;

        if (id == RETRO_DEVICE_ID_ANALOG_X) return port_stick_x[port][index];
        if (id == RETRO_DEVICE_ID_ANALOG_Y) return port_stick_y[port][index];

        return 0;
    }

    return 0;
}

void netplay_input_get_local(netplay_pad_state *state) {
    if (!state) return;
    *state = netplay_local_input;
}

void netplay_input_set_port(const unsigned port, const netplay_pad_state *state) {
    if (!state || port >= MUX_RETRO_PORT_COUNT) return;

    port_retropad_mask[port] = state->buttons;
    port_stick_x[port][0] = state->axes[0];
    port_stick_y[port][0] = state->axes[1];
    port_stick_x[port][1] = state->axes[2];
    port_stick_y[port][1] = state->axes[3];
}
