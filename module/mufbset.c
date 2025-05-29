#include <stdio.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <png.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

typedef struct {
    int fd;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
    size_t size;
    void * mem;
} fb_info;

int open_fb(fb_info * fb) {
    if (fb == NULL) return -1;

    fb->fd = open(device.SCREEN.DEVICE, O_RDWR);
    if (fb->fd < 0) {
        perror("Error opening framebuffer device");
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->finfo) < 0) {
        perror("Error retrieving fixed screen info");
        close(fb->fd);
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo) < 0) {
        perror("Error retrieving variable screen info");
        close(fb->fd);
        return -1;
    }

    fb->size = fb->finfo.smem_len;
    fb->mem = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (fb->mem == MAP_FAILED) {
        perror("Error mapping framebuffer memory");
        close(fb->fd);
        return -1;
    }
    return 0;
}

int destroy_fb(fb_info * fb) {
    if (fb == NULL) return -1;

    munmap(fb->mem, fb->size);
    close(fb->fd);
    return 0;
}


int clear_framebuffer() {
    fb_info fb;

    if (open_fb(&fb) < 0) return -1;

    // This is clearing the whole framebuffer, not only the active area
    memset(fb.mem, 0, fb.size);

    if (destroy_fb(&fb) < 0) return -1;

    printf("Framebuffer cleared successfully.\n");
    return 0;
}

void get_active_area_framebuffer(fb_info * fb, size_t * offset, size_t * size, size_t * stride, int reask) {
    if (reask && ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo) < 0) {
        perror("Error retrieving variable screen info");
    }
    *stride = fb->finfo.line_length / (fb->vinfo.bits_per_pixel / 8);
    *offset = fb->vinfo.xoffset + (fb->vinfo.yoffset * *stride);
    *size   = *stride * fb->vinfo.yres;
}



int save_framebuffer(const char * filename) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("Error opening the given filename");
        return -1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fclose(fp);
        perror("Can't create png library");
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, info_ptr == NULL ? NULL : &info_ptr);
        perror("Can't create png info");
        fclose(fp);
        return -1;
    }

    fb_info fb;

    if (open_fb(&fb) < 0) return -1;

    // Copy only the active area of the framebuffer
    size_t offset = 0, size = 0, stride = 0;
    get_active_area_framebuffer(&fb, &offset, &size, &stride, 0);

    png_set_IHDR(png_ptr, info_ptr, fb.vinfo.xres, fb.vinfo.yres, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_byte **row_pointers = png_malloc(png_ptr, fb.vinfo.yres * sizeof(png_byte *));
      uint32_t * in = ((uint32_t*)fb.mem) + offset;
    for (size_t y = 0; y < fb.vinfo.yres; ++y) {
        uint8_t *row = png_malloc(png_ptr, fb.finfo.line_length);
        row_pointers[y] = (png_byte *)row;

        for (size_t x = 0; x < fb.vinfo.xres; ++x) {
            uint32_t color = in[x];
            *row++ = (color >> fb.vinfo.red.offset) & ((1 << fb.vinfo.red.length) - 1);
            *row++ = (color >> fb.vinfo.green.offset) & ((1 << fb.vinfo.green.length) - 1);
            *row++ = (color >> fb.vinfo.blue.offset) & ((1 << fb.vinfo.blue.length) - 1);
        }
        in += stride;
    }

    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    for (size_t y = 0; y < fb.vinfo.yres; y++)
        png_free(png_ptr, row_pointers[y]);
    png_free(png_ptr, row_pointers);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    if (destroy_fb(&fb) < 0) return -1;

    printf("Framebuffer saved successfully as PNG to %s.\n", filename);
    return 0;
}

