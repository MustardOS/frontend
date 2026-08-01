#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../common/input.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "macro.h"
#include "runtime.h"

#define MACRO_PORT_COUNT (1 + MUX_INPUT_MAX_EXTRA_PLAYERS)

static int held_previous[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int playing[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int step_at[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int step_holding[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int step_repeat_at[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static uint32_t step_phase[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static uint32_t step_target[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];
static int loop_progress[MACRO_PORT_COUNT][PORT_SOURCE_COUNT][MACRO_STEP_MAX];
static int32_t variables[MACRO_PORT_COUNT][PORT_SOURCE_COUNT][MACRO_VAR_MAX];
static uint8_t call_stack[MACRO_PORT_COUNT][PORT_SOURCE_COUNT][MACRO_CALL_DEPTH_MAX];
static uint8_t call_depth[MACRO_PORT_COUNT][PORT_SOURCE_COUNT];

static int16_t stick_x[MACRO_PORT_COUNT][2];
static int16_t stick_y[MACRO_PORT_COUNT][2];
static int stick_active[MACRO_PORT_COUNT][2];

static uint32_t milliseconds_to_frames(const int milliseconds, const double frames_per_second) {
    return (uint32_t) (
        frames_per_second > 0.0 ? (double) milliseconds / 1000.0 * frames_per_second + 0.5 : 6
    );
}

static long next_random(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    return random();
}

static int random_milliseconds(const int minimum, const int maximum) {
    if (maximum <= minimum) return minimum < 0 ? 0 : minimum;
    return minimum + (int) (next_random() % (maximum - minimum + 1));
}

static uint32_t segment_frames(
    const struct macro_step *step, const int holding, const double frames_per_second
) {
    const int milliseconds = holding ? random_milliseconds(step->hold_ms, step->hold_rand_ms)
                                     : random_milliseconds(step->wait_ms, step->wait_rand_ms);
    const uint32_t frames = milliseconds_to_frames(milliseconds, frames_per_second);
    return frames < 1 ? 1 : frames;
}

static int compare_values(const int32_t value, const int operation, const int32_t expected) {
    switch (operation) {
        case if_op_equals:
            return value == expected;
        case if_op_notequals:
            return value != expected;
        case if_op_less:
            return value < expected;
        case if_op_greater:
            return value > expected;
        case if_op_atleast:
            return value >= expected;
        case if_op_atmost:
            return value <= expected;
        default:
            return 0;
    }
}

static int condition_is_true(
    const struct macro_step *step, const int port, const int source, const uint64_t input_mask
) {
    int result = 0;

    if (step->if_test == if_test_count_compare) {
        result = compare_values(
            loop_progress[port][source][step->if_loop_ref] + 1, step->if_op, step->loop_count
        );
    } else if (step->if_test == if_test_var_compare) {
        const int32_t expected = step->if_rhs_is_var ? variables[port][source][step->if_rhs_var_index]
                                                     : step->var_value;
        result = compare_values(variables[port][source][step->var_index], step->if_op, expected);
    } else if (step->if_test == if_test_random) {
        result = step->loop_count > 0 && next_random() % 100 < step->loop_count;
    } else if (step->target_mask != 0) {
        const int mux_type = session_settings_mux_type_for_target(__builtin_ctz((unsigned) step->target_mask));
        result = mux_type >= 0 && (input_mask & BIT(mux_type)) != 0;
    }

    return step->if_negate ? !result : result;
}

static int32_t clamp_variable(const int64_t value) {
    if (value > MACRO_VALUE_LIMIT) return MACRO_VALUE_LIMIT;
    if (value < -MACRO_VALUE_LIMIT) return -MACRO_VALUE_LIMIT;
    return (int32_t) value;
}

static void angle_components(const int32_t fixed_degrees, int64_t *sine, int64_t *cosine) {
    static const int32_t angles[] = {
        843314857, 497837829, 263043837, 133525159, 67021687, 33543516, 16775851, 8388437,
        4194283,   2097149,   1048576,   524288,    262144,   131072,   65536,    32768,
        16384,     8192,      4096,      2048,      1024,     512,      256,      128,
    };

    int64_t angle = fixed_degrees % 360000;
    if (angle >= 180000) angle -= 360000;
    if (angle < -180000) angle += 360000;

    int sign = 1;
    if (angle > 90000) {
        angle -= 180000;
        sign = -1;
    } else if (angle < -90000) {
        angle += 180000;
        sign = -1;
    }

    int64_t x = 652032874;
    int64_t y = 0;
    int64_t z = angle * 3373259426LL / 180000;

    for (int i = 0; i < (int) (sizeof(angles) / sizeof(angles[0])); i++) {
        const int64_t x_shift = x / (1LL << i);
        const int64_t y_shift = y / (1LL << i);
        const int64_t old_x = x;

        if (z >= 0) {
            x -= y_shift;
            y += x_shift;
            z -= angles[i];
        } else {
            x += y_shift;
            y -= old_x / (1LL << i);
            z += angles[i];
        }
    }

    *sine = y * sign;
    *cosine = x * sign;
}

static int32_t component_to_variable(const int64_t component) {
    const int64_t half = 1LL << 29;
    return (int32_t) (
        component >= 0 ? (component * MACRO_VALUE_SCALE + half) / (1LL << 30)
                       : (component * MACRO_VALUE_SCALE - half) / (1LL << 30)
    );
}

static int apply_variable_step(const struct macro_step *step, const int port, const int source) {
    int32_t *value = &variables[port][source][step->var_index];
    const int32_t operand = step->var_rhs_is_var ? variables[port][source][step->var_rhs_index] : step->var_value;

    switch (step->var_op) {
        case var_op_set:
            *value = operand;
            return 1;
        case var_op_add:
            *value = clamp_variable((int64_t) *value + operand);
            return 1;
        case var_op_subtract:
            *value = clamp_variable((int64_t) *value - operand);
            return 1;
        case var_op_multiply:
            *value = clamp_variable((int64_t) *value * operand / MACRO_VALUE_SCALE);
            return 1;
        case var_op_divide:
            if (operand == 0) return 0;
            *value = clamp_variable((int64_t) *value * MACRO_VALUE_SCALE / operand);
            return 1;
        case var_op_modulo:
            if (operand == 0) return 0;
            *value %= operand;
            return 1;
        case var_op_sine:
        case var_op_cosine:
        case var_op_tangent: {
            int64_t sine;
            int64_t cosine;
            angle_components(operand, &sine, &cosine);

            if (step->var_op == var_op_sine) {
                *value = component_to_variable(sine);
            } else if (step->var_op == var_op_cosine) {
                *value = component_to_variable(cosine);
            } else {
                const int32_t half_turn = operand % 180000;
                if (half_turn == 90000 || half_turn == -90000 || cosine == 0) return 0;
                *value = clamp_variable(sine * MACRO_VALUE_SCALE / cosine);
            }
            return 1;
        }
        case var_op_floor:
            *value = operand >= 0 ? operand / MACRO_VALUE_SCALE * MACRO_VALUE_SCALE
                                  : -((-operand + MACRO_VALUE_SCALE - 1) / MACRO_VALUE_SCALE * MACRO_VALUE_SCALE);
            return 1;
        case var_op_ceiling:
            *value = operand <= 0 ? operand / MACRO_VALUE_SCALE * MACRO_VALUE_SCALE
                                  : (operand + MACRO_VALUE_SCALE - 1) / MACRO_VALUE_SCALE * MACRO_VALUE_SCALE;
            return 1;
        default:
            return 0;
    }
}

void macro_runtime_begin_port(const int port) {
    stick_active[port][0] = 0;
    stick_active[port][1] = 0;
}

uint16_t macro_runtime_drive(
    const int port, const int source, const int macro_index, const int raw_held, const uint64_t input_mask,
    const double frames_per_second
) {
    const int press_edge = raw_held && !held_previous[port][source];
    held_previous[port][source] = raw_held;

    if (!press_edge && !playing[port][source]) return 0;

    const struct macro_entry *macro = macros_get_by_index(macro_index);
    if (!macro || macro->step_count <= 0) {
        playing[port][source] = 0;
        return 0;
    }

    if (press_edge) {
        playing[port][source] = 1;
        step_at[port][source] = 0;
        step_repeat_at[port][source] = 0;
        step_phase[port][source] = 0;
        step_target[port][source] = 0;
        step_holding[port][source] = 1;
        memset(loop_progress[port][source], 0, sizeof(loop_progress[port][source]));
        memset(variables[port][source], 0, sizeof(variables[port][source]));
        call_depth[port][source] = 0;
    }

    if (!playing[port][source]) return 0;

    for (int guard = 0; guard < MACRO_CONTROL_BUDGET && playing[port][source]; guard++) {
        if (step_at[port][source] < 0 || step_at[port][source] >= macro->step_count) {
            step_at[port][source] = 0;
            playing[port][source] = 0;
            break;
        }

        const struct macro_step *control = &macro->steps[step_at[port][source]];
        if (control->kind != macro_step_goto && control->kind != macro_step_loop && control->kind != macro_step_if
            && control->kind != macro_step_setvar && control->kind != macro_step_call
            && control->kind != macro_step_return)
            break;

        int take_jump = control->kind == macro_step_goto;
        if (control->kind == macro_step_loop) {
            if (loop_progress[port][source][step_at[port][source]] < control->loop_count) {
                loop_progress[port][source][step_at[port][source]]++;
                take_jump = 1;
            } else {
                loop_progress[port][source][step_at[port][source]] = 0;
            }
        } else if (control->kind == macro_step_if) {
            take_jump = condition_is_true(control, port, source, input_mask);
        } else if (control->kind == macro_step_setvar) {
            if (!apply_variable_step(control, port, source)) {
                playing[port][source] = 0;
                break;
            }
        } else if (control->kind == macro_step_call) {
            if (call_depth[port][source] >= MACRO_CALL_DEPTH_MAX) {
                playing[port][source] = 0;
                break;
            }
            call_stack[port][source][call_depth[port][source]++] = (uint8_t) (step_at[port][source] + 1);
            take_jump = 1;
        } else if (control->kind == macro_step_return) {
            if (call_depth[port][source] == 0) {
                playing[port][source] = 0;
                break;
            }
            step_at[port][source] = call_stack[port][source][--call_depth[port][source]];
            step_repeat_at[port][source] = 0;
            step_phase[port][source] = 0;
            step_target[port][source] = 0;
            step_holding[port][source] = 1;
            continue;
        }

        if (take_jump) {
            step_at[port][source] = control->jump_target;
        } else {
            step_at[port][source]++;
            if (step_at[port][source] >= macro->step_count) {
                step_at[port][source] = 0;
                playing[port][source] = 0;
                break;
            }
        }

        step_repeat_at[port][source] = 0;
        step_phase[port][source] = 0;
        step_target[port][source] = 0;
        step_holding[port][source] = 1;
    }

    if (playing[port][source] && (step_at[port][source] < 0 || step_at[port][source] >= macro->step_count)) {
        step_at[port][source] = 0;
        playing[port][source] = 0;
    }

    if (playing[port][source]) {
        const struct macro_step *landed = &macro->steps[step_at[port][source]];
        if (landed->kind != macro_step_button && landed->kind != macro_step_stick) {
            playing[port][source] = 0;
            step_at[port][source] = 0;
        }
    }

    if (!playing[port][source]) return 0;

    const struct macro_step *step = &macro->steps[step_at[port][source]];
    const int holding = step_holding[port][source];

    if (step_target[port][source] == 0)
        step_target[port][source] = segment_frames(step, holding, frames_per_second);
    const uint32_t current_target = step_target[port][source];

    uint16_t output = 0;
    if (step->kind == macro_step_stick) {
        const int stick = step->stick_index & 1;
        stick_active[port][stick] = 1;
        stick_x[port][stick] = holding ? (int16_t) step->axis_x : 0;
        stick_y[port][stick] = holding ? (int16_t) step->axis_y : 0;
    } else if (holding) {
        output = (uint16_t) (step->target_mask & 0xFFFF);
    }

    step_phase[port][source]++;
    if (step_phase[port][source] >= current_target) {
        step_phase[port][source] = 0;
        step_target[port][source] = 0;

        if (!step_holding[port][source]) {
            step_holding[port][source] = 1;
        } else {
            step_repeat_at[port][source]++;

            if (step_repeat_at[port][source] >= step->repeat) {
                step_repeat_at[port][source] = 0;
                step_at[port][source]++;
                step_holding[port][source] = 1;

                if (step_at[port][source] >= macro->step_count) {
                    step_at[port][source] = 0;
                    playing[port][source] = 0;
                }
            } else {
                step_holding[port][source] = 0;
            }
        }
    }

    return output;
}

int macro_runtime_stick(const int port, const int stick, int16_t *x, int16_t *y) {
    if (!stick_active[port][stick]) return 0;
    *x = stick_x[port][stick];
    *y = stick_y[port][stick];
    return 1;
}

void macro_runtime_reset_port(const int port) {
    memset(held_previous[port], 0, sizeof(held_previous[port]));
    memset(playing[port], 0, sizeof(playing[port]));
    memset(step_at[port], 0, sizeof(step_at[port]));
    memset(step_holding[port], 0, sizeof(step_holding[port]));
    memset(step_repeat_at[port], 0, sizeof(step_repeat_at[port]));
    memset(step_phase[port], 0, sizeof(step_phase[port]));
    memset(step_target[port], 0, sizeof(step_target[port]));
    memset(loop_progress[port], 0, sizeof(loop_progress[port]));
    memset(variables[port], 0, sizeof(variables[port]));
    memset(call_stack[port], 0, sizeof(call_stack[port]));
    memset(call_depth[port], 0, sizeof(call_depth[port]));
    memset(stick_x[port], 0, sizeof(stick_x[port]));
    memset(stick_y[port], 0, sizeof(stick_y[port]));
    memset(stick_active[port], 0, sizeof(stick_active[port]));
}
