#include <string.h>
#include "../../common/input.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../core/perf.h"
#include "../settings/settings.h"
#include "../state/macro.h"

#define MUX_RETRO_PORT_COUNT (1 + MUX_INPUT_MAX_EXTRA_PLAYERS)

static int suppress_until_released[mux_input_count];

static unsigned input_epoch = 0;
static unsigned input_polled_epoch = (unsigned) -1;

static uint16_t port_retropad_mask[MUX_RETRO_PORT_COUNT];
static int16_t port_stick_x[MUX_RETRO_PORT_COUNT][2];
static int16_t port_stick_y[MUX_RETRO_PORT_COUNT][2];

static int port_last_connected[MUX_RETRO_PORT_COUNT];

static int turbo_held_prev[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static uint32_t turbo_phase[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];

static int macro_held_prev[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int macro_playing[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int macro_step_at[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int macro_step_holding[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int macro_step_repeat_at[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static uint32_t macro_step_phase[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int macro_loop_progress[MUX_RETRO_PORT_COUNT][PORT_SOURCE_COUNT][MACRO_STEP_MAX];

static uint32_t ms_to_frames(const int ms, const double fps) {
    return (uint32_t) (fps > 0.0 ? (double) ms / 1000.0 * fps + 0.5 : 6);
}

static int resolve_raw_held(const mux_input_type mux_type, const uint64_t mask, const int apply_suppress) {
    int raw_held = (mask & BIT(mux_type)) != 0;

    if (apply_suppress && suppress_until_released[mux_type]) {
        if (!raw_held) suppress_until_released[mux_type] = 0;
        raw_held = 0;
    }

    return raw_held;
}

static int if_condition_true(const struct macro_step *step, const int port, const int s, const uint64_t mask) {
    if (step->if_test == if_test_count_compare) {
        const int count = macro_loop_progress[port][s][step->if_loop_ref] + 1;
        switch (step->if_op) {
            case if_op_equals:
                return count == step->loop_count;
            case if_op_notequals:
                return count != step->loop_count;
            case if_op_less:
                return count < step->loop_count;
            case if_op_greater:
                return count > step->loop_count;
            case if_op_atleast:
                return count >= step->loop_count;
            default: // if_op_atmost
                return count <= step->loop_count;
        }
    }

    if (step->target_mask == 0) return step->if_negate;

    const int mux_type = session_settings_mux_type_for_target(__builtin_ctz((unsigned) step->target_mask));
    const int held = mux_type >= 0 && (mask & BIT(mux_type)) != 0;
    return step->if_negate ? !held : held;
}

static uint16_t drive_macro(const int port, const int s, const int macro_index, const int raw_held, const uint64_t mask) {
    const int press_edge = raw_held && !macro_held_prev[port][s];
    macro_held_prev[port][s] = raw_held;

    const struct macro_entry *macro = macros_get_by_index(macro_index);
    if (!macro || macro->step_count <= 0) {
        macro_playing[port][s] = 0;
        return 0;
    }

    if (press_edge) {
        macro_playing[port][s] = 1;
        macro_step_at[port][s] = 0;
        macro_step_repeat_at[port][s] = 0;
        macro_step_phase[port][s] = 0;
        macro_step_holding[port][s] = 1;
        memset(macro_loop_progress[port][s], 0, sizeof(macro_loop_progress[port][s]));
    }

    if (!macro_playing[port][s]) return 0;

    for (int guard = 0; guard < MACRO_STEP_MAX * 4 && macro_playing[port][s]; guard++) {
        const struct macro_step *control = &macro->steps[macro_step_at[port][s]];
        if (control->kind != macro_step_goto && control->kind != macro_step_loop && control->kind != macro_step_if)
            break;

        int take_jump = control->kind == macro_step_goto;
        if (control->kind == macro_step_loop) {
            if (macro_loop_progress[port][s][macro_step_at[port][s]] < control->loop_count) {
                macro_loop_progress[port][s][macro_step_at[port][s]]++;
                take_jump = 1;
            } else {
                macro_loop_progress[port][s][macro_step_at[port][s]] = 0;
            }
        } else if (control->kind == macro_step_if) {
            take_jump = if_condition_true(control, port, s, mask);
        }

        if (take_jump) {
            macro_step_at[port][s] = control->jump_target;
        } else {
            macro_step_at[port][s]++;
            if (macro_step_at[port][s] >= macro->step_count) {
                macro_step_at[port][s] = 0;
                macro_playing[port][s] = 0;
                break;
            }
        }

        macro_step_repeat_at[port][s] = 0;
        macro_step_phase[port][s] = 0;
        macro_step_holding[port][s] = 1;
    }

    if (macro_playing[port][s]) {
        const struct macro_step *landed = &macro->steps[macro_step_at[port][s]];
        if (landed->kind != macro_step_button) {
            macro_playing[port][s] = 0;
            macro_step_at[port][s] = 0;
        }
    }

    if (!macro_playing[port][s]) return 0;

    const double fps = core_get_target_fps();
    const struct macro_step *step = &macro->steps[macro_step_at[port][s]];

    uint32_t hold_frames = ms_to_frames(step->hold_ms, fps);
    if (hold_frames < 1) hold_frames = 1;

    uint32_t wait_frames = ms_to_frames(step->wait_ms, fps);
    if (wait_frames < 1) wait_frames = 1;

    const uint16_t bit_out = macro_step_holding[port][s] ? (uint16_t) (step->target_mask & 0xFFFF) : 0;
    const uint32_t current_target = macro_step_holding[port][s] ? hold_frames : wait_frames;

    macro_step_phase[port][s]++;
    if (macro_step_phase[port][s] >= current_target) {
        macro_step_phase[port][s] = 0;

        if (!macro_step_holding[port][s]) {
            macro_step_holding[port][s] = 1;
        } else {
            macro_step_repeat_at[port][s]++;

            if (macro_step_repeat_at[port][s] >= step->repeat) {
                macro_step_repeat_at[port][s] = 0;
                macro_step_at[port][s]++;
                macro_step_holding[port][s] = 1;

                if (macro_step_at[port][s] >= macro->step_count) {
                    macro_step_at[port][s] = 0;
                    macro_playing[port][s] = 0;
                }
            } else {
                macro_step_holding[port][s] = 0;
            }
        }
    }

    return bit_out;
}

static uint16_t build_retropad_mask(const int port, const uint64_t mask, const int apply_suppress) {
    uint16_t out = 0;
    const double fps = core_get_target_fps();

    for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
        const mux_input_type mux_type = (mux_input_type) session_settings_source_types[s];

        const int macro_index = session_settings.port_source_macro[port][s];
        if (macro_index >= 0) {
            out |= drive_macro(port, s, macro_index, resolve_raw_held(mux_type, mask, apply_suppress), mask);
            continue;
        }

        const int target = session_settings.port_source_target[port][s];
        if (target < 0 || target >= 16) continue;

        const int raw_held = resolve_raw_held(mux_type, mask, apply_suppress);

        int held = raw_held;
        const int rate = session_settings.port_source_turbo[port][s];

        if (rate > 0) {
            if (raw_held) {
                if (!turbo_held_prev[port][s]) {
                    turbo_phase[port][s] = 0; // immediate first press turbo go!
                } else {
                    turbo_phase[port][s]++;
                }

                uint32_t period = ms_to_frames(rate, fps);
                if (period < 2) period = 2;
                held = turbo_phase[port][s] % period < period / 2;
            } else {
                turbo_phase[port][s] = 0;
            }
        }

        turbo_held_prev[port][s] = raw_held;

        if (held) out |= (uint16_t) (1u << target);
    }

    return out;
}

static int stick_has_bound_direction(const int port, const int stick) {
    const int first = 16 + stick * 4;

    for (int s = first; s < first + 4; s++) {
        if (session_settings.port_source_target[port][s] >= 0) return 1;
    }

    return 0;
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

static uint32_t resolve_cached_generation = (uint32_t) -1;
static int resolve_cached_assignment[MUX_RETRO_PORT_COUNT];
static char resolve_cached_keys[MUX_RETRO_PORT_COUNT][64];

static void resolve_port_assignments_cached(void) {
    const uint32_t generation = mux_input_source_generation();

    if (generation == resolve_cached_generation
        && memcmp(resolve_cached_assignment, session_settings.port_assignment, sizeof(resolve_cached_assignment)) == 0
        && memcmp(resolve_cached_keys, session_settings.port_device_key, sizeof(resolve_cached_keys)) == 0)
        return;

    resolve_port_assignments();

    resolve_cached_generation = generation;
    memcpy(resolve_cached_assignment, session_settings.port_assignment, sizeof(resolve_cached_assignment));
    memcpy(resolve_cached_keys, session_settings.port_device_key, sizeof(resolve_cached_keys));
}

static void input_bridge_build_snapshot(void) {
    const int track_latency = perf_is_enabled();
    const uint64_t previous_signature = track_latency ? input_bridge_snapshot_signature() : 0;

    resolve_port_assignments_cached();

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
            for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
                turbo_held_prev[port][s] = 0;
                turbo_phase[port][s] = 0;
                macro_held_prev[port][s] = 0;
                macro_playing[port][s] = 0;
                macro_step_at[port][s] = 0;
                macro_step_phase[port][s] = 0;
            }
            continue;
        }

        port_retropad_mask[port] = build_retropad_mask(port, mux_input_source_pressed_mask(source), source == 0);

        for (int s = 0; s < 2; s++) {
            if (stick_has_bound_direction(port, s)) {
                port_stick_x[port][s] = 0;
                port_stick_y[port][s] = 0;
                continue;
            }

            int16_t x, y;
            mux_input_source_stick(source, s, &x, &y);
            port_stick_x[port][s] = apply_analog_transform(x);
            port_stick_y[port][s] = invert_y_if_needed(apply_analog_transform(y));
        }
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
