#pragma once

#include <stdint.h>

extern struct control controller;

struct control {
    struct {
        int16_t a;
        int16_t b;
        int16_t x;
        int16_t y;
        int16_t z;
        int16_t l1;
        int16_t l2;
        int16_t l3;
        int16_t r1;
        int16_t r2;
        int16_t r3;
        int16_t select;
        int16_t start;
        int16_t menu;
        int16_t up;
        int16_t down;
        int16_t left;
        int16_t right;
    } button;

    struct {
        int16_t axis;
        int16_t l2;
        int16_t r2;
    } trigger;

    struct {
        int16_t axis;
        int16_t up;
        int16_t left;
    } dpad;

    struct {
        struct {
            int16_t axis;
            int16_t up;
            int16_t left;
        } left;
        struct {
            int16_t axis;
            int16_t up;
            int16_t left;
        } right;
    } analog;
};

void load_controller_profile(struct control *controller, char *controller_name);
