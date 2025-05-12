#pragma once

#include "options.h"

extern struct mux_parentlock parentlock;
extern struct mux_device device;

struct mux_parentlock {
    struct {
        char UNLOCK[MAX_BUFFER_SIZE];
    } CODE;

    struct {
        char UNLOCK[MAX_BUFFER_SIZE];
        char SETTING[MAX_BUFFER_SIZE];
    } MESSAGE;

    struct {
    	char MONDAY[MAX_BUFFER_SIZE];
    	char TUESDAY[MAX_BUFFER_SIZE];
    	char WEDNESDAY[MAX_BUFFER_SIZE];
    	char THURSDAY[MAX_BUFFER_SIZE];
    	char FRIDAY[MAX_BUFFER_SIZE];
    	char SATURDAY[MAX_BUFFER_SIZE];
    	char SUNDAY[MAX_BUFFER_SIZE];
    } TIMES;
};

void load_parentlock(struct mux_parentlock *parentlock, struct mux_device *device);
