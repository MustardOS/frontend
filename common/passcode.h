#pragma once

#include "options.h"

extern struct mux_passcode passcode;
extern struct mux_device device;

struct mux_passcode {
    struct {
        char boot[MAX_BUFFER_SIZE];
        char launch[MAX_BUFFER_SIZE];
        char setting[MAX_BUFFER_SIZE];
        char safety[MAX_BUFFER_SIZE];
    } code;

    struct {
        char boot[MAX_BUFFER_SIZE];
        char launch[MAX_BUFFER_SIZE];
        char setting[MAX_BUFFER_SIZE];
    } message;
};

void load_passcode(struct mux_passcode *passcode);

void save_passcode(struct mux_passcode *passcode);
