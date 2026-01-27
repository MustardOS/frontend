#pragma once

#define COLOUR_CONFIG "/opt/muos/config/settings/video/"

struct colour_state {
    float brightness; // -1.0 .. +1.0 (default 0.0)
    float contrast;   //  0.0 ..  2.0 (default 1.0)
    float saturation; //  0.0 ..  3.0 (default 1.0)
    float hue;        // -1.8 .. +1.8 (default 0.0)
    float gamma;      //  0.5 ..  2.0 (default 1.0)
};

const struct colour_state *colour_get(void);

void colour_reset(void);
