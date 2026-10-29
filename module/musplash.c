#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <png.h>
#include <poll.h>
#include <strings.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Where the display engine says it is reading pixels from
#define SCANOUT_PATH "/sys/class/disp/disp/attr/sys"

#define DURATION_FINISH (-1)
#define DURATION_PRESS  (-2)

#define FINISH_STILL_SECONDS 2

typedef enum { scale_original = 0, scale_keep = 1, scale_stretch = 2, scale_fill = 3 } scale_mode_t;

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
    uint8_t *scratch;
    size_t mem_size;
    size_t page_size;
    int page_count;
    int buffered;

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
    const char *ready_path;
    const char *log_path;

    int rotate;
    scale_mode_t scale;

    int wait;
    int verbose;
    int clear;
    int play;
    int duration;
    int fade;
    gradient_t gradient;

    int recolour_enabled;
    uint8_t recolour_r;
    uint8_t recolour_g;
    uint8_t recolour_b;

    int recolour_alpha;
} options_t;

static volatile sig_atomic_t keep_running = 1;

static FILE *trace_file = NULL;

// Keep the file small, a shutdown only ever adds a handful of lines
static void trace_open(const options_t *opts) {
    if (!opts->log_path || !*opts->log_path) return;

    const char *mode = "a";
    struct stat info;

    if (stat(opts->log_path, &info) == 0 && info.st_size > 262144) mode = "w";

    trace_file = fopen(opts->log_path, mode);
    if (trace_file) setvbuf(trace_file, NULL, _IOLBF, 0);
}

static void trace_close(void) {
    if (!trace_file) return;

    fclose(trace_file);
    trace_file = NULL;
}

__attribute__((format(printf, 1, 2))) static void trace(const char *fmt, ...) {
    if (!trace_file) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    fprintf(trace_file, "[%ld.%03ld] ", (long) now.tv_sec, now.tv_nsec / 1000000L);

    va_list args;
    va_start(args, fmt);
    vfprintf(trace_file, fmt, args);
    va_end(args);

    fputc('\n', trace_file);
}

// Ask the display engine which address it is actually scanning out
static unsigned long read_scanout_addr(void) {
    char buffer[8192];

    const int fd = open(SCANOUT_PATH, O_RDONLY);
    if (fd < 0) return 0;

    const ssize_t got = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (got <= 0) return 0;
    buffer[got] = '\0';

    const char *found = strstr(buffer, "addr[");
    if (!found) return 0;

    return strtoul(found + 5, NULL, 16);
}

static void log_msg(const options_t *opts, const char *msg) {
    if (opts->verbose) fprintf(stderr, "%s\n", msg);
}

static void log_value(const options_t *opts, const char *message, const char *value) {
    if (opts->verbose) fprintf(stderr, "%s%s\n", message, value);
}

static void handle_signal(const int sig) {
    (void) sig;
    keep_running = 0;
}

