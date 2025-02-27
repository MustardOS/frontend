#include <stdio.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/mman.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"

char *mux_module;

int msgbox_active = 0;
int nav_sound = 0;

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

int clear_framebuffer() {
    int fb_fd;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    fb_fd = open(device.SCREEN.DEVICE, O_RDWR);
    if (fb_fd < 0) {
        perror("Error opening framebuffer device");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("Error retrieving fixed screen info");
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Error retrieving variable screen info");
        close(fb_fd);
        return -1;
    }

    size_t fb_size = finfo.line_length * vinfo.yres;
    void *fb_mem = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        perror("Error mapping framebuffer memory");
        close(fb_fd);
        return -1;
    }


    memset(fb_mem, 0, fb_size);
    munmap(fb_mem, fb_size);
    close(fb_fd);
    printf("Framebuffer cleared successfully.\n");

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
    printf("  -H, --help                Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -w 640 -h 480 -d 32 -c  Clear and set resolution to 640x480 with 32bpp\n", progname);
    printf("  %s -m                      Show available modes\n", progname);
}

int main(int argc, char *argv[]) {
    int opt;
    int width = 0, height = 0, depth = 0;
    int hsync_len = 0, vsync_len = 0, ignore_dh = 2;
    int show_modes = 0, clear_screen = 0;

    static struct option long_options[] = {
            {"width",  required_argument, 0, 'w'},
            {"height", required_argument, 0, 'h'},
            {"depth",  required_argument, 0, 'd'},
            {"hsync",  required_argument, 0, 'x'},
            {"vsync",  required_argument, 0, 'y'},
            {"ignore", no_argument,       0, 'i'},
            {"modes",  no_argument,       0, 'm'},
            {"clear",  no_argument,       0, 'c'},
            {"help",   no_argument,       0, 'H'},
            {0, 0,                        0, 0}
    };

    while ((opt = getopt_long(argc, argv, "w:h:d:x:y:imcH", long_options, NULL)) != -1) {
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

    if (clear_screen) {
        if (clear_framebuffer() < 0) {
            fprintf(stderr, "Failed to clear the framebuffer.\n");
            return 1;
        }
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
