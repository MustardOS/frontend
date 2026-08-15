#pragma once

#include <stddef.h>

enum coreinfo_feature {
    coreinfo_feature_save_states = 0,
    coreinfo_feature_run_ahead,
    coreinfo_feature_netplay,
    coreinfo_feature_count
};

enum coreinfo_state_load_policy {
    coreinfo_state_load_exact = 0,
    coreinfo_state_load_core
};

struct coreinfo_override {
    const char *name;
    int features[coreinfo_feature_count];
    size_t state_max_bytes;
    int state_warmup_frames;
    enum coreinfo_state_load_policy state_load_policy;
};

#define COREINFO_INHERIT (-1)

#define COREINFO_CORE(IDENT, NAME, SAVE_STATES, RUN_AHEAD, NETPLAY, STATE_MAX_BYTES, WARMUP_FRAMES, LOAD_POLICY)     \
    const struct coreinfo_override coreinfo_##IDENT = {                                                             \
        .name = NAME,                                                                                                \
        .features = {SAVE_STATES, RUN_AHEAD, NETPLAY},                                                              \
        .state_max_bytes = STATE_MAX_BYTES,                                                                          \
        .state_warmup_frames = WARMUP_FRAMES,                                                                        \
        .state_load_policy = LOAD_POLICY                                                                              \
    }

#define COREINFO_DECLARE(IDENT) extern const struct coreinfo_override coreinfo_##IDENT;

void coreinfo_init(const char *core_path);

int coreinfo_feature_enabled(enum coreinfo_feature feature);

size_t coreinfo_state_max_bytes(void);

int coreinfo_state_warmup_frames(void);

enum coreinfo_state_load_policy coreinfo_state_load_policy(void);

const char *coreinfo_name(void);

#define COREINFO_LIST COREINFO(flycastvl)
