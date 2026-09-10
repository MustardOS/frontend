#include "geometry.h"

void video_geometry_snap_integer(SDL_Rect *rect, const int source_w, const int source_h) {
    if (!rect || source_w <= 0 || source_h <= 0 || rect->w <= 0 || rect->h <= 0) return;

    int scale = rect->w / source_w;
    const int height_scale = rect->h / source_h;
    if (height_scale < scale) scale = height_scale;
    if (scale < 1) scale = 1;

    const int snapped_w = source_w * scale;
    const int snapped_h = source_h * scale;

    rect->x += (rect->w - snapped_w) / 2;
    rect->y += (rect->h - snapped_h) / 2;
    rect->w = snapped_w;
    rect->h = snapped_h;
}

int video_geometry_is_integer(const SDL_Rect *rect, const int source_w, const int source_h) {
    return rect && source_w > 0 && source_h > 0 && rect->w > 0 && rect->h > 0 && rect->w % source_w == 0
           && rect->h % source_h == 0;
}
