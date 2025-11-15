#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/theme.h"
#include "../common/log.h"

static char *module = "fbset";
int verbose = 0;

int clear_framebuffer(void) {
    int fb_fd;

    struct fb_fix_screeninfo f_info;
    struct fb_var_screeninfo v_info;

    fb_fd = open(device.SCREEN.DEVICE, O_RDWR);
    if (fb_fd < 0) {
        LOG_ERROR(module, "Error opening framebuffer device")
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &f_info) < 0) {
        LOG_ERROR(module, "Error retrieving fixed screen info")
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error retrieving variable screen info")
        close(fb_fd);
        return -1;
    }

    size_t fb_size = f_info.line_length * v_info.yres;
    void *fb_mem = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        LOG_ERROR(module, "Error mapping framebuffer memory")
        close(fb_fd);
        return -1;
    }

    memset(fb_mem, 0, fb_size);
    munmap(fb_mem, fb_size);

    close(fb_fd);

    if (verbose) LOG_SUCCESS(module, "Framebuffer cleared successfully")

    return 0;
}

void print_available_modes(void) {
    const char *sys_modes = "/sys/class/graphics/fb0/modes";

    FILE *modes_file = fopen(sys_modes, "r");
    if (!modes_file) {
        LOG_ERROR(module, "Unable to read available modes from sysfs")
        return;
    }

    LOG_INFO(module, "Available Modes:")

    char mode[64];
    while (fgets(mode, sizeof(mode), modes_file)) LOG_INFO(module, "  %s", mode)

    fclose(modes_file);
}

void show_current_mode(void) {
    struct fb_var_screeninfo v_info;
    int fb_fd = open(device.SCREEN.DEVICE, O_RDONLY);

    if (fb_fd < 0) {
        LOG_ERROR(module, "Error opening framebuffer device")
        return;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error retrieving variable screen info")
        close(fb_fd);
        return;
    }

    LOG_INFO(module, "Current Mode: %dx%d (%dx%d virtual), %dbpp",
             v_info.xres, v_info.yres, v_info.xres_virtual,
             v_info.yres_virtual, v_info.bits_per_pixel)

    LOG_INFO(module, "Timing: hsync=%d, vsync=%d, rotate=%d", v_info.hsync_len, v_info.vsync_len, v_info.rotate)

    close(fb_fd);
}

int set_framebuffer(int width, int height, int depth, int hsync_len, int vsync_len, int ignore_dh, int rotation) {
    struct fb_var_screeninfo v_info, verify;
    int fb_fd;

    fb_fd = open(device.SCREEN.DEVICE, O_RDWR);
    if (fb_fd < 0) {
        LOG_ERROR(module, "Error opening framebuffer device")
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error retrieving variable screen info")
        close(fb_fd);
        return -1;
    }

    if (verbose) LOG_INFO(module, "Current resolution: %dx%d, %dbpp", v_info.xres, v_info.yres, v_info.bits_per_pixel)

    if (ignore_dh < 1) ignore_dh = 1;
    if (ignore_dh > 4) ignore_dh = 4;

    if (width > 0) v_info.xres = width;
    if (height > 0) v_info.yres = height;

    v_info.xres_virtual = v_info.xres;
    v_info.yres_virtual = v_info.yres * ignore_dh;

    v_info.xoffset = v_info.xres_virtual - v_info.xres;
    v_info.yoffset = v_info.yres_virtual - v_info.yres;

    if (depth > 0) v_info.bits_per_pixel = depth;

    if (hsync_len > 0) v_info.hsync_len = hsync_len;
    if (vsync_len > 0) v_info.vsync_len = vsync_len;

    if (rotation >= 0) v_info.rotate = rotation;

    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error setting variable screen info")
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &verify) == 0) {
        if (verify.xres != v_info.xres || verify.yres != v_info.yres) {
            LOG_WARN(module, "Hardware adjusted the mode: got %dx%d instead", verify.xres, verify.yres)
        }
    }

    if (verbose) {
        LOG_INFO(module, "Updated resolution: %dx%d, %dbpp", v_info.xres, v_info.yres, v_info.bits_per_pixel)
        LOG_INFO(module, "Timing: hsync=%d, vsync=%d, rotate=%d", v_info.hsync_len, v_info.vsync_len, v_info.rotate)
    }

    close(fb_fd);
    return 0;
}

void print_help(const char *prog) {
    printf("\nUsage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  -w, --width   <WIDTH>      Set the framebuffer width\n");
    printf("  -h, --height  <HEIGHT>     Set the framebuffer height\n");
    printf("  -d, --depth   <DEPTH>      Set colour depth (bits per pixel)\n");
    printf("  -x, --hsync   <HSYNC_LEN>  Set horizontal sync length\n");
    printf("  -y, --vsync   <VSYNC_LEN>  Set vertical sync length\n");
    printf("  -r, --rotate  <0-3>        Set rotation (if supported)\n");
    printf("  -i, --ignore               Ignore double-height logic\n");
    printf("  -m, --modes                Show available framebuffer modes\n");
    printf("  -s, --show                 Show current framebuffer mode\n");
    printf("  -c, --clear                Clear framebuffer\n");
    printf("  -v, --verbose              Verbose output\n");
    printf("  -H, --help                 Show this help\n");
    printf("\nExamples:\n");
    printf("  %s -w 640 -h 480 -d 32 -c  Clear and set resolution to 640x480 with 32bpp\n", prog);
    printf("  %s -m                      Show available modes\n\n", prog);
}

int main(int argc, char *argv[]) {
    int opt;
    int width = 0, height = 0, depth = 0;
    int hsync_len = 0, vsync_len = 0, ignore_dh = 2;
    int show_modes = 0, clear_screen = 0;
    int rotation = -1, show_info = 0;

    static struct option long_options[] = {
            {"width",   required_argument, 0, 'w'},
            {"height",  required_argument, 0, 'h'},
            {"depth",   required_argument, 0, 'd'},
            {"hsync",   required_argument, 0, 'x'},
            {"vsync",   required_argument, 0, 'y'},
            {"rotate",  required_argument, 0, 'r'},
            {"ignore",  no_argument,       0, 'i'},
            {"modes",   no_argument,       0, 'm'},
            {"show",    no_argument,       0, 's'},
            {"clear",   no_argument,       0, 'c'},
            {"verbose", no_argument,       0, 'v'},
            {"help",    no_argument,       0, 'H'},
            {0, 0,                         0, 0}
    };

    while ((opt = getopt_long(argc, argv, "w:h:d:x:y:r:imscvH", long_options, NULL)) != -1) {
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
            case 'r':
                rotation = safe_atoi(optarg);
                break;
            case 'i':
                ignore_dh = 1;
                break;
            case 'm':
                show_modes = 1;
                break;
            case 's':
                show_info = 1;
                break;
            case 'c':
                clear_screen = 1;
                break;
            case 'v':
                verbose = 1;
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

    if (show_info) {
        show_current_mode();
        return 0;
    }

    if (clear_screen) {
        if (clear_framebuffer() < 0) {
            LOG_ERROR(module, "Failed to clear the framebuffer")
            return 1;
        }
    }

    if (width > 0 || height > 0 || depth > 0 || hsync_len > 0 || vsync_len > 0 || rotation >= 0) {
        if (set_framebuffer(width, height, depth, hsync_len, vsync_len, ignore_dh, rotation) == 0) {
            if (verbose) LOG_SUCCESS(module, "Framebuffer updated successfully")
        } else {
            LOG_ERROR(module, "Failed to update framebuffer configuration")
            return 1;
        }
    }

    return 0;
}
