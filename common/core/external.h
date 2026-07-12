#pragma once

typedef enum { stage_overlay_disabled = 0, stage_overlay_enabled } stage_overlay_use;

struct ext_core_name {
    const char *core;
    const char *name;
    stage_overlay_use stage_overlay;
};

extern const struct ext_core_name ext_core_names[];
