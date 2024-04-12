#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <png.h>
#include "common.h"
#include "options.h"
#include "de_atm7059.h"

#define	DE         (0xB02E0000)
#define	DE_SIZE    (0x00001000)
#define MAX_XRES   2560
#define MIN_XRES   64
#define MAX_EVENTS 4

int fd_mem;
int js_fd;
int png_index = 0;
int volup_pressed = 0;
int power_pressed = 0;

char storage_location[8];
char prev_time_str[16];

uint32_t *de_map;
volatile uint32_t *de_mem;

void take_screenshot(char* screenshot_location) {
	uint32_t *buffer, *src;
	uint16_t *src16;
	uint32_t linebuffer[MAX_XRES], xres, yres, ofs, x, y, pix, bpp;
	
	png_structp png_ptr;
	png_infop info_ptr;

	ofs  = de_mem[DE_OVL_BA0(0)/4];
	xres = (de_mem[DE_OVL_ISIZE(0)/4] & 0xFFFF) +1;
	yres = (de_mem[DE_OVL_ISIZE(0)/4] >> 16) +1;
	bpp  = (!(de_mem[DE_OVL_CFG(0)/4] & 0x7))?2:4;

	if ((xres > MAX_XRES)||(xres < MIN_XRES)) return;

	if ((buffer = (uint32_t*)malloc(xres * yres * bpp)) != NULL) {
		lseek(fd_mem, ofs, SEEK_SET);
		read(fd_mem, buffer, xres * yres * bpp);

		FILE *fp = fopen(screenshot_location, "wb");
		if (fp != NULL) {
			set_rumble_level(35);
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
			info_ptr = png_create_info_struct(png_ptr);
			png_init_io(png_ptr, fp);
			png_set_IHDR(png_ptr, info_ptr, xres, yres, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			png_write_info(png_ptr, info_ptr);
			if (bpp == 4) {
				src = buffer;
				for (y=0; y < yres; y++) {
					for (x=0; x < xres; x++){
						pix = *src++;
						linebuffer[x] = 0xFF000000 | (pix & 0x0000FF00) | (pix & 0x00FF0000)>>16 | (pix & 0x000000FF)<<16;
					}
					png_write_row(png_ptr, (png_bytep)linebuffer);
				}
			} else {
				src16 = (uint16_t*)buffer;
				for (y=0; y < yres; y++) {
					for (x=0; x < xres; x++){
						pix = *src16++;
						linebuffer[x] = 0xFF000000 |
							(pix & 0xF100)>>8  | (pix & 0xE000)>>13 |
							(pix & 0x07E0)<<5  | (pix & 0x0600)>>1  |
							(pix & 0x001F)<<19 | (pix & 0x001C)<<14 ;
					}
					png_write_row(png_ptr, (png_bytep)linebuffer);
				}
			}
			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(fp);
			sync();
			set_rumble_level(0);
		}
		free(buffer);
	}
}

void read_joystick_events() {
    struct js_event ev;
    char time_str[16];
    char ss_path[64];
    time_t current_time;
    struct tm* time_info;

    while (read(js_fd, &ev, sizeof(struct js_event)) > 0) {
        current_time = time(NULL);
        time_info = localtime(&current_time);
        strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M", time_info);

        if (strcmp(time_str, prev_time_str) != 0) {
            strcpy(prev_time_str, time_str);
            png_index = 0;
        }

        switch (ev.type) {
            case JS_EVENT_BUTTON:
                if (ev.number == JOY_PLUS) {
                    volup_pressed = ev.value;
                }
                if (ev.number == JOY_POWER) {
                    power_pressed = ev.value;
                }
                break;
            default:
                break;
        }

        if (volup_pressed && power_pressed) {
            snprintf(ss_path, sizeof(ss_path), "/mnt/mmc/MUOS/screenshot/muOS_%s_%d.png", time_str, png_index);
            take_screenshot(ss_path);
            png_index++;
        }
    }
}

void open_joystick_device() {
    js_fd = open(JOY_DEVICE, O_RDONLY | O_NONBLOCK);

    if (js_fd == -1) {
        perror("Failed to open joystick device");
        exit(1);
    }
}

int main() {
	fd_mem = open("/dev/mem", O_RDONLY);
	de_map = mmap(0, DE_SIZE, PROT_READ, MAP_SHARED, fd_mem, DE);
	de_mem = de_map;

    setup_background_process();
    open_joystick_device();
    
    int epoll_fd = epoll_init(js_fd);
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int num_events = epoll_wait_events(epoll_fd, events, MAX_EVENTS);

        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == js_fd && (events[i].events & EPOLLIN)) {
                read_joystick_events();
            }
        }
    }

    munmap(de_map, DE_SIZE);

    close(epoll_fd);
	close(fd_mem);

    return 0;
}