// Scan files in dir, read any symbolic link and check if it matches the given filename
static bool has_file_in_dir (const char *const dir_to_scan, const char * compared_filename) {
    DIR * fd_dir = opendir (dir_to_scan);
    if (fd_dir == NULL) return false;

    struct dirent * fd_ent = readdir (fd_dir);
    while (fd_ent != NULL)
    {
        if (fd_ent->d_type == DT_LNK) {
            char fd_symlnk[PATH_MAX];
            snprintf (fd_symlnk, sizeof (fd_symlnk), "%s/%s", dir_to_scan, fd_ent->d_name);

            char fd_target[PATH_MAX + 1];
            size_t fd_target_len = readlink (fd_symlnk, fd_target, sizeof (fd_target) - 1);
            if (fd_target_len > 0) {
                fd_target[fd_target_len] = '\0';
                if (!strcmp (fd_target, compared_filename)) {
                    closedir(fd_dir);
                    return true;
                }
            }
        }
        fd_ent = readdir (fd_dir);
    }

    closedir (fd_dir);
    return false;
}

static pid_t find_pid_using (const char * filename) {
    DIR * proc_dir = opendir ("/proc");
    if (proc_dir == NULL) return 0;

    struct dirent * proc_ent = readdir (proc_dir);
    while (proc_ent != NULL)
    {
        char junk;
        pid_t pid;
        if ((proc_ent->d_type == DT_DIR) && (sscanf (proc_ent->d_name, "%d%c", &pid, &junk) == 1))
        {
            char fd_dir_to_scan[PATH_MAX];
            snprintf (fd_dir_to_scan, sizeof (fd_dir_to_scan), "/proc/%s/fd", proc_ent->d_name);
            if (has_file_in_dir(fd_dir_to_scan, filename)) return pid;
        }

        proc_ent = readdir (proc_dir);
    }

    closedir (proc_dir);
    return 0;
}

int overlay_framebuffer(const char * overlay_filename, const int pos_x, const int pos_y, const int delay) {
    // Then perform the overlay action here
    pid_t process = find_pid_using(device.SCREEN.DEVICE);

    FILE *fp = fopen(overlay_filename, "rb");
    if (fp == NULL) {
        perror("Error opening the given filename");
        return -1;
    }

    // Read PNG file now
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (setjmp(png_jmpbuf(png))) {
        perror("Error creating the png structure");
        return -1;
    }
    png_init_io(png, fp);

    png_infop info = png_create_info_struct(png);
    png_read_info(png, info);
    uint32_t width = png_get_image_width(png, info);
    uint32_t height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Convert to RGBA
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB) {
      png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);

    uint8_t * rgba = malloc(height * width * 4);
    png_bytep* row_pointers = malloc(height * sizeof(png_bytep));
    for(uint32_t y = 0; y < height; y++)
      row_pointers[y] = (png_bytep)(rgba + y * width * 4);

    png_read_image(png, row_pointers);
    free(row_pointers);
    fclose(fp);

    fb_info fb;

    if (open_fb(&fb) < 0) return -1;

    // Draw to only the active area in the framebuffer
    size_t offset = 0, size = 0, stride = 0;
    get_active_area_framebuffer(&fb, &offset, &size, &stride, 0);

    // Pause process if asked to
    if (process) kill(process, SIGSTOP);

    // Save the current framebuffer content to restore afterward
    uint32_t * saved_pixels = malloc(fb.size);
    if (saved_pixels == NULL)
    {
        destroy_fb(&fb);
        free(rgba);
        perror("Not enough memory to save the current frame buffer");
        return -1;
    }
    memcpy(saved_pixels, fb.mem, fb.size);

    uint32_t * out = ((uint32_t*)fb.mem) + offset, * in = saved_pixels + offset, * content = (uint32_t*)rgba;
    out += stride * pos_y; in += stride * pos_y;
    for (size_t y = pos_y; y < (height + pos_y) && y < fb.vinfo.yres; ++y) {
        for (size_t x = pos_x; x < (width + pos_x) && x < fb.vinfo.xres; ++x) {
            uint32_t c = content[x - pos_x], i = in[x];
            uint8_t a = (c & 0xFF000000) >> 24;
            uint8_t b = (c & 0x00FF0000) >> 16;
            uint8_t g = (c & 0x0000FF00) >> 8;
            uint8_t r = (c & 0x000000FF) >> 0;

            uint8_t ia = (i >> fb.vinfo.transp.offset) & ((1 << fb.vinfo.transp.length) - 1);
            uint8_t ir = (i >> fb.vinfo.red.offset) & ((1 << fb.vinfo.red.length) - 1);
            uint8_t ig = (i >> fb.vinfo.green.offset) & ((1 << fb.vinfo.green.length) - 1);
            uint8_t ib = (i >> fb.vinfo.blue.offset) & ((1 << fb.vinfo.blue.length) - 1);

            // Alpha blend pixels now
            r = (r * a + ir * (255 - a)) / 255;
            g = (g * a + ig * (255 - a)) / 255;
            b = (b * a + ib * (255 - a)) / 255;

            // And output them
            c = ((ia & ((1 << fb.vinfo.transp.length) - 1)) << fb.vinfo.transp.offset)
                | ((r & ((1 << fb.vinfo.red.length) - 1)) << fb.vinfo.red.offset)
                | ((g & ((1 << fb.vinfo.green.length) - 1)) << fb.vinfo.green.offset)
                | ((b & ((1 << fb.vinfo.blue.length) - 1)) << fb.vinfo.blue.offset);

            out[x] = c;
        }
        content += width;
        in += stride;
        out += stride;
    }

    sleep(delay);

    // Finally restore the framebuffer to what it was before
    memcpy(fb.mem, saved_pixels, fb.size);
    if (process) kill(process, SIGCONT);

    free(rgba);
    free(saved_pixels);
    destroy_fb(&fb);
    printf("Drawn overlay %s(%ux%u) at %d,%d.\n", overlay_filename, width, height, pos_x, pos_y);
    return 0;
}


