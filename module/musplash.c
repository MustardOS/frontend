#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <png.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

typedef enum {
    SCALE_ORIGINAL = 0,
    SCALE_KEEP = 1,
    SCALE_STRETCH = 2,
    SCALE_FILL = 3
} scale_mode_t;

typedef struct {
    uint8_t *data;

    size_t size;
    size_t offset;
} png_mem_t;

typedef struct {
    uint8_t *pixels;

    int width;
    int height;
} image_t;

typedef struct {
    int fd;

    uint8_t *mem;
    uint8_t *base;
    size_t mem_size;

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    int width;
    int height;
    int stride;

    int bpp;
    int bytes_per_pixel;
} fb_t;

typedef struct {
    int enabled;
    uint8_t top_r;
    uint8_t top_g;
    uint8_t top_b;
    uint8_t bot_r;
    uint8_t bot_g;
    uint8_t bot_b;
} gradient_t;

typedef struct {
    const char *image_path;
    const char *fb_path;

    int rotate;
    scale_mode_t scale;

    int wait;
    int verbose;
    int clear;

    gradient_t gradient;

    int recolour_enabled;
    uint8_t recolour_r;
    uint8_t recolour_g;
    uint8_t recolour_b;

    int recolour_alpha;
} options_t;

static volatile sig_atomic_t keep_running = 1;

static void log_msg(const options_t *opts, const char *msg) {
    if (opts->verbose) fprintf(stderr, "%s\n", msg);
}

static void log_fmt(const options_t *opts, const char *fmt, const char *value) {
    if (opts->verbose) fprintf(stderr, fmt, value);
}

static void handle_signal(int sig) {
    (void) sig;
    keep_running = 0;
}

