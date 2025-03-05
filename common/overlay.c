#include <stdlib.h>
#include <string.h>
#include "overlay.h"
#include "device.h"
#include "language.h"

#define OVERLAY_STANDARD 15

static char *overlay_standard[] = {
        lang.GENERIC.DISABLED,
        lang.MUXVISUAL.OVERLAY.THEME,
        lang.MUXVISUAL.OVERLAY.CHECKERBOARD.T1,
        lang.MUXVISUAL.OVERLAY.CHECKERBOARD.T4,
        lang.MUXVISUAL.OVERLAY.DIAGONAL.T1,
        lang.MUXVISUAL.OVERLAY.DIAGONAL.T2,
        lang.MUXVISUAL.OVERLAY.DIAGONAL.T4,
        lang.MUXVISUAL.OVERLAY.LATTICE.T1,
        lang.MUXVISUAL.OVERLAY.LATTICE.T4,
        lang.MUXVISUAL.OVERLAY.HORIZONTAL.T1,
        lang.MUXVISUAL.OVERLAY.HORIZONTAL.T2,
        lang.MUXVISUAL.OVERLAY.HORIZONTAL.T4,
        lang.MUXVISUAL.OVERLAY.VERTICAL.T1,
        lang.MUXVISUAL.OVERLAY.VERTICAL.T2,
        lang.MUXVISUAL.OVERLAY.VERTICAL.T4,
};

static char *overlay_640x480[] = {
        // TODO: Add resolution specific overlays!
};

static char *overlay_720x480[] = {
        // TODO: Add resolution specific overlays!
};

static char *overlay_720x576[] = {
        // TODO: Add resolution specific overlays!
};

static char *overlay_720x720[] = {
        // TODO: Add resolution specific overlays!
};

static char *overlay_1024x768[] = {
        // TODO: Add resolution specific overlays!
};

static char *overlay_1280x720[] = {
        // TODO: Add resolution specific overlays!
};

typedef struct {
    int width;
    int height;
    char **overlay_list;
    size_t overlay_count;
} overlay_resolution;

static overlay_resolution overlay_map[] = {
        {640,  480, overlay_640x480,  0},
        {720,  480, overlay_720x480,  0},
        {720,  576, overlay_720x576,  0},
        {720,  720, overlay_720x720,  0},
        {1024, 768, overlay_1024x768, 0},
        {1280, 720, overlay_1280x720, 0},
};

static char **merge_overlays(char **extra, size_t extra_size, size_t *merged_size) {
    *merged_size = OVERLAY_STANDARD + extra_size;
    char **merged = malloc(*merged_size * sizeof(char *));
    if (!merged) return NULL;

    memcpy(merged, overlay_standard, OVERLAY_STANDARD * sizeof(char *));
    memcpy(merged + OVERLAY_STANDARD, extra, extra_size * sizeof(char *));

    return merged;
}

int load_overlay_set(lv_obj_t *overlay_element) {
    char **selected_overlay;
    size_t overlay_count;

    for (size_t i = 0; i < sizeof(overlay_map) / sizeof(overlay_map[0]); i++) {
        if (overlay_map[i].width == device.SCREEN.WIDTH && overlay_map[i].height == device.SCREEN.HEIGHT) {
            selected_overlay = overlay_map[i].overlay_list;
            overlay_count = overlay_map[i].overlay_count;
            break;
        }
    }

    size_t merged_count;
    char **merged_overlay = merge_overlays(selected_overlay, overlay_count, &merged_count);

    if (!merged_overlay) return -1;
    add_drop_down_options(overlay_element, merged_overlay, merged_count);

    free(merged_overlay);
    return merged_count;
}