void print_available_modes() {
    const char *sys_modes = "/sys/class/graphics/fb0/modes";
    FILE *modes_file = fopen(sys_modes, "r");
    if (!modes_file) {
        perror("Unable to read available modes from sysfs");
        return;
    }

    printf("Available modes:\n");
    char mode[32];
    while (fgets(mode, sizeof(mode), modes_file)) printf("  %s", mode);
    fclose(modes_file);
}

int set_framebuffer(int width, int height, int depth, int hsync_len, int vsync_len, int ignore_dh) {
    struct fb_var_screeninfo vinfo;
    int fb_fd;

    fb_fd = open(device.SCREEN.DEVICE, O_RDWR);
    if (fb_fd < 0) {
        perror("Error opening framebuffer device");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Error retrieving variable screen info");
        close(fb_fd);
        return -1;
    }

    printf("Current resolution: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    if (width > 0) vinfo.xres = width;
    if (height > 0) vinfo.yres = height;

    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres * ignore_dh;

    // Set pan offset to match what fbset does
    vinfo.xoffset = vinfo.xres_virtual - vinfo.xres;
    vinfo.yoffset = vinfo.yres_virtual - vinfo.yres;

    if (depth > 0) vinfo.bits_per_pixel = depth;

    if (hsync_len > 0) vinfo.hsync_len = hsync_len;
    if (vsync_len > 0) vinfo.vsync_len = vsync_len;

    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
        perror("Error setting variable screen info");
        close(fb_fd);
        return -1;
    }

    printf("Updated resolution: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    printf("Timing: hsync=%d, vsync=%d\n", vinfo.hsync_len, vinfo.vsync_len);

    close(fb_fd);
    return 0;
}

void print_help(const char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("Options:\n");
    printf("  -w, --width  <WIDTH>      Set the framebuffer width\n");
    printf("  -h, --height <HEIGHT>     Set the framebuffer height\n");
    printf("  -d, --depth  <DEPTH>      Set the color depth (bits per pixel)\n");
    printf("  -x, --hsync  <HSYNC_LEN>  Set the horizontal sync length\n");
    printf("  -y, --vsync  <VSYNC_LEN>  Set the vertical sync length\n");
    printf("  -i, --ignore              Ignore virtual framebuffer height double\n");
    printf("  -m, --modes               Show available framebuffer modes\n");
    printf("  -c, --clear               Clear the framebuffer before changing resolution\n");
    printf("  -s, --save <FILENAME>     Save the current framebuffer to the given file as PNG\n");
    printf("  -o, --overlay <FILE>      Overlay the given file onto the framebuffer\n");
    printf("  -p, --pos <X,Y>           Top left position to display the overlay\n");
    printf("  -t, --time <SEC>          The duration to display the overlay\n");
    printf("  -H, --help                Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -w 640 -h 480 -d 32 -c  Clear and set resolution to 640x480 with 32bpp\n", progname);
    printf("  %s -o v.png -p 20,460 -t 2 Display overlay v.png at (20,460) for 2s\n", progname);
    printf("  %s -m                      Show available modes\n", progname);
}

int main(int argc, char *argv[]) {
    int opt;
    int width = 0, height = 0, depth = 0;
    int hsync_len = 0, vsync_len = 0, ignore_dh = 2;
    int show_modes = 0, clear_screen = 0;
    int delay = 0, pos_x = 0, pos_y = 0;
    const char * save_filename = NULL;
    const char * overlay_filename = NULL;

    static struct option long_options[] = {
            {"width",   required_argument, 0, 'w'},
            {"height",  required_argument, 0, 'h'},
            {"depth",   required_argument, 0, 'd'},
            {"hsync",   required_argument, 0, 'x'},
            {"vsync",   required_argument, 0, 'y'},
            {"save",    required_argument, 0, 's'},
            {"overlay", required_argument, 0, 'o'},
            {"pos",     required_argument, 0, 'p'},
            {"time",    required_argument, 0, 't'},
            {"ignore",  no_argument,       0, 'i'},
            {"modes",   no_argument,       0, 'm'},
            {"clear",   no_argument,       0, 'c'},
            {"help",    no_argument,       0, 'H'},
            {0, 0,                        0, 0}
    };

    while ((opt = getopt_long(argc, argv, "w:h:d:x:y:s:o:p:t:imcH", long_options, NULL)) != -1) {
        switch (opt) {
            case 'w':
                width = safe_atoi(optarg);
                break;
            case 'h':
                height = safe_atoi(optarg);
                break;
            case 'd':
                depth = safe_atoi(optarg);
                break;
            case 'x':
                hsync_len = safe_atoi(optarg);
                break;
            case 'y':
                vsync_len = safe_atoi(optarg);
                break;
            case 's':
                save_filename = optarg;
                break;
            case 'o':
                overlay_filename = optarg;
                break;
            case 'p':
                {
                    char * comma = NULL;
                    pos_x = (int)strtol(optarg, &comma, 10);
                    if (!comma || *comma != ',') {
                        fprintf(stderr, "Invalid format for position.\n");
                        return 1;
                    }
                    pos_y = (int)strtol(comma + 1, NULL, 10);
                    break;
                }
            case 't':
                delay = safe_atoi(optarg);
                break;
            case 'i':
                ignore_dh = 1;
                break;
            case 'm':
                show_modes = 1;
                break;
            case 'c':
                clear_screen = 1;
                break;
            case 'H':
                print_help(argv[0]);
                return 0;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    load_device(&device);
    load_config(&config);

    if (show_modes) {
        print_available_modes();
        return 0;
    }

    if (save_filename) {
        if (save_framebuffer(save_filename) < 0) {
            fprintf(stderr, "Failed to save the framebuffer.\n");
            return 1;
        }
    }

    if (clear_screen) {
        if (clear_framebuffer() < 0) {
            fprintf(stderr, "Failed to clear the framebuffer.\n");
            return 1;
        }
    }

    if (overlay_filename) {
        if (overlay_framebuffer(overlay_filename, pos_x, pos_y, delay) < 0) {
            fprintf(stderr, "Failed to overlay on the framebuffer.\n");
            return 1;
        }
    }

    if (save_filename || overlay_filename) {
        // Work is done already, let's quit
        return 0;
    }

    if (width > 0 || height > 0 || depth > 0 || hsync_len > 0 || vsync_len > 0) {
        if (set_framebuffer(width, height, depth, hsync_len, vsync_len, ignore_dh) == 0) {
            printf("Framebuffer updated successfully.\n");
        } else {
            fprintf(stderr, "Failed to update framebuffer configuration.\n");
            return 1;
        }
    } else {
        print_help(argv[0]);
    }

    return 0;
}
