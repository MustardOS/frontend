#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "calibration.h"

#define RAW_MIN            0.0
#define RAW_MAX            4095.0
#define RAW_CENTRE_DEFAULT ((RAW_MAX - RAW_MIN) / 2.0)
#define OUTPUT_MAX         32767.0

#define BOOT_SAMPLES           31
#define BOOT_MAX_SPREAD        80.0
#define BOOT_MAX_CENTRE_OFFSET 600.0

#define FILTER_ALPHA 0.25

#define CENTRE_LEARN_BAND 20.0
#define VEL_THRESH        4.0
#define IDLE_REQUIRED     12
#define CENTRE_ALPHA      0.003

#define DEADZONE_NOISE_BASE 6.0
#define NOISE_ALPHA         0.08
#define NOISE_MULT          6.0
#define DEADZONE_USER_MIN   25.0
#define DEADZONE_USER_MAX   250.0

#define START_HALF_SPAN  900.0
#define MIN_ACTIVE_SPAN  80.0
#define OUTER_GATE_RATIO 0.65
#define SPAN_ALPHA_UP    0.0020
#define SPAN_ALPHA_DOWN  0.0005
#define SPAN_MAX         2400.0

#define KNEE_START 0.90

#define PARK_BAND     100.0
#define PARK_REQUIRED 60
#define PARK_ALPHA    0.06

static double clampd(const double v, const double lo, const double hi) {
    if (!isfinite(v)) return lo;

    if (v < lo) return lo;
    if (v > hi) return hi;

    return v;
}

static int cmp_double(const void *a, const void *b) {
    const double da = *(const double *) a;
    const double db = *(const double *) b;

    return da < db ? -1 : da > db ? 1 : 0;
}

static double median_of(double *buf) {
    qsort(buf, BOOT_SAMPLES, sizeof(double), cmp_double);
    if (BOOT_SAMPLES & 1) return buf[BOOT_SAMPLES / 2];

    return (buf[BOOT_SAMPLES / 2 - 1] + buf[BOOT_SAMPLES / 2]) / 2.0;
}

