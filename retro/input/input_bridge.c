#include <string.h>
#include "../../common/input.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"

#define MUX_RETRO_PORT_COUNT (1 + MUX_INPUT_MAX_EXTRA_PLAYERS)

static int suppress_until_released[mux_input_count];

static unsigned input_epoch = 0;
static unsigned input_polled_epoch = (unsigned) -1;

static uint16_t port_retropad_mask[MUX_RETRO_PORT_COUNT];
static int16_t port_stick_x[MUX_RETRO_PORT_COUNT][2];
static int16_t port_stick_y[MUX_RETRO_PORT_COUNT][2];

static int port_last_connected[MUX_RETRO_PORT_COUNT];

static int turbo_held_prev[MUX_RETRO_PORT_COUNT][16];
static uint32_t turbo_phase[MUX_RETRO_PORT_COUNT][16];

static const int turbo_hz_table[4] = {0, 10, 15, 20};

static uint16_t build_retropad_mask(const int port, const uint64_t mask, const int apply_suppress) {
    uint16_t out = 0;

    for (int id = 0; id < 16; id++) {
        const int mapped = session_settings.port_button_map[port][id];
        const mux_input_type mux_type = (mux_input_type) mapped;

        int raw_held = mapped < mux_input_count && (mask & BIT(mux_type)) != 0;

        if (apply_suppress && mapped < mux_input_count && suppress_until_released[mux_type]) {
            if (!raw_held) suppress_until_released[mux_type] = 0;
            raw_held = 0;
        }

        int held = raw_held;
        const int rate = session_settings.port_turbo_rate[port][id];

        if (rate > 0) {
            if (raw_held) {
                if (!turbo_held_prev[port][id]) {
                    turbo_phase[port][id] = 0; // immediate first press turbo go!
                } else {
                    turbo_phase[port][id]++;
                }

                const double fps = core_get_target_fps();
                int period = fps > 0.0 ? (int) (fps / turbo_hz_table[rate] + 0.5) : 6;
                if (period < 2) period = 2;

                held = (turbo_phase[port][id] % (uint32_t) period) < (uint32_t) (period / 2);
            } else {
                turbo_phase[port][id] = 0;
            }
        }

        turbo_held_prev[port][id] = raw_held;

        if (held) out |= (uint16_t) (1u << id);
    }

    return out;
}

static int16_t apply_analog_transform(const int16_t raw) {
    const double dz = (double) session_settings.analog_deadzone / 100.0;
    const double adz = (double) session_settings.analog_anti_deadzone / 100.0;
    const double sens = (double) session_settings.analog_sensitivity / 100.0;

    double v = (double) raw / 32767.0;
    if (v > 1.0) v = 1.0;
    if (v < -1.0) v = -1.0;

    const double mag = v < 0.0 ? -v : v;
    if (mag < dz) return 0;

    double scaled = dz >= 1.0 ? 0.0 : adz + (mag - dz) / (1.0 - dz) * (1.0 - adz);
    scaled *= sens;
    if (scaled > 1.0) scaled = 1.0;

    return (int16_t) ((v < 0.0 ? -scaled : scaled) * 32767.0);
}

static int16_t invert_y_if_needed(const int16_t y) {
    if (!session_settings.analog_invert_y) return y;
    return y == INT16_MIN ? INT16_MAX : (int16_t) -y;
}

static unsigned port_applied_device[MUX_RETRO_PORT_COUNT] = {[0 ... MUX_RETRO_PORT_COUNT - 1] = (unsigned) -1};

static void apply_controller_ports(void) {
    if (!current_core.retro_set_controller_port_device) return;

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        unsigned device = RETRO_DEVICE_NONE;

        if (port_last_connected[port]) {
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

static int resolved_source[MUX_RETRO_PORT_COUNT];

static void resolve_port_assignments(void) {
    int claimed[MUX_RETRO_PORT_COUNT] = {0}; // indexed by mux_input_source index, not port index!

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        resolved_source[port] = -1;
        if (session_settings.port_assignment[port] != port_assignment_remembered) continue;

        for (int s = 0; s < mux_input_source_count(); s++) {
            mux_input_source_info info;
            if (!mux_input_source_get(s, &info) || !info.connected || claimed[s]) continue;

            if (strcmp(info.stable_key, session_settings.port_device_key[port]) == 0) {
                resolved_source[port] = s;
                claimed[s] = 1;
                break;
            }
        }
    }

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        if (session_settings.port_assignment[port] != port_assignment_auto) continue;

        for (int s = 0; s < mux_input_source_count(); s++) {
            mux_input_source_info info;
            if (!mux_input_source_get(s, &info) || !info.connected || claimed[s]) continue;

            resolved_source[port] = s;
            claimed[s] = 1;
            break;
        }
    }
}

static void input_bridge_build_snapshot(void) {
    resolve_port_assignments();

    int ports_changed = 0;

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        const int source = resolved_source[port];
        const int connected = source >= 0;

        if (connected != port_last_connected[port]) ports_changed = 1;
        port_last_connected[port] = connected;

        if (!connected) {
            port_retropad_mask[port] = 0;
            port_stick_x[port][0] = port_stick_y[port][0] = 0;
            port_stick_x[port][1] = port_stick_y[port][1] = 0;
            for (int id = 0; id < 16; id++) {
                turbo_held_prev[port][id] = 0;
                turbo_phase[port][id] = 0;
            }
            continue;
        }

        port_retropad_mask[port] = build_retropad_mask(port, mux_input_source_pressed_mask(source), source == 0);

        int16_t x, y;
        mux_input_source_stick(source, RETRO_DEVICE_INDEX_ANALOG_LEFT, &x, &y);
        port_stick_x[port][RETRO_DEVICE_INDEX_ANALOG_LEFT] = apply_analog_transform(x);
        port_stick_y[port][RETRO_DEVICE_INDEX_ANALOG_LEFT] = invert_y_if_needed(apply_analog_transform(y));

        mux_input_source_stick(source, RETRO_DEVICE_INDEX_ANALOG_RIGHT, &x, &y);
        port_stick_x[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT] = apply_analog_transform(x);
        port_stick_y[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT] = invert_y_if_needed(apply_analog_transform(y));
    }

    if (ports_changed) apply_controller_ports();
}

void input_bridge_begin_run(void) {
    input_epoch++;
    input_polled_epoch = (unsigned) -1;

    mux_input_poll();
    input_bridge_build_snapshot();
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

int16_t mux_retro_input_state_cb(const unsigned port, const unsigned device, const unsigned index, const unsigned id) {
    if (port >= MUX_RETRO_PORT_COUNT) return 0;

    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t) port_retropad_mask[port];
        if (id >= 16) return 0;

        return port_retropad_mask[port] & (uint16_t) (1u << id) ? 1 : 0;
    }

    if (device == RETRO_DEVICE_ANALOG) {
        if (index != RETRO_DEVICE_INDEX_ANALOG_LEFT && index != RETRO_DEVICE_INDEX_ANALOG_RIGHT) return 0;
        if (id == RETRO_DEVICE_ID_ANALOG_X) return port_stick_x[port][index];
        if (id == RETRO_DEVICE_ID_ANALOG_Y) return port_stick_y[port][index];
        return 0;
    }

    return 0;
}
