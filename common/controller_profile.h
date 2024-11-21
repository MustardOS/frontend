#pragma once

#include "options.h"

extern struct controller_profile controller;

struct controller_profile {
    struct {
        int16_t A;
        int16_t B;
        int16_t X;
        int16_t Y;
        int16_t Z;
        int16_t L1;
        int16_t L2;
        int16_t L3;
        int16_t R1;
        int16_t R2;
        int16_t R3;
        int16_t SELECT;
        int16_t START;
        int16_t MENU;
    } BUTTON;

    struct {
        int16_t AXIS;        
        int16_t UP;
        int16_t LEFT;
    } DPAD;

    struct {
        struct {
            int16_t AXIS;
            int16_t UP;
            int16_t LEFT;
        } LEFT;
        struct {
            int16_t AXIS;
            int16_t UP;
            int16_t LEFT;
        } RIGHT;
    } ANALOG;
};

void load_controller_profile(struct controller_profile *controller, char *controller_name);