static double smootherstep(double t) {
    t = clampd(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static double soft_knee_0_to1(double a) {
    a = clampd(a, 0.0, 1.0);
    if (a <= KNEE_START) return a;

    const double t = (a - KNEE_START) / (1.0 - KNEE_START);
    const double s = smootherstep(t);

    return KNEE_START + (1.0 - KNEE_START) * s;
}

static void recompute_minmax(struct axis_state *a) {
    a->neg_span = clampd(a->neg_span, MIN_ACTIVE_SPAN, SPAN_MAX);
    a->pos_span = clampd(a->pos_span, MIN_ACTIVE_SPAN, SPAN_MAX);

    a->centre = clampd(a->centre, RAW_MIN, RAW_MAX);

    a->min = clampd(a->centre - a->neg_span, RAW_MIN, a->centre);
    a->max = clampd(a->centre + a->pos_span, a->centre, RAW_MAX);
}

void cal_initialise(struct axis_state *axis) {
    memset(axis, 0, sizeof(*axis));

    axis->initialised = 0;
    axis->samples = 0;

    axis->boot_count = 0;
    axis->boot_rejections = 0;
    for (int i = 0; i < BOOT_SAMPLES; i++)
        axis->boot_buf[i] = RAW_CENTRE_DEFAULT;

    axis->centre = RAW_CENTRE_DEFAULT;

    axis->neg_span = START_HALF_SPAN;
    axis->pos_span = START_HALF_SPAN;

    axis->deadzone = DEADZONE_USER_MIN;

    axis->filt = RAW_CENTRE_DEFAULT;
    axis->prev_filt = RAW_CENTRE_DEFAULT;
    axis->idle_count = 0;

    axis->noise_ema = 0.0;

    axis->park_sum = 0.0;
    axis->park_count = 0;

    recompute_minmax(axis);
}

void cal_seed(struct axis_state *axis, const int raw) {
    if (!axis || axis->initialised) return;

    for (int i = 0; i <= BOOT_SAMPLES && !axis->initialised; ++i)
        cal_update(axis, raw);
}

static int finish_boot_calibration(struct axis_state *a) {
    double sample_min = a->boot_buf[0];
    double sample_max = a->boot_buf[0];

    for (int i = 1; i < BOOT_SAMPLES; ++i) {
        sample_min = fmin(sample_min, a->boot_buf[i]);
        sample_max = fmax(sample_max, a->boot_buf[i]);
    }

    const double centre = median_of(a->boot_buf);
    const int stable = sample_max - sample_min <= BOOT_MAX_SPREAD;
    const int plausible_centre = fabs(centre - RAW_CENTRE_DEFAULT) <= BOOT_MAX_CENTRE_OFFSET;

    if (!stable || !plausible_centre) {

        a->boot_count = 0;
        a->boot_rejections++;
        return 0;
    }

    double startup_noise = 0.0;
    for (int i = 0; i < BOOT_SAMPLES; ++i) {
        startup_noise += fabs(a->boot_buf[i] - centre);
    }
    startup_noise /= BOOT_SAMPLES;

    a->initialised = 1;
    a->samples = BOOT_SAMPLES;
    a->centre = centre;
    a->filt = centre;
    a->prev_filt = centre;
    a->neg_span = START_HALF_SPAN;
    a->pos_span = START_HALF_SPAN;
    a->noise_ema = startup_noise;
    a->deadzone = clampd(DEADZONE_NOISE_BASE + NOISE_MULT * startup_noise, DEADZONE_USER_MIN, DEADZONE_USER_MAX);
    a->idle_count = 0;

    recompute_minmax(a);
    return 1;
}

void cal_update(struct axis_state *axis, const int raw) {
    if (!axis) return;

    const double val = clampd(raw, RAW_MIN, RAW_MAX);

    if (!axis->initialised) {
        if (axis->boot_count < BOOT_SAMPLES) {
            axis->boot_buf[axis->boot_count++] = val;
            return;
        }

        finish_boot_calibration(axis);
        return;
    }

    axis->samples++;

    axis->filt = axis->filt + FILTER_ALPHA * (val - axis->filt);
    const double vel = fabs(axis->filt - axis->prev_filt);
    axis->prev_filt = axis->filt;

    const int near_centre = fabs(axis->filt - axis->centre) <= CENTRE_LEARN_BAND;
    const int slow = vel <= VEL_THRESH;

    if (near_centre && slow)
        axis->idle_count++;
    else
        axis->idle_count = 0;

    const int idle = axis->idle_count >= IDLE_REQUIRED;

    if (idle) {
        axis->centre = axis->centre + CENTRE_ALPHA * (axis->filt - axis->centre);

        const double dev = fabs(axis->filt - axis->centre);
        axis->noise_ema = axis->noise_ema + NOISE_ALPHA * (dev - axis->noise_ema);

        const double dz = DEADZONE_NOISE_BASE + NOISE_MULT * axis->noise_ema;
        axis->deadzone = clampd(dz, DEADZONE_USER_MIN, DEADZONE_USER_MAX);
    }

    const int park_near = fabs(axis->filt - axis->centre) <= PARK_BAND;
    const int park_slow = vel <= VEL_THRESH;

    if (park_near && park_slow) {
        axis->park_sum += axis->filt;
        axis->park_count++;

        if (axis->park_count >= PARK_REQUIRED) {
            const double target = axis->park_sum / (double) axis->park_count;
            axis->centre = axis->centre + PARK_ALPHA * (target - axis->centre);

            axis->park_sum = 0.0;
            axis->park_count = 0;
        }
    } else {
        axis->park_sum = 0.0;
        axis->park_count = 0;
    }

    const double d = axis->filt - axis->centre;

    if (d > 0.0) {
        const double gate = axis->pos_span * OUTER_GATE_RATIO;
        if (d >= gate) {
            const double cand = d;
            const double alpha = cand > axis->pos_span ? SPAN_ALPHA_UP : SPAN_ALPHA_DOWN;
            axis->pos_span = axis->pos_span + alpha * (cand - axis->pos_span);
        }
    } else if (d < 0.0) {
        const double ad = -d;
        const double gate = axis->neg_span * OUTER_GATE_RATIO;
        if (ad >= gate) {
            const double cand = ad;
            const double alpha = cand > axis->neg_span ? SPAN_ALPHA_UP : SPAN_ALPHA_DOWN;
            axis->neg_span = axis->neg_span + alpha * (cand - axis->neg_span);
        }
    }

    recompute_minmax(axis);
}

void cal_update2(struct axis_state *ax, struct axis_state *ay, const int raw_x, const int raw_y) {
    cal_update(ax, raw_x);
    cal_update(ay, raw_y);
}

int cal_apply(const struct axis_state *axis, const int raw) {
    if (!axis || !axis->initialised) return 0;

    const double val = clampd(raw, RAW_MIN, RAW_MAX);
    const double d = val - axis->centre;

    const double dz = axis->deadzone;

    const double ad = fabs(d);
    if (ad <= dz) return 0;

    double span = d >= 0.0 ? axis->pos_span : axis->neg_span;
    span = fmax(span, MIN_ACTIVE_SPAN);

    double denom = span - dz;
    if (denom < 1.0) denom = 1.0;

    double a_norm = (ad - dz) / denom;
    a_norm = clampd(a_norm, 0.0, 1.0);

    a_norm = soft_knee_0_to1(a_norm);

    double out = a_norm * OUTPUT_MAX;
    if (d < 0.0) out = -out;

    if (out > OUTPUT_MAX) out = OUTPUT_MAX;
    if (out < -OUTPUT_MAX) out = -OUTPUT_MAX;

    return (int) lrint(out);
}

int cal_ready(const struct axis_state *axis) {
    return axis && axis->initialised;
}
