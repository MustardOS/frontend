#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "../common/fbset_args.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/log.h"

static char *module = "fbset";
int verbose = 0;

int clear_framebuffer(void) {
    int fb_fd;

    struct fb_fix_screeninfo f_info;
    struct fb_var_screeninfo v_info;

    fb_fd = open(device.screen.device, O_RDWR);
    if (fb_fd < 0) {
        LOG_ERROR(module, "Error opening framebuffer device");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &f_info) < 0) {
        LOG_ERROR(module, "Error retrieving fixed screen info");
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error retrieving variable screen info");
        close(fb_fd);
        return -1;
    }

    size_t fb_size = f_info.line_length * v_info.yres;
    void *fb_mem = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        LOG_ERROR(module, "Error mapping framebuffer memory");
        close(fb_fd);
        return -1;
    }

    memset(fb_mem, 0, fb_size);
    munmap(fb_mem, fb_size);

    close(fb_fd);

    if (verbose) LOG_SUCCESS(module, "Framebuffer cleared successfully");

    return 0;
}

void print_available_modes(void) {
    const char *sys_modes = "/sys/class/graphics/fb0/modes";

    FILE *modes_file = fopen(sys_modes, "r");
    if (!modes_file) {
        LOG_ERROR(module, "Unable to read available modes from sysfs");
        return;
    }

    LOG_INFO(module, "Available Modes:");

    char mode[64];
    while (fgets(mode, sizeof(mode), modes_file))
        LOG_INFO(module, "  %s", mode);

    fclose(modes_file);
}

void show_current_mode(void) {
    struct fb_var_screeninfo v_info;
    int fb_fd = open(device.screen.device, O_RDONLY);

    if (fb_fd < 0) {
        LOG_ERROR(module, "Error opening framebuffer device");
        return;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error retrieving variable screen info");
        close(fb_fd);
        return;
    }

    LOG_INFO(
        module, "Current Mode: %dx%d (%dx%d virtual), %dbpp", v_info.xres, v_info.yres, v_info.xres_virtual,
        v_info.yres_virtual, v_info.bits_per_pixel
    );

    LOG_INFO(module, "Timing: hsync=%d, vsync=%d, rotate=%d", v_info.hsync_len, v_info.vsync_len, v_info.rotate);

    close(fb_fd);
}

int set_framebuffer(int width, int height, int depth, int hsync_len, int vsync_len, int ignore_dh, int rotation) {
    struct fb_var_screeninfo v_info, verify;
    int fb_fd;

    fb_fd = open(device.screen.device, O_RDWR);
    if (fb_fd < 0) {
        LOG_ERROR(module, "Error opening framebuffer device");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v_info) < 0) {
        LOG_ERROR(module, "Error retrieving variable screen info");
        close(fb_fd);
        return -1;
    }

    if (verbose) LOG_INFO(module, "Current resolution: %dx%d, %dbpp", v_info.xres, v_info.yres, v_info.bits_per_pixel);

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
        LOG_ERROR(module, "Error setting variable screen info");
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &verify) == 0) {
        if (verify.xres != v_info.xres || verify.yres != v_info.yres) {
            LOG_WARN(module, "Hardware adjusted the mode: got %dx%d instead", verify.xres, verify.yres);
        }
    }

    if (verbose) {
        LOG_INFO(module, "Updated resolution: %dx%d, %dbpp", v_info.xres, v_info.yres, v_info.bits_per_pixel);
        LOG_INFO(module, "Timing: hsync=%d, vsync=%d, rotate=%d", v_info.hsync_len, v_info.vsync_len, v_info.rotate);
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
    printf("  -g, --grab   <FILE>        Save screenshot to PNG\n");
    printf("  -M, --method <auto|fbdev|drm>  Screenshot capture method\n");
    printf("  -v, --verbose              Verbose output\n");
    printf("  -H, --help                 Show this help\n");

    printf("\nExamples:\n");
    printf("  %s -w 640 -h 480 -d 32 -c\n", prog);
    printf("  %s -g /mnt/mmc/screen.png\n", prog);
    printf("  %s -g /mnt/mmc/screen.png -M drm\n\n", prog);
}

int main(int argc, char *argv[]) {
    mufbset_args args;
    if (mufbset_args_parse(argc, argv, &args) != 0) {
        if (args.invalid_argument) fprintf(stderr, "Invalid argument: %s\n", args.invalid_argument);
        print_help(argv[0]);
        return 1;
    }
    if (args.help) {
        print_help(argv[0]);
        return 0;
    }
    verbose = args.verbose;

    load_device(&device);
    load_config(&config);

    if (args.grab_path) {
        screenshot_hue hue = {
            .red = device.colour.red,
            .green = device.colour.green,
            .blue = device.colour.blue,
        };

        if (screenshot_save(args.grab_path, args.grab_mode, hue) < 0) {
            LOG_ERROR(module, "Failed to capture screenshot");
            return 1;
        }

        if (verbose) LOG_SUCCESS(module, "Screenshot saved to %s", args.grab_path);

        return 0;
    }

    if (args.show_modes) {
        print_available_modes();
        return 0;
    }

    if (args.show_info) {
        show_current_mode();
        return 0;
    }

    if (args.clear_screen) {
        if (clear_framebuffer() < 0) {
            LOG_ERROR(module, "Failed to clear the framebuffer");
            return 1;
        }
    }

    if (args.width > 0 || args.height > 0 || args.depth > 0 || args.hsync_len > 0 || args.vsync_len > 0
        || args.rotation >= 0) {
        if (set_framebuffer(
                args.width, args.height, args.depth, args.hsync_len, args.vsync_len, args.ignore_double_height,
                args.rotation
            )
            == 0) {
            if (verbose) LOG_SUCCESS(module, "Framebuffer updated successfully");
        } else {
            LOG_ERROR(module, "Failed to update framebuffer configuration");
            return 1;
        }
    }

    return 0;
}
