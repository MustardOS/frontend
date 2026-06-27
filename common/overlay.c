#include <stdlib.h>
#include "overlay.h"
#include "device.h"
#include "language.h"
#include "ui/nav.h"

static int overlay_size = 1;

static char *overlay_640_x480[] = {
    // TODO: Add resolution specific overlays!
};

static char *overlay_720_x480[] = {
    // TODO: Add resolution specific overlays!
};

static char *overlay_720_x576[] = {
    // TODO: Add resolution specific overlays!
};

static char *overlay_720_x720[] = {
    // TODO: Add resolution specific overlays!
};

static char *overlay_1024_x768[] = {
    // TODO: Add resolution specific overlays!
};

static char *overlay_1280_x720[] = {
    // TODO: Add resolution specific overlays!
};

static char *overlay_1920_x1080[] = {
    // TODO: Add resolution specific overlays!
};

typedef struct {
    int width;
    int height;
    char **overlay_list;
    size_t overlay_count;
} overlay_resolution;

static overlay_resolution overlay_map[] = {
    {640, 480, overlay_640_x480, 0},     {720, 480, overlay_720_x480, 0},   {720, 576, overlay_720_x576, 0},
    {720, 720, overlay_720_x720, 0},     {1024, 768, overlay_1024_x768, 0}, {1280, 720, overlay_1280_x720, 0},
    {1920, 1080, overlay_1920_x1080, 0},
};

int load_overlay_set(lv_obj_t *overlay_element, const int include_theme) {
    char *overlay_patterns[] = {
        lang.muxvisual.overlay.checkerboard.t1, lang.muxvisual.overlay.checkerboard.t4,
        lang.muxvisual.overlay.diagonal.t1,     lang.muxvisual.overlay.diagonal.t2,
        lang.muxvisual.overlay.diagonal.t4,     lang.muxvisual.overlay.lattice.t1,
        lang.muxvisual.overlay.lattice.t4,      lang.muxvisual.overlay.horizontal.t1,
        lang.muxvisual.overlay.horizontal.t2,   lang.muxvisual.overlay.horizontal.t4,
        lang.muxvisual.overlay.vertical.t1,     lang.muxvisual.overlay.vertical.t2,
        lang.muxvisual.overlay.vertical.t4,
    };

    char **selected_overlay = NULL;
    size_t res_overlay_count = 0;

    for (size_t i = 0; i < A_SIZE(overlay_map); i++) {
        if (overlay_map[i].width == device.screen.width && overlay_map[i].height == device.screen.height) {
            selected_overlay = overlay_map[i].overlay_list;
            res_overlay_count = overlay_map[i].overlay_count;
            break;
        }
    }

    lv_dropdown_clear_options(overlay_element);

    int total = 0;

    lv_dropdown_add_option(overlay_element, lang.generic.disabled, LV_DROPDOWN_POS_LAST);
    total++;

    if (include_theme) {
        lv_dropdown_add_option(overlay_element, lang.muxvisual.overlay.theme, LV_DROPDOWN_POS_LAST);
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