static void setup_signals(void) {
    struct sigaction sa = {0};

    sa.sa_handler = handle_signal;

    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void print_usage(const char *name) {
    fprintf(
        stderr,
        "Usage:\n"
        "  %s -i <image.png> [options]\n"
        "  %s --image <image.png> [options]\n"
        "  %s -c [options]\n"
        "  %s <image.png> [fbdev]\n\n"
        "Options:\n"
        "  -i, --image <path>          Image, animation or video path\n"
        "  -p, --play                  Play animations and videos through, any button skips\n"
        "  -d, --duration <value>      Seconds to keep the image up, finish (animated media plays once,\n"
        "                              stills show for two seconds) or press (wait for a button)\n"
        "  -F, --fade <ms>             Fade in from black and out to black over this many milliseconds\n"
        "  -f, --fb <path>             Framebuffer device, default: /dev/fb0\n"
        "  -r, --rotate <0|90|180|270> Rotation, default: 0\n"
        "  -s, --scale <mode>          0/original | 1/keep | 2/stretch | 3/fill, default: keep\n"
        "  -g, --gradient <A:B>        Vertical background gradient, top:bottom\n"
        "                              e.g. 000000:FFFFFF or #1A1A2E:#16213E\n"
        "  -t, --tint <RRGGBB>         PNG recolour (tint), e.g. FFFFFF or #FFFFFF\n"
        "  -a, --alpha <0-100>         Recolour strength as a percentage, default: 0 (off)\n"
        "  -c, --clear                 Clear the framebuffer to black and exit\n"
        "  -n, --notify <path>         Create a file after the frame is presented\n"
        "  -l, --log <path>            Append what was drawn, and where, to a file\n"
        "  -w, --wait                  Keep running until killed\n"
        "  -v, --verbose               Enable log output\n"
        "  -h, --help                  Show this help\n\n"
        "Scale modes:\n"
        "  0, original                 Draw at original size, centred\n"
        "  1, keep                     Keep aspect ratio and fit inside the screen\n"
        "  2, stretch                  Stretch to the full screen\n"
        "  3, fill                     Keep aspect ratio, fill screen, crop overflow\n",
        name, name, name, name
    );
}

static const char *scale_name(const scale_mode_t scale) {
    switch (scale) {
        case scale_original:
            return "original";
        case scale_keep:
            return "keep";
        case scale_stretch:
            return "stretch";
        case scale_fill:
            return "fill";
        default:
            return "unknown";
    }
}

static int parse_scale(const char *value, scale_mode_t *scale) {
    if (strcmp(value, "0") == 0 || strcmp(value, "original") == 0 || strcmp(value, "orig") == 0) {
        *scale = scale_original;
        return 0;
    }

    if (strcmp(value, "1") == 0 || strcmp(value, "keep") == 0 || strcmp(value, "aspect") == 0
        || strcmp(value, "contain") == 0) {
        *scale = scale_keep;
        return 0;
    }

    if (strcmp(value, "2") == 0 || strcmp(value, "stretch") == 0) {
        *scale = scale_stretch;
        return 0;
    }

    if (strcmp(value, "3") == 0 || strcmp(value, "fill") == 0 || strcmp(value, "cover") == 0) {
        *scale = scale_fill;
        return 0;
    }

    return -1;
}

static int parse_rotation(const char *value, int *rotate) {
    char *end = NULL;

    errno = 0;
    const long r = strtol(value, &end, 10);

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

static int hex_nibble(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');

    return -1;
}

static int parse_hex_colour(const char *value, uint8_t *r, uint8_t *g, uint8_t *b) {
    int n[6];

    if (!value) return -1;
    if (value[0] == '#') value++;

    for (int i = 0; i < 6; i++) {
        n[i] = hex_nibble(value[i]);
        if (n[i] < 0) return -1;
    }

    if (value[6] != '\0') return -1;

    *r = (uint8_t) (n[0] << 4 | n[1]);
    *g = (uint8_t) (n[2] << 4 | n[3]);
    *b = (uint8_t) (n[4] << 4 | n[5]);

    return 0;
}

static int parse_gradient(const char *value, gradient_t *grad) {
    char top[8];

    uint8_t tr;
    uint8_t tg;
    uint8_t tb;
    uint8_t br;
    uint8_t bg;
    uint8_t bb;

    if (!value) return -1;

    const char *colon = strchr(value, ':');
    if (!colon || colon == value) return -1;

    const size_t top_len = (size_t) (colon - value);
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

    errno = 0;
    long v = strtol(value, &end, 10);

    if (errno != 0 || !end || *end != '\0') return -1;

    if (v < 0) v = 0;
    if (v > 100) v = 100;

    *out = (int) v;
    return 0;
}

static int parse_duration(const char *value, int *out) {
    if (strcmp(value, "finish") == 0) {
        *out = DURATION_FINISH;
        return 0;
    }

    if (strcmp(value, "press") == 0) {
        *out = DURATION_PRESS;
        return 0;
    }

    char *end = NULL;

    errno = 0;
    long v = strtol(value, &end, 10);

    if (errno != 0 || !end || *end != '\0') return -1;

    if (v < 0) v = 0;
    if (v > 60) v = 60;

    *out = (int) v;
    return 0;
}

static int option_takes_value(const char *arg) {
    return strcmp(arg, "-i") == 0 || strcmp(arg, "--image") == 0 || strcmp(arg, "-f") == 0 || strcmp(arg, "--fb") == 0
           || strcmp(arg, "-r") == 0 || strcmp(arg, "--rotate") == 0 || strcmp(arg, "-s") == 0
           || strcmp(arg, "--scale") == 0 || strcmp(arg, "-g") == 0 || strcmp(arg, "--gradient") == 0
           || strcmp(arg, "-t") == 0 || strcmp(arg, "--tint") == 0 || strcmp(arg, "--recolour") == 0
           || strcmp(arg, "--recolor") == 0 || strcmp(arg, "-a") == 0 || strcmp(arg, "--alpha") == 0
           || strcmp(arg, "-n") == 0 || strcmp(arg, "--notify") == 0 || strcmp(arg, "-l") == 0
           || strcmp(arg, "--log") == 0 || strcmp(arg, "-d") == 0 || strcmp(arg, "--duration") == 0
           || strcmp(arg, "-F") == 0 || strcmp(arg, "--fade") == 0;
}

static int parse_fade(const char *value, int *out) {
    char *end = NULL;

    errno = 0;
    long v = strtol(value, &end, 10);

    if (errno != 0 || !end || *end != '\0') return -1;

    if (v < 0) v = 0;
    if (v > 2000) v = 2000;

    *out = (int) v;
    return 0;
}

static int parse_args(const int argc, char *argv[], options_t *opts) {

    opts->image_path = NULL;
    opts->fb_path = "/dev/fb0";
    opts->ready_path = NULL;
    opts->log_path = NULL;
    opts->rotate = 0;
    opts->scale = scale_keep;
    opts->wait = 0;
    opts->verbose = 0;
    opts->clear = 0;
    opts->play = 0;
    opts->duration = 0;
    opts->fade = 0;
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

    for (int i = 1; i < argc; i++) {
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

        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--play") == 0) {
            opts->play = 1;
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

        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--notify") == 0) {
            opts->ready_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) {
            opts->log_path = argv[++i];
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

        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tint") == 0 || strcmp(argv[i], "--recolour") == 0
            || strcmp(argv[i], "--recolor") == 0) {
            if (parse_hex_colour(argv[++i], &opts->recolour_r, &opts->recolour_g, &opts->recolour_b) != 0) return -1;
            opts->recolour_enabled = 1;
            continue;
        }

        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--alpha") == 0) {
            if (parse_alpha(argv[++i], &opts->recolour_alpha) != 0) return -1;
            continue;
        }

        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--duration") == 0) {
            if (parse_duration(argv[++i], &opts->duration) != 0) return -1;
            continue;
        }

        if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--fade") == 0) {
            if (parse_fade(argv[++i], &opts->fade) != 0) return -1;
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

static void png_read_from_memory(const png_struct *const png_ptr, png_byte *const out, const size_t count) {
    png_mem_t *mem = png_get_io_ptr(png_ptr);
    if (!mem || mem->offset + count > mem->size) png_error(png_ptr, "PNG read beyond end of buffer");

    memcpy(out, mem->data + mem->offset, count);
    mem->offset += count;
}

static int read_file(const char *path, uint8_t **out_buf, size_t *out_size) {

    *out_buf = NULL;
    *out_size = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);

        return -1;
    }

    const long size = ftell(fp);
    if (size <= 0) {
        fclose(fp);

        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);

        return -1;
    }

    uint8_t *buf = malloc((size_t) size);
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

    png_set_read_fn(png_ptr, &mem, (png_rw_ptr) png_read_from_memory);
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

    for (int y = 0; y < img->height; y++)
        rows[y] = img->pixels + (size_t) y * (size_t) img->width * 4;

    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, NULL);

    free(rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(file_buf);

    return 0;
}

// Most displays here are double buffered, so a few pages is plenty to cover
#define MAX_PAGES 4

