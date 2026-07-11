#include "../../common/input.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"

#define MUX_RETRO_PORT_COUNT (1 + MUX_INPUT_MAX_EXTRA_PLAYERS)

static int suppress_until_released[mux_input_count];

static uint64_t input_snapshot_mask = 0;
static unsigned input_epoch = 0;
static unsigned input_polled_epoch = (unsigned) -1;

static uint16_t port_retropad_mask[MUX_RETRO_PORT_COUNT];
static int16_t port_stick_x[MUX_RETRO_PORT_COUNT][2];
static int16_t port_stick_y[MUX_RETRO_PORT_COUNT][2];

static unsigned port_device_id[MUX_RETRO_PORT_COUNT];
static int port_last_connected[MUX_RETRO_PORT_COUNT];

static const mux_input_type joypad_id_to_mux[16] = {
    mux_input_b,       mux_input_y,         mux_input_select,    mux_input_start,
    mux_input_dpad_up, mux_input_dpad_down, mux_input_dpad_left, mux_input_dpad_right,
    mux_input_a,       mux_input_x,         mux_input_l1,        mux_input_r1,
    mux_input_l2,      mux_input_r2,        mux_input_l3,        mux_input_r3,
};

static uint16_t build_retropad_mask(const uint64_t mask, const int apply_suppress) {
    uint16_t out = 0;

    for (int id = 0; id < 16; id++) {
        const mux_input_type mux_type = joypad_id_to_mux[id];
        int held = (mask & BIT(mux_type)) != 0;

        if (apply_suppress && suppress_until_released[mux_type]) {
            if (!held) suppress_until_released[mux_type] = 0;
            held = 0;
        }

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

static void apply_controller_ports(void) {
    if (!current_core.retro_set_controller_port_device) return;

    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++) {
        const unsigned device = port_last_connected[port] ? port_device_id[port] : RETRO_DEVICE_NONE;
        current_core.retro_set_controller_port_device((unsigned) port, device);
    }
}

void input_bridge_set_controller_info(const struct retro_controller_info *info) {
    for (int port = 0; port < MUX_RETRO_PORT_COUNT; port++)
        port_device_id[port] = RETRO_DEVICE_JOYPAD;

    if (!info) return;

    for (int port = 0; port < MUX_RETRO_PORT_COUNT && info[port].types; port++) {
        for (unsigned t = 0; t < info[port].num_types; t++) {
            if ((info[port].types[t].id & RETRO_DEVICE_MASK) == RETRO_DEVICE_ANALOG) {
                port_device_id[port] = info[port].types[t].id;
                break;
            }
        }
    }
}

static void input_bridge_build_snapshot(void) {
    input_snapshot_mask = mux_input_pressed_mask();
    port_retropad_mask[0] = build_retropad_mask(input_snapshot_mask, 1);

    int16_t ls_x, ls_y, rs_x, rs_y;
    mux_input_get_raw_sticks(&ls_x, &ls_y, &rs_x, &rs_y);
    port_stick_x[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] = apply_analog_transform(ls_x);
    port_stick_y[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] = invert_y_if_needed(apply_analog_transform(ls_y));
    port_stick_x[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] = apply_analog_transform(rs_x);
    port_stick_y[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] = invert_y_if_needed(apply_analog_transform(rs_y));

    int ports_changed = 0;
    if (!port_last_connected[0]) ports_changed = 1;
    port_last_connected[0] = 1;

    for (int i = 0; i < MUX_INPUT_MAX_EXTRA_PLAYERS; i++) {
        const int port = i + 1;
        const int connected = mux_input_extra_player_connected(i);

        if (connected != port_last_connected[port]) ports_changed = 1;
        port_last_connected[port] = connected;

        if (!connected) {
            port_retropad_mask[port] = 0;
            port_stick_x[port][0] = port_stick_y[port][0] = 0;
            port_stick_x[port][1] = port_stick_y[port][1] = 0;
            continue;
        }

        port_retropad_mask[port] = build_retropad_mask(mux_input_extra_player_pressed_mask(i), 0);

        int16_t x, y;
        mux_input_extra_player_stick(i, RETRO_DEVICE_INDEX_ANALOG_LEFT, &x, &y);
        port_stick_x[port][RETRO_DEVICE_INDEX_ANALOG_LEFT] = apply_analog_transform(x);
        port_stick_y[port][RETRO_DEVICE_INDEX_ANALOG_LEFT] = invert_y_if_needed(apply_analog_transform(y));

        mux_input_extra_player_stick(i, RETRO_DEVICE_INDEX_ANALOG_RIGHT, &x, &y);
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

        return (port_retropad_mask[port] & (uint16_t) (1u << id)) ? 1 : 0;
    }

    if (device == RETRO_DEVICE_ANALOG) {
        if (index != RETRO_DEVICE_INDEX_ANALOG_LEFT && index != RETRO_DEVICE_INDEX_ANALOG_RIGHT) return 0;
        if (id == RETRO_DEVICE_ID_ANALOG_X) return port_stick_x[port][index];
        if (id == RETRO_DEVICE_ID_ANALOG_Y) return port_stick_y[port][index];
        return 0;
    }

    return 0;
}