static void setup_signals(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;

    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void print_usage(const char *name) {
    fprintf(stderr,
            "Usage:\n"
            "  %s -i <image.png> [options]\n"
            "  %s --image <image.png> [options]\n"
            "  %s -c [options]\n"
            "  %s <image.png> [fbdev]\n\n"
            "Options:\n"
            "  -i, --image <path>          PNG image path\n"
            "  -f, --fb <path>             Framebuffer device, default: /dev/fb0\n"
            "  -r, --rotate <0|90|180|270> Rotation, default: 0\n"
            "  -s, --scale <mode>          0/original | 1/keep | 2/stretch | 3/fill, default: keep\n"
            "  -g, --gradient <A:B>        Vertical background gradient, top:bottom\n"
            "                              e.g. 000000:FFFFFF or #1A1A2E:#16213E\n"
            "  -t, --tint <RRGGBB>         PNG recolour (tint), e.g. FFFFFF or #FFFFFF\n"
            "  -a, --alpha <0-100>         Recolour strength as a percentage, default: 0 (off)\n"
            "  -c, --clear                 Clear the framebuffer to black and exit\n"
            "  -w, --wait                  Keep running until killed\n"
            "  -v, --verbose               Enable log output\n"
            "  -h, --help                  Show this help\n\n"
            "Scale modes:\n"
            "  0, original                 Draw at original size, centred\n"
            "  1, keep                     Keep aspect ratio and fit inside the screen\n"
            "  2, stretch                  Stretch to the full screen\n"
            "  3, fill                     Keep aspect ratio, fill screen, crop overflow\n",
            name, name, name, name);
}

static const char *scale_name(scale_mode_t scale) {
    switch (scale) {
        case SCALE_ORIGINAL:
            return "original";
        case SCALE_KEEP:
            return "keep";
        case SCALE_STRETCH:
            return "stretch";
        case SCALE_FILL:
            return "fill";
        default:
            return "unknown";
    }
}

static int parse_scale(const char *value, scale_mode_t *scale) {
    if (strcmp(value, "0") == 0 || strcmp(value, "original") == 0 || strcmp(value, "orig") == 0) {
        *scale = SCALE_ORIGINAL;
        return 0;
    }

    if (strcmp(value, "1") == 0 || strcmp(value, "keep") == 0 || strcmp(value, "aspect") == 0 || strcmp(value, "contain") == 0) {
        *scale = SCALE_KEEP;
        return 0;
    }

    if (strcmp(value, "2") == 0 || strcmp(value, "stretch") == 0) {
        *scale = SCALE_STRETCH;
        return 0;
    }

    if (strcmp(value, "3") == 0 || strcmp(value, "fill") == 0 || strcmp(value, "cover") == 0) {
        *scale = SCALE_FILL;
        return 0;
    }

    return -1;
}

static int parse_rotation(const char *value, int *rotate) {
    char *end = NULL;
    long r;

    errno = 0;
    r = strtol(value, &end, 10);

    if (errno != 0 || !end || *end != '\0') return -1;

    switch (r) {
        case 0:
        case 90:
        case 180:
        case 270:
            *rotate = (int) r;
            return 0;
        default:
            return -1;
    }
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');

    return -1;
}

static int parse_hex_colour(const char *value, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i;
    int n[6];

    if (!value) return -1;
    if (value[0] == '#') value++;

    for (i = 0; i < 6; i++) {
        n[i] = hex_nibble(value[i]);
        if (n[i] < 0) return -1;
    }

    if (value[6] != '\0') return -1;

    *r = (uint8_t) ((n[0] << 4) | n[1]);
    *g = (uint8_t) ((n[2] << 4) | n[3]);
    *b = (uint8_t) ((n[4] << 4) | n[5]);

    return 0;
}

static int parse_gradient(const char *value, gradient_t *grad) {
    const char *colon;

    char top[8];
    size_t top_len;

    uint8_t tr;
    uint8_t tg;
    uint8_t tb;
    uint8_t br;
    uint8_t bg;
    uint8_t bb;

    if (!value) return -1;

    colon = strchr(value, ':');
    if (!colon || colon == value) return -1;

    top_len = (size_t)(colon - value);
    if (top_len >= sizeof(top)) return -1;

    memcpy(top, value, top_len);
    top[top_len] = '\0';

    if (parse_hex_colour(top, &tr, &tg, &tb) != 0) return -1;
    if (parse_hex_colour(colon + 1, &br, &bg, &bb) != 0) return -1;

    grad->enabled = 1;

    grad->top_r = tr;
    grad->top_g = tg;
    grad->top_b = tb;

    grad->bot_r = br;
    grad->bot_g = bg;
    grad->bot_b = bb;

    return 0;
}

static int parse_alpha(const char *value, int *out) {
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(value, &end, 10);

    if (errno != 0 || !end || *end != '\0') return -1;

    if (v < 0) v = 0;
    if (v > 100) v = 100;

    *out = (int) v;
    return 0;
}

static int option_takes_value(const char *arg) {
    return strcmp(arg, "-i") == 0 ||
           strcmp(arg, "--image") == 0 ||
           strcmp(arg, "-f") == 0 ||
           strcmp(arg, "--fb") == 0 ||
           strcmp(arg, "-r") == 0 ||
           strcmp(arg, "--rotate") == 0 ||
           strcmp(arg, "-s") == 0 ||
           strcmp(arg, "--scale") == 0 ||
           strcmp(arg, "-g") == 0 ||
           strcmp(arg, "--gradient") == 0 ||
           strcmp(arg, "-t") == 0 ||
           strcmp(arg, "--tint") == 0 ||
           strcmp(arg, "--recolour") == 0 ||
           strcmp(arg, "--recolor") == 0 ||
           strcmp(arg, "-a") == 0 ||
           strcmp(arg, "--alpha") == 0;
}

static int parse_args(int argc, char *argv[], options_t *opts) {
    int i;

    opts->image_path = NULL;
    opts->fb_path = "/dev/fb0";
    opts->rotate = 0;
    opts->scale = SCALE_KEEP;
    opts->wait = 0;
    opts->verbose = 0;
    opts->clear = 0;

    memset(&opts->gradient, 0, sizeof(opts->gradient));

    opts->recolour_enabled = 0;
    opts->recolour_r = 0xFF;
    opts->recolour_g = 0xFF;
    opts->recolour_b = 0xFF;
    opts->recolour_alpha = 0;

    if (argc == 2 && argv[1][0] != '-') {
        opts->image_path = argv[1];

        return 0;
    }

    if (argc == 3 && argv[1][0] != '-' && argv[2][0] != '-') {
        opts->image_path = argv[1];
        opts->fb_path = argv[2];

        return 0;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }

        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = 1;
            continue;
        }

        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--wait") == 0) {
            opts->wait = 1;
            continue;
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--clear") == 0) {
            opts->clear = 1;
            continue;
        }

        if (option_takes_value(argv[i]) && i + 1 >= argc) return -1;

        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--image") == 0) {
            opts->image_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fb") == 0) {
            opts->fb_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rotate") == 0) {
            if (parse_rotation(argv[++i], &opts->rotate) != 0) return -1;
            continue;
        }

        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scale") == 0) {
            if (parse_scale(argv[++i], &opts->scale) != 0) return -1;
            continue;
        }

        if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gradient") == 0) {
            if (parse_gradient(argv[++i], &opts->gradient) != 0) return -1;
            continue;
        }

        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tint") == 0 || strcmp(argv[i], "--recolour") == 0 || strcmp(argv[i], "--recolor") == 0) {
            if (parse_hex_colour(argv[++i], &opts->recolour_r, &opts->recolour_g, &opts->recolour_b) != 0) return -1;
            opts->recolour_enabled = 1;
            continue;
        }

        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--alpha") == 0) {
            if (parse_alpha(argv[++i], &opts->recolour_alpha) != 0) return -1;
            continue;
        }

        return -1;
    }

    if (opts->recolour_alpha <= 0) {
        opts->recolour_enabled = 0;
        opts->recolour_alpha = 0;
    }

    if (opts->clear) return 0;

    return opts->image_path ? 0 : -1;
}

