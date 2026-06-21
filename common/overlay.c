#include <stdlib.h>
#include <string.h>
#include "overlay.h"
#include "device.h"
#include "language.h"
#include "ui/nav.h"

static int overlay_size = 1;

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

static char *overlay_1920x1080[] = {
        // TODO: Add resolution specific overlays!
};

typedef struct {
    int width;
    int height;
    char **overlay_list;
    size_t overlay_count;
} overlay_resolution;

static overlay_resolution overlay_map[] = {
        {640,  480,  overlay_640x480,   0},
        {720,  480,  overlay_720x480,   0},
        {720,  576,  overlay_720x576,   0},
        {720,  720,  overlay_720x720,   0},
        {1024, 768,  overlay_1024x768,  0},
        {1280, 720,  overlay_1280x720,  0},
        {1920, 1080, overlay_1920x1080, 0},
};

static char **merge_overlays(char **standard, char **extra, size_t extra_size, size_t *merged_size) {
    *merged_size = overlay_size + extra_size;
    char **merged = malloc(*merged_size * sizeof(char *));
    if (!merged) return NULL;

    memcpy(merged, standard, overlay_size * sizeof(char *));
    if (extra_size > 0 && extra) {
        memcpy(merged + overlay_size, extra, extra_size * sizeof(char *));
    }

    return merged;
}

int load_overlay_set(lv_obj_t *overlay_element, int include_theme) {
    char *overlay_patterns[] = {
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

    char **selected_overlay = NULL;
    size_t res_overlay_count = 0;

    for (size_t i = 0; i < A_SIZE(overlay_map); i++) {
        if (overlay_map[i].width == device.SCREEN.WIDTH && overlay_map[i].height == device.SCREEN.HEIGHT) {
            selected_overlay = overlay_map[i].overlay_list;
            res_overlay_count = overlay_map[i].overlay_count;
            break;
        }
    }

    lv_dropdown_clear_options(overlay_element);

    int total = 0;

    lv_dropdown_add_option(overlay_element, lang.GENERIC.DISABLED, LV_DROPDOWN_POS_LAST);
    total++;

    if (include_theme) {
        lv_dropdown_add_option(overlay_element, lang.MUXVISUAL.OVERLAY.THEME, LV_DROPDOWN_POS_LAST);
        total++;
    }

    for (size_t i = 0; i < A_SIZE(overlay_patterns); i++) {
        lv_dropdown_add_option(overlay_element, overlay_patterns[i], LV_DROPDOWN_POS_LAST);
        total++;
    }

    for (size_t i = 0; i < res_overlay_count; i++) {
        lv_dropdown_add_option(overlay_element, selected_overlay[i], LV_DROPDOWN_POS_LAST);
        total++;
    }

    overlay_size = (int) A_SIZE(overlay_patterns) + (include_theme ? 1 : 0) + 1;

    return total;
}
