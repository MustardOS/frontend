#pragma once

struct axis_state {
    int initialised;
    int samples;

    int boot_count;
    int boot_rejections;
    double boot_buf[31];

    double centre;

    double neg_span;
    double pos_span;

    double min;
    double max;

    double deadzone;

    double filt;
    double prev_filt;
    int idle_count;

    double noise_ema;

    double park_sum;
    int park_count;

    int debug_id;
};

void cal_initialise(struct axis_state *axis);

void cal_update(struct axis_state *axis, int raw);

void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y);

int cal_apply(struct axis_state *axis, int raw);

int cal_ready(const struct axis_state *axis);
