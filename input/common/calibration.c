#include "calibration.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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

static double clampd(double v, double lo, double hi) {
    if (!isfinite(v)) return lo;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *) a;
    double db = *(const double *) b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static double median_of(double *buf, int n) {
    qsort(buf, (size_t) n, sizeof(double), cmp_double);
    if (n & 1) return buf[n / 2];
    return 0.5 * (buf[n / 2 - 1] + buf[n / 2]);
}

static double smootherstep(double t) {
    t = clampd(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static double soft_knee_0to1(double a) {
    a = clampd(a, 0.0, 1.0);
    if (a <= KNEE_START) return a;
    double t = (a - KNEE_START) / (1.0 - KNEE_START);
    double s = smootherstep(t);
    return KNEE_START + (1.0 - KNEE_START) * s;
}

static void recompute_minmax(struct axis_state *a) {
    a->neg_span = clampd(a->neg_span, MIN_ACTIVE_SPAN, SPAN_MAX);
    a->pos_span = clampd(a->pos_span, MIN_ACTIVE_SPAN, SPAN_MAX);

    a->centre = clampd(a->centre, RAW_MIN, RAW_MAX);

    a->min = clampd(a->centre - a->neg_span, RAW_MIN, a->centre);
    a->max = clampd(a->centre + a->pos_span, a->centre, RAW_MAX);
}

void cal_initialise(struct axis_state *a) {
    memset(a, 0, sizeof(*a));

    a->initialised = 0;
    a->samples = 0;

    a->boot_count = 0;
    a->boot_rejections = 0;
    for (int i = 0; i < BOOT_SAMPLES; i++)
        a->boot_buf[i] = RAW_CENTRE_DEFAULT;

    a->centre = RAW_CENTRE_DEFAULT;

    a->neg_span = START_HALF_SPAN;
    a->pos_span = START_HALF_SPAN;

    a->deadzone = DEADZONE_USER_MIN;

    a->filt = RAW_CENTRE_DEFAULT;
    a->prev_filt = RAW_CENTRE_DEFAULT;
    a->idle_count = 0;

    a->noise_ema = 0.0;

    a->park_sum = 0.0;
    a->park_count = 0;

    recompute_minmax(a);
}

static int finish_boot_calibration(struct axis_state *a) {
    double sample_min = a->boot_buf[0];
    double sample_max = a->boot_buf[0];
    for (int i = 1; i < BOOT_SAMPLES; ++i) {
        sample_min = fmin(sample_min, a->boot_buf[i]);
        sample_max = fmax(sample_max, a->boot_buf[i]);
    }

    double centre = median_of(a->boot_buf, BOOT_SAMPLES);
    int stable = (sample_max - sample_min) <= BOOT_MAX_SPREAD;
    int plausible_centre = fabs(centre - RAW_CENTRE_DEFAULT) <= BOOT_MAX_CENTRE_OFFSET;
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

void cal_update(struct axis_state *a, int raw) {
    double val = clampd((double) raw, RAW_MIN, RAW_MAX);

    if (!a->initialised) {
        if (a->boot_count < BOOT_SAMPLES) {
            a->boot_buf[a->boot_count++] = val;
            return;
        }

        finish_boot_calibration(a);
        return;
    }

    a->samples++;

    a->filt = a->filt + FILTER_ALPHA * (val - a->filt);
    double vel = fabs(a->filt - a->prev_filt);
    a->prev_filt = a->filt;

    int near_centre = fabs(a->filt - a->centre) <= CENTRE_LEARN_BAND;
    int slow = vel <= VEL_THRESH;

    if (near_centre && slow)
        a->idle_count++;
    else
        a->idle_count = 0;

    int idle = a->idle_count >= IDLE_REQUIRED;

    if (idle) {
        a->centre = a->centre + CENTRE_ALPHA * (a->filt - a->centre);

        double dev = fabs(a->filt - a->centre);
        a->noise_ema = a->noise_ema + NOISE_ALPHA * (dev - a->noise_ema);

        double dz = DEADZONE_NOISE_BASE + NOISE_MULT * a->noise_ema;
        a->deadzone = clampd(dz, DEADZONE_USER_MIN, DEADZONE_USER_MAX);
    }

    int park_near = fabs(a->filt - a->centre) <= PARK_BAND;
    int park_slow = vel <= VEL_THRESH;

    if (park_near && park_slow) {
        a->park_sum += a->filt;
        a->park_count++;

        if (a->park_count >= PARK_REQUIRED) {
            double target = a->park_sum / (double) a->park_count;
            a->centre = a->centre + PARK_ALPHA * (target - a->centre);

            a->park_sum = 0.0;
            a->park_count = 0;
        }
    } else {
        a->park_sum = 0.0;
        a->park_count = 0;
    }

    double d = a->filt - a->centre;

    if (d > 0.0) {
        double gate = a->pos_span * OUTER_GATE_RATIO;
        if (d >= gate) {
            double cand = d;
            double alpha = (cand > a->pos_span) ? SPAN_ALPHA_UP : SPAN_ALPHA_DOWN;
            a->pos_span = a->pos_span + alpha * (cand - a->pos_span);
        }
    } else if (d < 0.0) {
        double ad = -d;
        double gate = a->neg_span * OUTER_GATE_RATIO;
        if (ad >= gate) {
            double cand = ad;
            double alpha = (cand > a->neg_span) ? SPAN_ALPHA_UP : SPAN_ALPHA_DOWN;
            a->neg_span = a->neg_span + alpha * (cand - a->neg_span);
        }
    }

    recompute_minmax(a);
}

void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y) {
    cal_update(ax, raw_x);
    cal_update(ay, raw_y);
}

int cal_apply(struct axis_state *a, int raw) {
    if (!a->initialised) return 0;

    double val = clampd((double) raw, RAW_MIN, RAW_MAX);
    double d = val - a->centre;

    double dz = a->deadzone;

    double ad = fabs(d);
    if (ad <= dz) return 0;

    double span = (d >= 0.0) ? a->pos_span : a->neg_span;
    span = fmax(span, MIN_ACTIVE_SPAN);

    double denom = span - dz;
    if (denom < 1.0) denom = 1.0;

    double a_norm = (ad - dz) / denom;
    a_norm = clampd(a_norm, 0.0, 1.0);

    a_norm = soft_knee_0to1(a_norm);

    double out = a_norm * OUTPUT_MAX;
    if (d < 0.0) out = -out;

    if (out > OUTPUT_MAX) out = OUTPUT_MAX;
    if (out < -OUTPUT_MAX) out = -OUTPUT_MAX;

    return (int) lrint(out);
}

int cal_ready(const struct axis_state *a) {
    return a && a->initialised;
}