static void png_read_from_memory(png_structp png_ptr, png_bytep out, png_size_t count) {
    png_mem_t *mem = (png_mem_t *) png_get_io_ptr(png_ptr);
    if (!mem || mem->offset + count > mem->size) png_error(png_ptr, "PNG read beyond end of buffer");

    memcpy(out, mem->data + mem->offset, count);
    mem->offset += count;
}

static int read_file(const char *path, uint8_t **out_buf, size_t *out_size) {
    FILE *fp;
    long size;
    uint8_t *buf;

    *out_buf = NULL;
    *out_size = 0;

    fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);

        return -1;
    }

    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);

        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);

        return -1;
    }

    buf = malloc((size_t) size);
    if (!buf) {
        fclose(fp);

        return -1;
    }

    if (fread(buf, 1, (size_t) size, fp) != (size_t) size) {
        free(buf);
        fclose(fp);

        return -1;
    }

    fclose(fp);

    *out_buf = buf;
    *out_size = (size_t) size;

    return 0;
}

static int load_png_to_rgba(const char *path, image_t *img) {
    uint8_t *file_buf = NULL;
    size_t file_size = 0;
    png_mem_t mem;

    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *rows = NULL;

    png_uint_32 width;
    png_uint_32 height;

    int bit_depth;
    int colour_type;
    int interlace_type;

    int y;

    memset(img, 0, sizeof(*img));

    if (read_file(path, &file_buf, &file_size) != 0) return -1;

    if (file_size < 8 || png_sig_cmp(file_buf, 0, 8) != 0) {
        free(file_buf);

        return -1;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        free(file_buf);

        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        free(file_buf);

        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        free(rows);
        free(img->pixels);

        img->pixels = NULL;
        img->width = 0;
        img->height = 0;

        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(file_buf);

        return -1;
    }

    mem.data = file_buf;
    mem.size = file_size;
    mem.offset = 0;

    png_set_read_fn(png_ptr, &mem, png_read_from_memory);
    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &colour_type, &interlace_type, NULL, NULL);
    (void) interlace_type;

    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (colour_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (colour_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
    if (colour_type == PNG_COLOR_TYPE_GRAY || colour_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png_ptr);
    if (!(colour_type & PNG_COLOR_MASK_ALPHA)) png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png_ptr, info_ptr);

    img->width = (int) width;
    img->height = (int) height;
    img->pixels = malloc((size_t) img->width * (size_t) img->height * 4);

    if (!img->pixels) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(file_buf);
        return -1;
    }

    rows = malloc((size_t) img->height * sizeof(*rows));
    if (!rows) {
        free(img->pixels);
        img->pixels = NULL;

        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        free(file_buf);
        return -1;
    }

    for (y = 0; y < img->height; y++) rows[y] = img->pixels + ((size_t) y * (size_t) img->width * 4);

    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, NULL);

    free(rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(file_buf);

    return 0;
}

