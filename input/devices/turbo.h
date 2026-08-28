#pragma once

#include <stddef.h>

#include "../runtime.h"

#include "../common/uinput.h"
#include "device_helpers.h"

#define TURBO_FLAG_PREFIX MUINPUT_STATE_DIR "/turbo_"

enum { TURBO_TOGGLE_PERIOD = 3 };

struct turbo_state {
    int enabled;
    int physical_down;
    int virtual_down;
    int frame_counter;
};

struct turbo_binding_cfg {
    const char *flag_path;
    unsigned short code;
};

struct turbo_binding {
    const char *flag_path;
    unsigned short code;
    struct turbo_state state;
    int last_output;
    int was_active;
};

void turbo_initialise_bindings(struct turbo_binding *bindings, size_t count, const struct turbo_binding_cfg *cfgs);

void turbo_refresh_flags(struct turbo_binding *bindings, size_t count);

int turbo_note_physical(struct turbo_binding *bindings, size_t count, unsigned short code, int pressed);

void turbo_process_frame(
    struct turbo_binding *bindings, size_t count, struct gamepad *gp, struct device_dirty_state *dirty
);
