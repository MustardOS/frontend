#pragma once

struct axis_state;
struct device_backend;
struct gamepad;
struct input_tester;

struct input_tester *input_tester_create(
    const struct device_backend *backend, struct gamepad *gamepad, unsigned int rumble_strength, struct axis_state *lx,
    struct axis_state *ly, struct axis_state *rx, struct axis_state *ry
);

int input_tester_poll(struct input_tester *tester);

void input_tester_render(struct input_tester *tester, int force);

void input_tester_destroy(struct input_tester *tester);
