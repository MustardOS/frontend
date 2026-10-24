#include <strings.h>
#include <common/base/image_ext.h>

static const char *image_exts[] = {"svg", "png", "apng", "jpg", "jpeg", "webp", "qoi", "tga", "gif", "bmp", "pcx"};
static const int image_exts_n = sizeof(image_exts) / sizeof(image_exts[0]);

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
