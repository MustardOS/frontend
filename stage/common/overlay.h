#pragma once

#include "common.h"

#define OVERLAY_OPTIONS_PATH OVERLAY_RUNNER "options"

enum overlay_option_field {
    opt_gen_alpha = 0,
    opt_gen_anchor,
    opt_gen_scale,
    opt_bat_alpha,
    opt_bat_anchor,
    opt_bat_scale,
    opt_vol_alpha,
    opt_vol_anchor,
    opt_vol_scale,
    opt_bri_alpha,
    opt_bri_anchor,
    opt_bri_scale,
    overlay_option_field_count
};

int get_overlay_option_override(enum overlay_option_field field, int *out);
