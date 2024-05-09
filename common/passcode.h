#pragma once

#include "options.h"

extern struct mux_passcode passcode;

struct mux_passcode {
    struct {
        char BOOT[MAX_BUFFER_SIZE];
        char LAUNCH[MAX_BUFFER_SIZE];
        char SETTING[MAX_BUFFER_SIZE];
    } CODE;

    struct {
        char BOOT[MAX_BUFFER_SIZE];
        char LAUNCH[MAX_BUFFER_SIZE];
        char SETTING[MAX_BUFFER_SIZE];
    } MESSAGE;
};

void load_passcode(struct mux_passcode *passcode);
