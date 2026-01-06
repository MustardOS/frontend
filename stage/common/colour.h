#pragma once

#define COLOUR_CONFIG "/opt/muos/config/settings/video/"

struct colour_state {
    float brightness; // -1.0 .. +1.0
    float contrast;   //  0.0 ..  2.0
    float saturation; //  0.0 ..  2.0
    float hue;        //  -PI .. +PI  (mmm pie)
    float gamma;      //  0.1 ..  4.0

};

const struct colour_state *colour_get(void);