static int open_fb(const char *path, fb_t *fb) {
    size_t visible_offset;

    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;

    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0) return -1;

    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fix) < 0) {
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->var) < 0) {
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    fb->width = (int) fb->var.xres;
    fb->height = (int) fb->var.yres;
    fb->bpp = (int) fb->var.bits_per_pixel;
    fb->stride = (int) fb->fix.line_length;
    fb->mem_size = fb->fix.smem_len;

    if (fb->bpp != 16 && fb->bpp != 24 && fb->bpp != 32) {
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    fb->bytes_per_pixel = fb->bpp / 8;

    fb->mem = mmap(NULL, fb->mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (fb->mem == MAP_FAILED) {
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    visible_offset = (size_t) fb->var.yoffset * (size_t) fb->stride + (size_t) fb->var.xoffset * (size_t) fb->bytes_per_pixel;
    if (visible_offset >= fb->mem_size) {
        munmap(fb->mem, fb->mem_size);
        close(fb->fd);
        fb->fd = -1;
        fb->mem = NULL;
        return -1;
    }

    fb->base = fb->mem + visible_offset;

    return 0;
}

static void close_fb(fb_t *fb) {
    if (fb->mem && fb->mem != MAP_FAILED) {
        msync(fb->mem, fb->mem_size, MS_SYNC);
        munmap(fb->mem, fb->mem_size);
    }

    if (fb->fd >= 0) close(fb->fd);

    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;
}

static int unblank_fb(const char *fbdev) {
    char path[256];
    const char *name;
    const char *slash;
    int fd;

    slash = strrchr(fbdev, '/');
    name = slash ? slash + 1 : fbdev;

    snprintf(path, sizeof(path), "/sys/class/graphics/%s/blank", name);

    fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    if (write(fd, "0\n", 2) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static uint32_t pack_pixel(const fb_t *fb, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t pixel = 0;
    uint32_t max;

    uint32_t rv = 0;
    uint32_t gv = 0;
    uint32_t bv = 0;
    uint32_t av = 0;

    if (fb->var.red.length) {
        max = (1u << fb->var.red.length) - 1u;
        rv = ((uint32_t) r * max + 127u) / 255u;
    }

    if (fb->var.green.length) {
        max = (1u << fb->var.green.length) - 1u;
        gv = ((uint32_t) g * max + 127u) / 255u;
    }

    if (fb->var.blue.length) {
        max = (1u << fb->var.blue.length) - 1u;
        bv = ((uint32_t) b * max + 127u) / 255u;
    }

    if (fb->var.transp.length) av = (1u << fb->var.transp.length) - 1u;

    pixel |= rv << fb->var.red.offset;
    pixel |= gv << fb->var.green.offset;
    pixel |= bv << fb->var.blue.offset;
    pixel |= av << fb->var.transp.offset;

    return pixel;
}

static void put_pixel(const fb_t *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t *dst;
    uint32_t pixel;
    uint16_t pixel16;

    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return;

    dst = fb->base + (size_t) y * (size_t) fb->stride + (size_t) x * (size_t) fb->bytes_per_pixel;
    pixel = pack_pixel(fb, r, g, b);

    switch (fb->bpp) {
        case 16:
            pixel16 = (uint16_t) pixel;
            memcpy(dst, &pixel16, sizeof(pixel16));
            break;
        case 24:
            dst[0] = (uint8_t) (pixel & 0xFF);
            dst[1] = (uint8_t) ((pixel >> 8) & 0xFF);
            dst[2] = (uint8_t) ((pixel >> 16) & 0xFF);
            break;
        case 32:
            memcpy(dst, &pixel, sizeof(pixel));
            break;
        default:
            break;
    }
}

static void clear_fb(const fb_t *fb, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t pixel;
    uint16_t pixel16;

    int x;
    int y;

    pixel = pack_pixel(fb, r, g, b);

    if (fb->bpp == 32) {
        for (y = 0; y < fb->height; y++) {
            uint32_t *row = (uint32_t *) (fb->base + (size_t) y * (size_t) fb->stride);

            for (x = 0; x < fb->width; x++) row[x] = pixel;
        }

        return;
    }

    if (fb->bpp == 16) {
        pixel16 = (uint16_t) pixel;

        for (y = 0; y < fb->height; y++) {
            uint16_t *row = (uint16_t *) (fb->base + (size_t) y * (size_t) fb->stride);

            for (x = 0; x < fb->width; x++) row[x] = pixel16;
        }

        return;
    }

    for (y = 0; y < fb->height; y++) {
        uint8_t *row = fb->base + (size_t) y * (size_t) fb->stride;

        for (x = 0; x < fb->width; x++) {
            row[(size_t) x * 3 + 0] = (uint8_t) (pixel & 0xFF);
            row[(size_t) x * 3 + 1] = (uint8_t) ((pixel >> 8) & 0xFF);
            row[(size_t) x * 3 + 2] = (uint8_t) ((pixel >> 16) & 0xFF);
        }
    }
}

static void gradient_colour_at(const gradient_t *grad, int t_in, int span_in, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
    int span = span_in > 1 ? span_in - 1 : 1;

    uint32_t s = (uint32_t) span;
    uint32_t t = (uint32_t) (t_in < 0 ? 0 : (t_in > span ? span : t_in));

    *out_r = (uint8_t) (((uint32_t) grad->top_r * (s - t) + (uint32_t) grad->bot_r * t) / s);
    *out_g = (uint8_t) (((uint32_t) grad->top_g * (s - t) + (uint32_t) grad->bot_g * t) / s);
    *out_b = (uint8_t) (((uint32_t) grad->top_b * (s - t) + (uint32_t) grad->bot_b * t) / s);
}

static void gradient_axis(int rotate, const fb_t *fb, int *axis, int *span, int *flip) {
    switch (rotate) {
        case 90:
            *axis = 1;
            *span = fb->width;
            *flip = 1;
            break;
        case 180:
            *axis = 0;
            *span = fb->height;
            *flip = 1;
            break;
        case 270:
            *axis = 1;
            *span = fb->width;
            *flip = 0;
            break;
        case 0:
        default:
            *axis = 0;
            *span = fb->height;
            *flip = 0;
            break;
    }
}

static int fill_fb_with_gradient(const fb_t *fb, const gradient_t *grad, int rotate) {
    int axis;
    int span;
    int flip;

    int x;
    int y;

    uint8_t r;
    uint8_t g;
    uint8_t b;

    uint32_t pixel;
    uint16_t pixel16;

    gradient_axis(rotate, fb, &axis, &span, &flip);

    if (axis == 0) {
        for (y = 0; y < fb->height; y++) {
            int t = flip ? (fb->height - 1 - y) : y;

            gradient_colour_at(grad, t, span, &r, &g, &b);
            pixel = pack_pixel(fb, r, g, b);

            if (fb->bpp == 32) {
                uint32_t *row = (uint32_t *) (fb->base + (size_t) y * (size_t) fb->stride);

                for (x = 0; x < fb->width; x++) row[x] = pixel;
                continue;
            }

            if (fb->bpp == 16) {
                uint16_t *row = (uint16_t *) (fb->base + (size_t) y * (size_t) fb->stride);

                pixel16 = (uint16_t) pixel;

                for (x = 0; x < fb->width; x++) row[x] = pixel16;
                continue;
            }

            {
                uint8_t *row = fb->base + (size_t) y * (size_t) fb->stride;

                uint8_t b0 = (uint8_t) (pixel & 0xFF);
                uint8_t b1 = (uint8_t) ((pixel >> 8) & 0xFF);
                uint8_t b2 = (uint8_t) ((pixel >> 16) & 0xFF);

                for (x = 0; x < fb->width; x++) {
                    row[(size_t) x * 3 + 0] = b0;
                    row[(size_t) x * 3 + 1] = b1;
                    row[(size_t) x * 3 + 2] = b2;
                }
            }
        }

        return 0;
    }

    if (fb->bpp == 32) {
        uint32_t *col_pixels32 = malloc((size_t) fb->width * sizeof(*col_pixels32));
        if (!col_pixels32) return -1;

        for (x = 0; x < fb->width; x++) {
            int t = flip ? (fb->width - 1 - x) : x;
            gradient_colour_at(grad, t, span, &r, &g, &b);
            col_pixels32[x] = pack_pixel(fb, r, g, b);
        }

        for (y = 0; y < fb->height; y++) {
            uint32_t *row = (uint32_t *) (fb->base + (size_t) y * (size_t) fb->stride);
            memcpy(row, col_pixels32, (size_t) fb->width * sizeof(*col_pixels32));
        }

        free(col_pixels32);
        return 0;
    }

    if (fb->bpp == 16) {
        uint16_t *col_pixels16 = malloc((size_t) fb->width * sizeof(*col_pixels16));
        if (!col_pixels16) return -1;

        for (x = 0; x < fb->width; x++) {
            int t = flip ? (fb->width - 1 - x) : x;
            gradient_colour_at(grad, t, span, &r, &g, &b);
            col_pixels16[x] = (uint16_t) pack_pixel(fb, r, g, b);
        }

        for (y = 0; y < fb->height; y++) {
            uint16_t *row = (uint16_t *) (fb->base + (size_t) y * (size_t) fb->stride);
            memcpy(row, col_pixels16, (size_t) fb->width * sizeof(*col_pixels16));
        }

        free(col_pixels16);
        return 0;
    }

    {
        uint8_t *col_pixels24 = malloc((size_t) fb->width * 3);
        if (!col_pixels24) return -1;

        for (x = 0; x < fb->width; x++) {
            int t = flip ? (fb->width - 1 - x) : x;
            uint32_t pix;

            gradient_colour_at(grad, t, span, &r, &g, &b);
            pix = pack_pixel(fb, r, g, b);

            col_pixels24[(size_t) x * 3 + 0] = (uint8_t) (pix & 0xFF);
            col_pixels24[(size_t) x * 3 + 1] = (uint8_t) ((pix >> 8) & 0xFF);
            col_pixels24[(size_t) x * 3 + 2] = (uint8_t) ((pix >> 16) & 0xFF);
        }

        for (y = 0; y < fb->height; y++) {
            uint8_t *row = fb->base + (size_t) y * (size_t) fb->stride;
            memcpy(row, col_pixels24, (size_t) fb->width * 3);
        }

        free(col_pixels24);
    }

    return 0;
}

static void rotated_size(const image_t *img, int rotate, int *out_w, int *out_h) {
    if (rotate == 90 || rotate == 270) {
        *out_w = img->height;
        *out_h = img->width;

        return;
    }

    *out_w = img->width;
    *out_h = img->height;
}

static void rotated_to_source(const image_t *img, int rotate, int rx, int ry, int *sx, int *sy) {
    switch (rotate) {
        case 90:
            *sx = ry;
            *sy = img->height - 1 - rx;
            break;
        case 180:
            *sx = img->width - 1 - rx;
            *sy = img->height - 1 - ry;
            break;
        case 270:
            *sx = img->width - 1 - ry;
            *sy = rx;
            break;
        case 0:
        default:
            *sx = rx;
            *sy = ry;
            break;
    }
}

static void calculate_dest_rect(const fb_t *fb, int src_w, int src_h, scale_mode_t scale, int *dst_x, int *dst_y, int *dst_w, int *dst_h) {
    int w;
    int h;

    switch (scale) {
        case SCALE_ORIGINAL:
            w = src_w;
            h = src_h;
            break;
        case SCALE_STRETCH:
            w = fb->width;
            h = fb->height;
            break;
        case SCALE_FILL:
            if ((long long) fb->width * (long long) src_h >=
                (long long) fb->height * (long long) src_w) {
                w = fb->width;
                h = (src_h * fb->width) / src_w;
            } else {
                h = fb->height;
                w = (src_w * fb->height) / src_h;
            }
            break;
        case SCALE_KEEP:
        default:
            if ((long long) fb->width * (long long) src_h <=
                (long long) fb->height * (long long) src_w) {
                w = fb->width;
                h = (src_h * fb->width) / src_w;
            } else {
                h = fb->height;
                w = (src_w * fb->height) / src_h;
            }
            break;
    }

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    *dst_w = w;
    *dst_h = h;

    *dst_x = (fb->width - w) / 2;
    *dst_y = (fb->height - h) / 2;
}

static void write_fb_pixel(const fb_t *fb, int x, int y, const uint8_t *src, uint8_t bg_r, uint8_t bg_g, uint8_t bg_b) {
    uint8_t sr = src[0];
    uint8_t sg = src[1];
    uint8_t sb = src[2];
    uint8_t sa = src[3];
    uint8_t dr;
    uint8_t dg;
    uint8_t db;
    uint32_t inv;

    if (sa == 0xFF) {
        put_pixel(fb, x, y, sr, sg, sb);

        return;
    }

    if (sa == 0x00) {
        put_pixel(fb, x, y, bg_r, bg_g, bg_b);

        return;
    }

    inv = 255u - (uint32_t) sa;

    dr = (uint8_t) (((uint32_t) sr * sa + (uint32_t) bg_r * inv + 127u) / 255u);
    dg = (uint8_t) (((uint32_t) sg * sa + (uint32_t) bg_g * inv + 127u) / 255u);
    db = (uint8_t) (((uint32_t) sb * sa + (uint32_t) bg_b * inv + 127u) / 255u);

    put_pixel(fb, x, y, dr, dg, db);
}

static void build_recolour_lut(const options_t *opts, uint8_t lut_r[256], uint8_t lut_g[256], uint8_t lut_b[256]) {
    uint32_t a = (uint32_t) opts->recolour_alpha;
    uint32_t inv = 100u - a;

    uint32_t tr = (uint32_t) opts->recolour_r;
    uint32_t tg = (uint32_t) opts->recolour_g;
    uint32_t tb = (uint32_t) opts->recolour_b;

    int i;

    for (i = 0; i < 256; i++) {
        lut_r[i] = (uint8_t) (((uint32_t) i * inv + tr * a + 50u) / 100u);
        lut_g[i] = (uint8_t) (((uint32_t) i * inv + tg * a + 50u) / 100u);
        lut_b[i] = (uint8_t) (((uint32_t) i * inv + tb * a + 50u) / 100u);
    }
}

static void draw_image(const fb_t *fb, const image_t *img, const options_t *opts) {
    int rotate = opts->rotate;
    scale_mode_t scale = opts->scale;
    const gradient_t *grad = &opts->gradient;
    int recolour = opts->recolour_enabled;

    uint8_t lut_r[256];
    uint8_t lut_g[256];
    uint8_t lut_b[256];

    uint8_t *bg_lut = NULL;

    int bg_axis = 0;

    int rot_w;
    int rot_h;

    int dst_x;
    int dst_y;
    int dst_w;
    int dst_h;

    int screen_x0;
    int screen_y0;

    int screen_x1;
    int screen_y1;

    int x;
    int y;

    if (!img->pixels || img->width <= 0 || img->height <= 0) return;

    rotated_size(img, rotate, &rot_w, &rot_h);
    calculate_dest_rect(fb, rot_w, rot_h, scale, &dst_x, &dst_y, &dst_w, &dst_h);

    if (grad->enabled) {
        if (fill_fb_with_gradient(fb, grad, rotate) != 0) clear_fb(fb, grad->top_r, grad->top_g, grad->top_b);
    } else {
        clear_fb(fb, 0, 0, 0);
    }

    if (recolour) build_recolour_lut(opts, lut_r, lut_g, lut_b);

    if (grad->enabled) {
        int axis;
        int span;
        int flip;

        int i;
        int n;

        gradient_axis(rotate, fb, &axis, &span, &flip);
        bg_axis = axis;
        n = (axis == 0) ? fb->height : fb->width;

        bg_lut = malloc((size_t) n * 3);

        if (!bg_lut) {
            clear_fb(fb, grad->top_r, grad->top_g, grad->top_b);
        } else {
            for (i = 0; i < n; i++) {
                int t = flip ? (n - 1 - i) : i;
                gradient_colour_at(grad, t, span, &bg_lut[(size_t) i * 3 + 0], &bg_lut[(size_t) i * 3 + 1], &bg_lut[(size_t) i * 3 + 2]);
            }
        }
    }

    screen_x0 = dst_x < 0 ? 0 : dst_x;
    screen_y0 = dst_y < 0 ? 0 : dst_y;

    screen_x1 = dst_x + dst_w;
    screen_y1 = dst_y + dst_h;

    if (screen_x1 > fb->width) screen_x1 = fb->width;
    if (screen_y1 > fb->height) screen_y1 = fb->height;

    for (y = screen_y0; y < screen_y1; y++) {
        int local_y = y - dst_y;
        int ry = (local_y * rot_h) / dst_h;

        uint8_t row_bg_r = 0;
        uint8_t row_bg_g = 0;
        uint8_t row_bg_b = 0;

        if (ry < 0) ry = 0;
        if (ry >= rot_h) ry = rot_h - 1;

        if (bg_lut && bg_axis == 0) {
            row_bg_r = bg_lut[(size_t) y * 3 + 0];
            row_bg_g = bg_lut[(size_t) y * 3 + 1];
            row_bg_b = bg_lut[(size_t) y * 3 + 2];
        }

        for (x = screen_x0; x < screen_x1; x++) {
            int local_x = x - dst_x;

            int rx = (local_x * rot_w) / dst_w;
            int sx;
            int sy;

            const uint8_t *src;

            uint8_t bg_r = row_bg_r;
            uint8_t bg_g = row_bg_g;
            uint8_t bg_b = row_bg_b;

            uint8_t pixel[4];

            if (rx < 0) rx = 0;
            if (rx >= rot_w) rx = rot_w - 1;

            rotated_to_source(img, rotate, rx, ry, &sx, &sy);

            if (sx < 0 || sx >= img->width || sy < 0 || sy >= img->height) continue;

            if (bg_lut && bg_axis == 1) {
                bg_r = bg_lut[(size_t) x * 3 + 0];
                bg_g = bg_lut[(size_t) x * 3 + 1];
                bg_b = bg_lut[(size_t) x * 3 + 2];
            }

            src = img->pixels + (((size_t) sy * (size_t) img->width + (size_t) sx) * 4);

            if (recolour) {
                pixel[0] = lut_r[src[0]];
                pixel[1] = lut_g[src[1]];
                pixel[2] = lut_b[src[2]];
                pixel[3] = src[3];

                write_fb_pixel(fb, x, y, pixel, bg_r, bg_g, bg_b);
            } else {
                write_fb_pixel(fb, x, y, src, bg_r, bg_g, bg_b);
            }
        }
    }

    free(bg_lut);

    msync(fb->mem, fb->mem_size, MS_SYNC);
}

static void free_image(image_t *img) {
    free(img->pixels);

    img->pixels = NULL;
    img->width = 0;
    img->height = 0;
}

static int run_clear(const options_t *opts) {
    fb_t fb;

    memset(&fb, 0, sizeof(fb));
    fb.fd = -1;

    log_fmt(opts, "Opening framebuffer: %s\n", opts->fb_path);

    if (open_fb(opts->fb_path, &fb) != 0) {
        log_fmt(opts, "Failed to open framebuffer: %s\n", opts->fb_path);

        return 1;
    }

    unblank_fb(opts->fb_path);

    log_msg(opts, "Clearing framebuffer");

    clear_fb(&fb, 0, 0, 0);
    msync(fb.mem, fb.mem_size, MS_SYNC);

    unblank_fb(opts->fb_path);
    close_fb(&fb);

    log_msg(opts, "Done");

    return 0;
}

static int run_draw(const options_t *opts) {
    image_t img;
    fb_t fb;

    memset(&img, 0, sizeof(img));
    memset(&fb, 0, sizeof(fb));
    fb.fd = -1;

    log_fmt(opts, "Loading image: %s\n", opts->image_path);

    if (load_png_to_rgba(opts->image_path, &img) != 0) {
        log_fmt(opts, "Failed to load PNG: %s\n", opts->image_path);

        return 1;
    }

    log_fmt(opts, "Opening framebuffer: %s\n", opts->fb_path);

    if (open_fb(opts->fb_path, &fb) != 0) {
        log_fmt(opts, "Failed to open framebuffer: %s\n", opts->fb_path);
        free_image(&img);

        return 1;
    }

    unblank_fb(opts->fb_path);

    if (opts->verbose) {
        fprintf(stderr, "Drawing image=%s fb=%s screen=%dx%d bpp=%d rotate=%d scale=%s\n",
                opts->image_path, opts->fb_path, fb.width, fb.height, fb.bpp, opts->rotate, scale_name(opts->scale));

        if (opts->gradient.enabled) {
            fprintf(stderr, "Gradient: top=%02X%02X%02X bottom=%02X%02X%02X\n",
                    opts->gradient.top_r, opts->gradient.top_g, opts->gradient.top_b,
                    opts->gradient.bot_r, opts->gradient.bot_g, opts->gradient.bot_b);
        }

        if (opts->recolour_enabled) {
            fprintf(stderr, "Recolour: tint=%02X%02X%02X alpha=%d%%\n",
                    opts->recolour_r, opts->recolour_g, opts->recolour_b, opts->recolour_alpha);
        }
    }

    draw_image(&fb, &img, opts);

    unblank_fb(opts->fb_path);

    if (opts->wait) {
        log_msg(opts, "Waiting until killed");
        while (keep_running) pause();
    }

    close_fb(&fb);
    free_image(&img);

    log_msg(opts, "Done");

    return 0;
}

int main(int argc, char *argv[]) {
    options_t opts;

    if (parse_args(argc, argv, &opts) != 0) return 1;

    setup_signals();

    if (opts.clear) return run_clear(&opts);

    return run_draw(&opts);
}