static void count_pages(fb_t *fb) {
    fb->page_size = (size_t) fb->stride * (size_t) fb->height;
    fb->page_count = 1;

    if (fb->page_size == 0) return;

    size_t pages = fb->mem_size / fb->page_size;

    if (fb->var.yres > 0 && fb->var.yres_virtual >= fb->var.yres) {
        const size_t virtual_pages = fb->var.yres_virtual / fb->var.yres;
        if (virtual_pages < pages) pages = virtual_pages;
    }

    if (pages > MAX_PAGES) pages = MAX_PAGES;
    if (pages < 1) pages = 1;

    fb->page_count = (int) pages;
}

// The panel can be parked on any page, so give every page the same picture
static void mirror_pages(const fb_t *fb) {
    if (fb->page_count < 2 || fb->page_size == 0) return;

    for (int page = 0; page < fb->page_count; page++) {
        const size_t offset = (size_t) page * fb->page_size;
        if (offset + fb->page_size > fb->mem_size) break;

        uint8_t *target = fb->mem + offset;
        if (target == fb->base) continue;

        memmove(target, fb->base, fb->page_size);
    }
}

static char fb_error[128];

static int open_fb(const char *path, fb_t *fb) {
    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;

    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0) {
        snprintf(fb_error, sizeof(fb_error), "open: %s", strerror(errno));
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fix) < 0) {
        snprintf(fb_error, sizeof(fb_error), "%s: %s", "FBIOGET_FSCREENINFO", strerror(errno));
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->var) < 0) {
        snprintf(fb_error, sizeof(fb_error), "%s: %s", "FBIOGET_VSCREENINFO", strerror(errno));
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
        snprintf(fb_error, sizeof(fb_error), "unsupported depth of %d bpp", fb->bpp);
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    fb->bytes_per_pixel = fb->bpp / 8;

    count_pages(fb);

    const size_t visible_offset =
        (size_t) fb->var.yoffset * (size_t) fb->stride + (size_t) fb->var.xoffset * (size_t) fb->bytes_per_pixel;
    const size_t visible_size = (size_t) fb->stride * (size_t) fb->height;
    if (visible_offset >= fb->mem_size || visible_size > fb->mem_size - visible_offset) {
        snprintf(
            fb_error, sizeof(fb_error), "visible page %zu+%zu outside %zu bytes", visible_offset, visible_size,
            fb->mem_size
        );
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    fb->buffered = strncmp(fb->fix.id, "rockchip", 8) == 0;
    if (fb->buffered) {
        fb->mem = calloc(1, fb->mem_size);
        if (!fb->mem) {
            snprintf(fb_error, sizeof(fb_error), "no memory for %zu bytes", fb->mem_size);
            close(fb->fd);
            fb->fd = -1;
            return -1;
        }

        size_t offset = 0;
        while (offset < fb->mem_size) {
            const ssize_t read_size = pread(fb->fd, fb->mem + offset, fb->mem_size - offset, (off_t) offset);
            if (read_size > 0) {
                offset += (size_t) read_size;
                continue;
            }
            if (read_size < 0 && errno == EINTR) continue;
            break;
        }
    } else {
        fb->mem = mmap(NULL, fb->mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
        if (fb->mem == MAP_FAILED) {
            snprintf(fb_error, sizeof(fb_error), "mmap: %s", strerror(errno));
            close(fb->fd);
            fb->fd = -1;
            return -1;
        }
    }

    fb->base = fb->mem + visible_offset;

    return 0;
}

static int flush_fb(const fb_t *fb) {
    if (!fb->mem || fb->mem == MAP_FAILED) return -1;

    if (!fb->buffered) return msync(fb->mem, fb->mem_size, MS_SYNC);

    size_t offset = 0;
    while (offset < fb->mem_size) {
        const ssize_t write_size = pwrite(fb->fd, fb->mem + offset, fb->mem_size - offset, (off_t) offset);
        if (write_size > 0) {
            offset += (size_t) write_size;
            continue;
        }
        if (write_size < 0 && errno == EINTR) continue;
        return -1;
    }

    return 0;
}

static void close_fb(fb_t *fb) {
    free(fb->scratch);

    if (fb->mem && fb->mem != MAP_FAILED) {
        if (fb->buffered)
            free(fb->mem);
        else
            munmap(fb->mem, fb->mem_size);
    }

    if (fb->fd >= 0) close(fb->fd);

    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;
}

static int unblank_fb(const char *fbdev) {
    char path[256];

    const char *slash = strrchr(fbdev, '/');
    const char *name = slash ? slash + 1 : fbdev;

    snprintf(path, sizeof(path), "/sys/class/graphics/%s/blank", name);

    const int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    if (write(fd, "0\n", 2) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int refresh_fb_view(fb_t *fb) {
    struct fb_var_screeninfo current;

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &current) < 0) return -1;
    if ((int) current.bits_per_pixel != fb->bpp) return -1;

    const size_t visible_offset =
        (size_t) current.yoffset * (size_t) fb->stride + (size_t) current.xoffset * (size_t) fb->bytes_per_pixel;
    const size_t visible_size = (size_t) fb->stride * (size_t) current.yres;
    if (visible_offset >= fb->mem_size || visible_size > fb->mem_size - visible_offset) return -1;

    const int changed = current.xoffset != fb->var.xoffset || current.yoffset != fb->var.yoffset
                        || current.xres != fb->var.xres || current.yres != fb->var.yres;

    fb->var = current;
    fb->width = (int) current.xres;
    fb->height = (int) current.yres;
    fb->base = fb->mem + visible_offset;

    count_pages(fb);

    return changed;
}

static int notify_ready(const char *path) {
    if (!path || !*path) return 0;

    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return -1;

    const ssize_t written = write(fd, "1\n", 2);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;

    return written == 2 ? 0 : -1;
}

static uint32_t pack_pixel(const fb_t *fb, const uint8_t r, const uint8_t g, const uint8_t b) {
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

static void put_pixel(const fb_t *fb, const int x, const int y, const uint8_t r, const uint8_t g, const uint8_t b) {
    uint32_t pixel;
    uint16_t pixel16;

    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return;

    uint8_t *dst = fb->base + (size_t) y * (size_t) fb->stride + (size_t) x * (size_t) fb->bytes_per_pixel;
    pixel = pack_pixel(fb, r, g, b);

    switch (fb->bpp) {
        case 16:
            pixel16 = (uint16_t) pixel;
            memcpy(dst, &pixel16, sizeof(pixel16));
            break;
        case 24:
            dst[0] = (uint8_t) (pixel & 0xFF);
            dst[1] = (uint8_t) (pixel >> 8 & 0xFF);
            dst[2] = (uint8_t) (pixel >> 16 & 0xFF);
            break;
        case 32:
            memcpy(dst, &pixel, sizeof(pixel));
            break;
        default:
            break;
    }
}

static void clear_fb(const fb_t *fb, const uint8_t r, const uint8_t g, const uint8_t b) {

    int x;
    int y;

    const uint32_t pixel = pack_pixel(fb, r, g, b);

    if (fb->bpp == 32) {
        for (y = 0; y < fb->height; y++) {
            uint32_t *row = (uint32_t *) (fb->base + (size_t) y * (size_t) fb->stride);

            for (x = 0; x < fb->width; x++)
                row[x] = pixel;
        }

        return;
    }

    if (fb->bpp == 16) {
        const uint16_t pixel16 = (uint16_t) pixel;

        for (y = 0; y < fb->height; y++) {
            uint16_t *row = (uint16_t *) (fb->base + (size_t) y * (size_t) fb->stride);

            for (x = 0; x < fb->width; x++)
                row[x] = pixel16;
        }

        return;
    }

    for (y = 0; y < fb->height; y++) {
        uint8_t *row = fb->base + (size_t) y * (size_t) fb->stride;

        for (x = 0; x < fb->width; x++) {
            row[(size_t) x * 3 + 0] = (uint8_t) (pixel & 0xFF);
            row[(size_t) x * 3 + 1] = (uint8_t) (pixel >> 8 & 0xFF);
            row[(size_t) x * 3 + 2] = (uint8_t) (pixel >> 16 & 0xFF);
        }
    }
}

static void gradient_colour_at(
    const gradient_t *grad, const int t_in, const int span_in, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b
) {
    const int span = span_in > 1 ? span_in - 1 : 1;

    const uint32_t s = (uint32_t) span;
    const uint32_t t = (uint32_t) (t_in < 0 ? 0 : t_in > span ? span : t_in);

    *out_r = (uint8_t) (((uint32_t) grad->top_r * (s - t) + (uint32_t) grad->bot_r * t) / s);
    *out_g = (uint8_t) (((uint32_t) grad->top_g * (s - t) + (uint32_t) grad->bot_g * t) / s);
    *out_b = (uint8_t) (((uint32_t) grad->top_b * (s - t) + (uint32_t) grad->bot_b * t) / s);
}

static void gradient_axis(const int rotate, const fb_t *fb, int *axis, int *span, int *flip) {
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

static int fill_fb_with_gradient(const fb_t *fb, const gradient_t *grad, const int rotate) {
    int axis;
    int span;
    int flip;

    int x;
    int y;

    uint8_t r;
    uint8_t g;
    uint8_t b;

    gradient_axis(rotate, fb, &axis, &span, &flip);

    if (axis == 0) {
        for (y = 0; y < fb->height; y++) {
            const int t = flip ? fb->height - 1 - y : y;

            gradient_colour_at(grad, t, span, &r, &g, &b);
            const uint32_t pixel = pack_pixel(fb, r, g, b);

            if (fb->bpp == 32) {
                uint32_t *row = (uint32_t *) (fb->base + (size_t) y * (size_t) fb->stride);

                for (x = 0; x < fb->width; x++)
                    row[x] = pixel;
                continue;
            }

            if (fb->bpp == 16) {
                uint16_t *row = (uint16_t *) (fb->base + (size_t) y * (size_t) fb->stride);

                const uint16_t pixel16 = (uint16_t) pixel;

                for (x = 0; x < fb->width; x++)
                    row[x] = pixel16;
                continue;
            }

            {
                uint8_t *row = fb->base + (size_t) y * (size_t) fb->stride;

                const uint8_t b0 = (uint8_t) (pixel & 0xFF);
                const uint8_t b1 = (uint8_t) (pixel >> 8 & 0xFF);
                const uint8_t b2 = (uint8_t) (pixel >> 16 & 0xFF);

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
            const int t = flip ? fb->width - 1 - x : x;
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
            const int t = flip ? fb->width - 1 - x : x;
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
            const int t = flip ? fb->width - 1 - x : x;

            gradient_colour_at(grad, t, span, &r, &g, &b);
            const uint32_t pix = pack_pixel(fb, r, g, b);

            col_pixels24[(size_t) x * 3 + 0] = (uint8_t) (pix & 0xFF);
            col_pixels24[(size_t) x * 3 + 1] = (uint8_t) (pix >> 8 & 0xFF);
            col_pixels24[(size_t) x * 3 + 2] = (uint8_t) (pix >> 16 & 0xFF);
        }

        for (y = 0; y < fb->height; y++) {
            uint8_t *row = fb->base + (size_t) y * (size_t) fb->stride;
            memcpy(row, col_pixels24, (size_t) fb->width * 3);
        }

        free(col_pixels24);
    }

    return 0;
}

static void rotated_size(const image_t *img, const int rotate, int *out_w, int *out_h) {
    if (rotate == 90 || rotate == 270) {
        *out_w = img->height;
        *out_h = img->width;

        return;
    }

    *out_w = img->width;
    *out_h = img->height;
}

static void rotated_to_source(const image_t *img, const int rotate, const int rx, const int ry, int *sx, int *sy) {
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

static void calculate_dest_rect(
    const fb_t *fb, const int src_w, const int src_h, const scale_mode_t scale, int *dst_x, int *dst_y, int *dst_w,
    int *dst_h
) {
    int w;
    int h;

    switch (scale) {
        case scale_original:
            w = src_w;
            h = src_h;
            break;
        case scale_stretch:
            w = fb->width;
            h = fb->height;
            break;
        case scale_fill:
            if ((long long) fb->width * (long long) src_h >= (long long) fb->height * (long long) src_w) {
                w = fb->width;
                h = src_h * fb->width / src_w;
            } else {
                h = fb->height;
                w = fb->height * src_w / src_h;
            }
            break;
        case scale_keep:
        default:
            if ((long long) fb->width * (long long) src_h <= (long long) fb->height * (long long) src_w) {
                w = fb->width;
                h = src_h * fb->width / src_w;
            } else {
                h = fb->height;
                w = src_w * fb->height / src_h;
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

static void write_fb_pixel(
    const fb_t *fb, const int x, const int y, const uint8_t *src, const uint8_t bg_r, const uint8_t bg_g,
    const uint8_t bg_b
) {
    const uint8_t sr = src[0];
    const uint8_t sg = src[1];
    const uint8_t sb = src[2];
    const uint8_t sa = src[3];

    if (sa == 0xFF) {
        put_pixel(fb, x, y, sr, sg, sb);

        return;
    }

    if (sa == 0x00) {
        put_pixel(fb, x, y, bg_r, bg_g, bg_b);

        return;
    }

    const uint32_t inv = 255u - (uint32_t) sa;

    const uint8_t dr = (uint8_t) (((uint32_t) sr * sa + (uint32_t) bg_r * inv + 127u) / 255u);
    const uint8_t dg = (uint8_t) (((uint32_t) sg * sa + (uint32_t) bg_g * inv + 127u) / 255u);
    const uint8_t db = (uint8_t) (((uint32_t) sb * sa + (uint32_t) bg_b * inv + 127u) / 255u);

    put_pixel(fb, x, y, dr, dg, db);
}

static void build_recolour_lut(const options_t *opts, uint8_t lut_r[256], uint8_t lut_g[256], uint8_t lut_b[256]) {
    const uint32_t a = (uint32_t) opts->recolour_alpha;
    const uint32_t inv = 100u - a;

    const uint32_t tr = opts->recolour_r;
    const uint32_t tg = opts->recolour_g;
    const uint32_t tb = opts->recolour_b;

    for (int i = 0; i < 256; i++) {
        lut_r[i] = (uint8_t) (((uint32_t) i * inv + tr * a + 50u) / 100u);
        lut_g[i] = (uint8_t) (((uint32_t) i * inv + tg * a + 50u) / 100u);
        lut_b[i] = (uint8_t) (((uint32_t) i * inv + tb * a + 50u) / 100u);
    }
}

static void draw_image(const fb_t *fb, const image_t *img, const options_t *opts) {
    const int rotate = opts->rotate;
    const scale_mode_t scale = opts->scale;
    const gradient_t *grad = &opts->gradient;
    const int recolour = opts->recolour_enabled;

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

        gradient_axis(rotate, fb, &axis, &span, &flip);
        bg_axis = axis;
        const int n = axis == 0 ? fb->height : fb->width;

        bg_lut = malloc((size_t) n * 3);

        if (!bg_lut) {
            clear_fb(fb, grad->top_r, grad->top_g, grad->top_b);
        } else {
            for (int i = 0; i < n; i++) {
                const int t = flip ? n - 1 - i : i;
                gradient_colour_at(
                    grad, t, span, &bg_lut[(size_t) i * 3 + 0], &bg_lut[(size_t) i * 3 + 1], &bg_lut[(size_t) i * 3 + 2]
                );
            }
        }
    }

    const int screen_x0 = dst_x < 0 ? 0 : dst_x;
    const int screen_y0 = dst_y < 0 ? 0 : dst_y;

    int screen_x1 = dst_x + dst_w;
    int screen_y1 = dst_y + dst_h;

    if (screen_x1 > fb->width) screen_x1 = fb->width;
    if (screen_y1 > fb->height) screen_y1 = fb->height;

    for (int y = screen_y0; y < screen_y1; y++) {
        const int local_y = y - dst_y;
        int ry = local_y * rot_h / dst_h;

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

        for (int x = screen_x0; x < screen_x1; x++) {
            const int local_x = x - dst_x;

            int rx = local_x * rot_w / dst_w;
            int sx;
            int sy;

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

            const uint8_t *src = img->pixels + ((size_t) sy * (size_t) img->width + (size_t) sx) * 4;

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
}

static void free_image(image_t *img) {
    free(img->pixels);

    img->pixels = NULL;
    img->width = 0;
    img->height = 0;
}

// Hand the panel back to the framebuffer, whatever was driving it before
static int claim_display(fb_t *fb) {
    struct fb_var_screeninfo pan = fb->var;

    pan.xoffset = 0;
    pan.yoffset = 0;
    pan.activate = FB_ACTIVATE_NOW;

    if (ioctl(fb->fd, FBIOPAN_DISPLAY, &pan) < 0) return -1;

    fb->var.xoffset = 0;
    fb->var.yoffset = 0;
    fb->base = fb->mem;

    return 0;
}

static void copy_to_pages(const fb_t *fb, const uint8_t *source) {
    for (int page = 0; page < fb->page_count; page++) {
        const size_t offset = (size_t) page * fb->page_size;
        if (offset + fb->page_size > fb->mem_size) break;
        memcpy(fb->mem + offset, source, fb->page_size);
    }
}

static int present_image(fb_t *fb, const image_t *img, const options_t *opts) {
    if (!fb->buffered && fb->page_size > 0 && !fb->scratch) fb->scratch = malloc(fb->page_size);

    if (fb->buffered || !fb->scratch) {
        draw_image(fb, img, opts);
        mirror_pages(fb);
    } else {
        uint8_t *visible = fb->base;
        fb->base = fb->scratch;
        draw_image(fb, img, opts);
        fb->base = visible;
        copy_to_pages(fb, fb->scratch);
    }

    if (flush_fb(fb) != 0) return -1;

    const int claimed = claim_display(fb);
    unblank_fb(opts->fb_path);

    trace("present claimed=%d scanout=%lx", claimed, read_scanout_addr());

    return 0;
}

#define INPUT_MAX 32

typedef enum { media_still, media_animation, media_video } media_kind_t;

typedef struct {
    AVFormatContext *format;
    AVCodecContext *codec;
    struct SwsContext *sws;
    AVPacket *packet;
    AVFrame *frame;
    int stream;
    int draining;
    double time_base;
} video_t;

static int input_fds[INPUT_MAX];
static int input_count = 0;

static int has_extension(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    return dot && strcasecmp(dot + 1, extension) == 0;
}

static int png_is_animated(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;

    uint8_t buffer[65536];
    const size_t length = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    for (size_t i = 8; i + 4 <= length; i++) {
        if (memcmp(buffer + i, "acTL", 4) == 0) return 1;
        if (memcmp(buffer + i, "IDAT", 4) == 0) return 0;
    }

    return 0;
}

static media_kind_t media_kind(const char *path) {
    if (has_extension(path, "mp4") || has_extension(path, "apng")) return media_video;
    if (has_extension(path, "png")) return png_is_animated(path) ? media_video : media_still;
    if (has_extension(path, "gif") || has_extension(path, "webp")) return media_animation;
    return media_still;
}

static double now_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double) now.tv_sec + (double) now.tv_nsec / 1e9;
}

static int surface_to_image(SDL_Surface *surface, image_t *img) {
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgba) return -1;

    const size_t row = (size_t) rgba->w * 4;
    uint8_t *pixels = malloc(row * (size_t) rgba->h);
    if (!pixels) {
        SDL_FreeSurface(rgba);
        return -1;
    }

    for (int y = 0; y < rgba->h; y++) {
        memcpy(pixels + row * (size_t) y, (const uint8_t *) rgba->pixels + (size_t) y * (size_t) rgba->pitch, row);
    }

    free_image(img);
    img->pixels = pixels;
    img->width = rgba->w;
    img->height = rgba->h;

    SDL_FreeSurface(rgba);
    return 0;
}

static void video_close(video_t *video) {
    sws_freeContext(video->sws);
    av_frame_free(&video->frame);
    av_packet_free(&video->packet);
    avcodec_free_context(&video->codec);
    avformat_close_input(&video->format);
    memset(video, 0, sizeof(*video));
}

static int video_open(video_t *video, const char *path) {
    const AVCodec *decoder = NULL;

    memset(video, 0, sizeof(*video));

    if (avformat_open_input(&video->format, path, NULL, NULL) < 0) return -1;
    if (avformat_find_stream_info(video->format, NULL) < 0) goto fail;

    video->stream = av_find_best_stream(video->format, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (video->stream < 0 || !decoder) goto fail;

    video->codec = avcodec_alloc_context3(decoder);
    if (!video->codec) goto fail;
    if (avcodec_parameters_to_context(video->codec, video->format->streams[video->stream]->codecpar) < 0) goto fail;
    if (avcodec_open2(video->codec, decoder, NULL) < 0) goto fail;

    video->packet = av_packet_alloc();
    video->frame = av_frame_alloc();
    if (!video->packet || !video->frame) goto fail;

    video->time_base = av_q2d(video->format->streams[video->stream]->time_base);
    return 0;

fail:
    video_close(video);
    return -1;
}

static int video_frame_to_image(video_t *video, image_t *img) {
    const AVFrame *frame = video->frame;

    video->sws = sws_getCachedContext(
        video->sws, frame->width, frame->height, frame->format, frame->width, frame->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    if (!video->sws) return -1;

    if (!img->pixels || img->width != frame->width || img->height != frame->height) {
        free_image(img);

        img->pixels = malloc((size_t) frame->width * (size_t) frame->height * 4);
        if (!img->pixels) return -1;

        img->width = frame->width;
        img->height = frame->height;
    }

    uint8_t *destination[4] = {img->pixels, NULL, NULL, NULL};
    const int stride[4] = {frame->width * 4, 0, 0, 0};

    sws_scale(video->sws, (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height, destination, stride);

    return 0;
}

static int video_next(video_t *video, image_t *img, double *seconds) {
    for (;;) {
        const int received = avcodec_receive_frame(video->codec, video->frame);

        if (received == 0) {
            const int64_t pts = video->frame->best_effort_timestamp;
            *seconds = pts == AV_NOPTS_VALUE ? -1.0 : (double) pts * video->time_base;

            const int converted = video_frame_to_image(video, img);
            av_frame_unref(video->frame);

            return converted == 0 ? 1 : -1;
        }

        if (received == AVERROR_EOF) return 0;
        if (received != AVERROR(EAGAIN)) return -1;
        if (video->draining) return 0;

        if (av_read_frame(video->format, video->packet) < 0) {
            avcodec_send_packet(video->codec, NULL);
            video->draining = 1;
            continue;
        }

        if (video->packet->stream_index == video->stream) avcodec_send_packet(video->codec, video->packet);
        av_packet_unref(video->packet);
    }
}

static int load_media_still(const char *path, const media_kind_t kind, image_t *img) {
    if (kind == media_video) {
        video_t video;
        double seconds;

        if (video_open(&video, path) != 0) return -1;

        const int found = video_next(&video, img, &seconds);
        video_close(&video);

        return found > 0 ? 0 : -1;
    }

    if (has_extension(path, "png")) return load_png_to_rgba(path, img);

    SDL_Surface *surface = IMG_Load(path);
    if (!surface) return -1;

    const int converted = surface_to_image(surface, img);
    SDL_FreeSurface(surface);

    return converted;
}

static int input_pressed(void) {
    int pressed = 0;
    struct input_event event;

    for (int i = 0; i < input_count; i++) {
        while (read(input_fds[i], &event, sizeof(event)) == (ssize_t) sizeof(event)) {
            if (event.type == EV_KEY && event.value == 1) pressed = 1;
            if (event.type == EV_ABS && (event.code == ABS_HAT0X || event.code == ABS_HAT0Y) && event.value != 0)
                pressed = 1;
        }
    }

    return pressed;
}

static void input_open(void) {
    for (int i = 0; i < INPUT_MAX && input_count < INPUT_MAX; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);

        const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) input_fds[input_count++] = fd;
    }

    input_pressed();
}

static void input_close(void) {
    for (int i = 0; i < input_count; i++) {
        close(input_fds[i]);
    }

    input_count = 0;
}

static int wait_until(const double target) {
    struct pollfd fds[INPUT_MAX];

    for (;;) {
        if (!keep_running || input_pressed()) return 1;

        const double left = target - now_seconds();
        if (left <= 0.0) return 0;

        for (int i = 0; i < input_count; i++) {
            fds[i].fd = input_fds[i];
            fds[i].events = POLLIN;
            fds[i].revents = 0;
        }

        poll(fds, (nfds_t) input_count, left > 0.05 ? 50 : (int) (left * 1000.0) + 1);
    }
}

static void scale_frame(const fb_t *fb, const uint8_t *source, uint8_t *destination, const unsigned level) {
    const size_t size = fb->page_size;

    if (fb->bpp == 32 || fb->bpp == 24) {
        const size_t bytes = (size_t) fb->bytes_per_pixel;
        uint8_t keep[4] = {0, 0, 0, 0};

        if (fb->var.transp.length && fb->var.transp.offset % 8 == 0 && fb->var.transp.offset / 8 < bytes) {
            keep[fb->var.transp.offset / 8] = 1;
        }

        for (size_t i = 0; i < size; i++) {
            destination[i] = keep[i % bytes] ? source[i] : (uint8_t) (((unsigned) source[i] * level) >> 8);
        }

        return;
    }

    if (fb->bpp == 16) {
        const struct fb_bitfield *channels[3] = {&fb->var.red, &fb->var.green, &fb->var.blue};

        for (size_t i = 0; i + 1 < size; i += 2) {
            uint16_t pixel;
            memcpy(&pixel, source + i, sizeof(pixel));

            uint16_t scaled = pixel;
            for (int c = 0; c < 3; c++) {
                const uint16_t mask = (uint16_t) (((1u << channels[c]->length) - 1u) << channels[c]->offset);
                const unsigned value = ((unsigned) (pixel & mask) >> channels[c]->offset) * level >> 8;

                scaled = (uint16_t) ((scaled & ~mask) | ((value << channels[c]->offset) & mask));
            }

            memcpy(destination + i, &scaled, sizeof(scaled));
        }

        return;
    }

    memcpy(destination, source, size);
}

static void fade_frame(fb_t *fb, const options_t *opts, const uint8_t *source, const int fade_in) {
    uint8_t *frame = malloc(fb->page_size);
    if (!frame) return;

    const double start = now_seconds();
    const double length = (double) opts->fade / 1000.0;
    int steps = 0;

    for (;;) {
        double progress = (now_seconds() - start) / length;
        if (progress > 1.0) progress = 1.0;

        unsigned level = (unsigned) (256.0 * progress);
        if (!fade_in) level = 256u - level;

        scale_frame(fb, source, frame, level);
        copy_to_pages(fb, frame);
        flush_fb(fb);
        claim_display(fb);
        unblank_fb(opts->fb_path);
        steps++;

        if (progress >= 1.0) break;
    }

    trace("fade %s took %.0fms in %d steps", fade_in ? "in" : "out", (now_seconds() - start) * 1000.0, steps);
    free(frame);
}

static int fade_in_image(fb_t *fb, const image_t *img, const options_t *opts) {
    uint8_t *frame = calloc(1, fb->page_size);
    if (!frame) return present_image(fb, img, opts);

    uint8_t *visible = fb->base;
    fb->base = frame;
    draw_image(fb, img, opts);
    fb->base = visible;

    fade_frame(fb, opts, frame, 1);
    free(frame);

    trace("faded in over %dms scanout=%lx", opts->fade, read_scanout_addr());
    return 0;
}

static void fade_out_screen(fb_t *fb, const options_t *opts) {
    uint8_t *frame = malloc(fb->page_size);
    if (!frame) return;

    memcpy(frame, fb->base, fb->page_size);
    fade_frame(fb, opts, frame, 0);
    free(frame);

    trace("faded out over %dms", opts->fade);
}

static int play_animation(fb_t *fb, const options_t *opts, image_t *img) {
    IMG_Animation *animation = IMG_LoadAnimation(opts->image_path);
    if (!animation) return 0;

    double target = now_seconds();
    int skipped = 0;

    for (int i = 0; i < animation->count; i++) {
        if (surface_to_image(animation->frames[i], img) != 0) break;
        if (present_image(fb, img, opts) != 0) break;

        target += (animation->delays[i] > 0 ? animation->delays[i] : 100) / 1000.0;
        if (wait_until(target)) {
            skipped = 1;
            break;
        }
    }

    trace("animation finished frames=%d skipped=%d", animation->count, skipped);
    IMG_FreeAnimation(animation);

    return skipped;
}

static int play_video(fb_t *fb, const options_t *opts, image_t *img) {
    video_t video;
    if (video_open(&video, opts->image_path) != 0) return 0;

    const double start = now_seconds();

    double first = -1.0;
    double previous = 0.0;
    double seconds = 0.0;

    int shown = 1;
    int presented = 0;
    int dropped = 0;
    int skipped = 0;

    while (video_next(&video, img, &seconds) > 0) {
        if (seconds < 0.0) seconds = previous + 1.0 / 30.0;
        if (first < 0.0) first = seconds;
        previous = seconds;

        const double target = start + (seconds - first);
        shown = 0;

        if (wait_until(target)) {
            shown = 1;
            skipped = 1;
            break;
        }

        if (now_seconds() - target > 0.1) {
            dropped++;
            continue;
        }

        if (present_image(fb, img, opts) != 0) break;
        shown = 1;
        presented++;
    }

    if (!shown && img->pixels) present_image(fb, img, opts);

    trace("video finished presented=%d dropped=%d skipped=%d", presented, dropped, skipped);
    video_close(&video);

    return skipped;
}

static int run_clear(const options_t *opts) {
    fb_t fb = {0};
    fb.fd = -1;

    log_value(opts, "Opening framebuffer: ", opts->fb_path);

    if (open_fb(opts->fb_path, &fb) != 0) {
        log_value(opts, "Failed to open framebuffer: ", opts->fb_path);

        return 1;
    }

    unblank_fb(opts->fb_path);

    log_msg(opts, "Clearing framebuffer");

    clear_fb(&fb, 0, 0, 0);
    mirror_pages(&fb);

    if (flush_fb(&fb) != 0) {
        log_msg(opts, "Failed to flush framebuffer");
        close_fb(&fb);
        return 1;
    }

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

    trace_open(opts);
    trace("start image=%s wait=%d", opts->image_path ? opts->image_path : "(none)", opts->wait);

    log_value(opts, "Loading image: ", opts->image_path);

    const media_kind_t kind = media_kind(opts->image_path);

    if (load_media_still(opts->image_path, kind, &img) != 0) {
        log_value(opts, "Failed to load image: ", opts->image_path);
        trace("failed to load image");
        trace_close();

        return 1;
    }

    log_value(opts, "Opening framebuffer: ", opts->fb_path);

    if (open_fb(opts->fb_path, &fb) != 0) {
        log_value(opts, "Failed to open framebuffer: ", opts->fb_path);
        trace("failed to open %s: %s", opts->fb_path, fb_error[0] ? fb_error : "unknown");
        trace_close();
        free_image(&img);

        return 1;
    }

    trace(
        "opened %dx%d bpp=%d stride=%d smem=%lx len=%zu pages=%d offset=%u,%u buffered=%d scanout=%lx", fb.width,
        fb.height, fb.bpp, fb.stride, (unsigned long) fb.fix.smem_start, fb.mem_size, fb.page_count, fb.var.xoffset,
        fb.var.yoffset, fb.buffered, read_scanout_addr()
    );

    unblank_fb(opts->fb_path);

    if (opts->verbose) {
        fprintf(
            stderr, "Drawing image=%s fb=%s screen=%dx%d bpp=%d rotate=%d scale=%s pages=%d offset=%u,%u\n",
            opts->image_path, opts->fb_path, fb.width, fb.height, fb.bpp, opts->rotate, scale_name(opts->scale),
            fb.page_count, fb.var.xoffset, fb.var.yoffset
        );

        if (opts->gradient.enabled) {
            fprintf(
                stderr, "Gradient: top=%02X%02X%02X bottom=%02X%02X%02X\n", opts->gradient.top_r, opts->gradient.top_g,
                opts->gradient.top_b, opts->gradient.bot_r, opts->gradient.bot_g, opts->gradient.bot_b
            );
        }

        if (opts->recolour_enabled) {
            fprintf(
                stderr, "Recolour: tint=%02X%02X%02X alpha=%d%%\n", opts->recolour_r, opts->recolour_g,
                opts->recolour_b, opts->recolour_alpha
            );
        }
    }

    const int playing = opts->play && kind != media_still;
    const int accepts_input = playing || opts->duration != 0;
    if (accepts_input) input_open();

    const int fading = opts->fade > 0 && !opts->wait && fb.page_size > 0;
    const int shown = fading ? fade_in_image(&fb, &img, opts) : present_image(&fb, &img, opts);

    if (shown != 0) {
        log_msg(opts, "Failed to flush framebuffer");
        trace("failed to flush framebuffer");
        if (accepts_input) input_close();
        trace_close();
        close_fb(&fb);
        free_image(&img);
        return 1;
    }

    trace(
        "drew into offset=%u,%u mirrored=%d scanout=%lx", fb.var.xoffset, fb.var.yoffset, fb.page_count,
        read_scanout_addr()
    );

    if (notify_ready(opts->ready_path) != 0) {
        log_value(opts, "Failed to create ready file: ", opts->ready_path);
        trace("failed to create ready file %s", opts->ready_path);
        if (accepts_input) input_close();
        trace_close();
        close_fb(&fb);
        free_image(&img);
        return 1;
    }

    const double shown_at = now_seconds();

    if (accepts_input) {
        int skipped = 0;
        if (playing) skipped = kind == media_animation ? play_animation(&fb, opts, &img) : play_video(&fb, opts, &img);

        if (!skipped) {
            if (opts->duration == DURATION_PRESS) {
                wait_until(shown_at + 86400.0);
            } else if (opts->duration == DURATION_FINISH && !playing) {
                wait_until(shown_at + FINISH_STILL_SECONDS);
            } else if (opts->duration > 0) {
                wait_until(shown_at + opts->duration);
            }
        }

        input_close();
    }

    if (fading) fade_out_screen(&fb, opts);

    if (opts->wait) {
        log_msg(opts, "Waiting until killed");

        // Redraw a few times early on in case something else was still letting go of the screen
        static const unsigned int settle_ticks[] = {2, 5, 10, 20};
        const unsigned int settle_total = sizeof(settle_ticks) / sizeof(settle_ticks[0]);

        unsigned int settle_next = 0;
        unsigned int tick = 0;

        unsigned long last_scanout = read_scanout_addr();

        while (keep_running) {
            struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
            while (nanosleep(&delay, &delay) < 0 && errno == EINTR && keep_running) {
            }
            if (!keep_running) break;

            const int view_changed = refresh_fb_view(&fb);

            tick++;

            const unsigned long scanout = read_scanout_addr();
            if (scanout != last_scanout) {
                trace("scanout moved to %lx (offset now %u,%u)", scanout, fb.var.xoffset, fb.var.yoffset);
                last_scanout = scanout;
            }

            int redraw = view_changed > 0;

            if (settle_next < settle_total && tick >= settle_ticks[settle_next]) {
                settle_next++;
                redraw = 1;
            }

            if (!redraw) continue;

            trace(
                "redraw tick=%u moved=%d offset=%u,%u scanout=%lx", tick, view_changed > 0, fb.var.xoffset,
                fb.var.yoffset, scanout
            );

            if (view_changed > 0) {
                if (opts->verbose) {
                    fprintf(stderr, "Screen page moved to %u,%u, redrawing\n", fb.var.xoffset, fb.var.yoffset);
                }
            }

            if (present_image(&fb, &img, opts) != 0) log_msg(opts, "Failed to refresh framebuffer");
        }
    }

    trace("exiting scanout=%lx", read_scanout_addr());
    trace_close();

    close_fb(&fb);
    free_image(&img);

    log_msg(opts, "Done");

    return 0;
}

int main(const int argc, char *argv[]) {
    options_t opts;

    if (parse_args(argc, argv, &opts) != 0) return 1;

    setup_signals();

    if (opts.clear) return run_clear(&opts);

    return run_draw(&opts);
}
