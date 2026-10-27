#include <strings.h>
#include <common/base/image_ext.h>

static const char *image_exts[] = {"svg", "png", "apng", "jpg", "jpeg", "webp", "qoi", "tga", "gif", "bmp", "pcx"};
static const int image_exts_n = sizeof(image_exts) / sizeof(image_exts[0]);

static const char *overlay_exts[] = {"png", "webp", "qoi", "tga"};
static const int overlay_exts_n = sizeof(overlay_exts) / sizeof(overlay_exts[0]);

int is_image_ext(const char *ext) {
    for (int i = 0; i < image_exts_n; i++) {
        if (strcasecmp(ext, image_exts[i]) == 0) return 1;
    }
    return 0;
}

const char **image_ext_list(int *count) {
    *count = image_exts_n;
    return image_exts;
}

int is_overlay_ext(const char *ext) {
    for (int i = 0; i < overlay_exts_n; i++) {
        if (strcasecmp(ext, overlay_exts[i]) == 0) return 1;
    }
    return 0;
}

const char **overlay_ext_list(int *count) {
    *count = overlay_exts_n;
    return overlay_exts;
}
