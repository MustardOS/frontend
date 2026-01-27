#include "stretch.h"
#include "scale.h"
#include "rotate.h"

void stretch_draw_size(int tex_w, int tex_h, int fb_w, int fb_h, int scale, int rot, int *out_w, int *out_h) {
    int eff_w = tex_w;
    int eff_h = tex_h;
    rotate_dims(tex_w, tex_h, rot, &eff_w, &eff_h);

    float draw_w = (float) eff_w;
    float draw_h = (float) eff_h;

    if (draw_w < 1.0f) draw_w = 1.0f;
    if (draw_h < 1.0f) draw_h = 1.0f;

    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;

    if (scale == SCALE_FIT) {
        const float sx = (float) fb_w / draw_w;
        const float sy = (float) fb_h / draw_h;
        const float s = (sx < sy) ? sx : sy;

        draw_w *= s;
        draw_h *= s;
    } else if (scale == SCALE_STRETCH) {
        draw_w = (float) fb_w;
        draw_h = (float) fb_h;
    }

    int iw = (int) lroundf(draw_w);
    int ih = (int) lroundf(draw_h);

    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;

    if (out_w) *out_w = iw;
    if (out_h) *out_h = ih;
}
