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
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "common.h"
#include "options.h"

#define MAX_EVENTS 4

int js_fd;

int png_index = 0;
int R2_pressed = 0;
int L2_pressed = 0;

char prev_time_str[16];

void read_joystick_events() {
    struct input_event ev;
    char time_str[16];
    char ss_path[64];
    char command[256];
    time_t current_time;
    struct tm* time_info;

    while (read(js_fd, &ev, sizeof(struct input_event)) > 0) {
        current_time = time(NULL);
        time_info = localtime(&current_time);
        strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M", time_info);

        if (strcmp(time_str, prev_time_str) != 0) {
            strcpy(prev_time_str, time_str);
            png_index = 0;
        }

        if (ev.type == EV_KEY && ev.value == 1) {
            if (ev.code == JOY_R2) {
                R2_pressed = 1;
            }
            if (ev.code == JOY_L2) {
                L2_pressed = 1;
            }
        } else {
            R2_pressed = L2_pressed = 0;
        }

        if (R2_pressed && L2_pressed) {
            set_rumble_level(50);
            R2_pressed = L2_pressed = 0;
            snprintf(ss_path, sizeof(ss_path), "/mnt/mmc/MUOS/screenshot/muOS_%s_%d.png", time_str, png_index);
            snprintf(command, sizeof(command), "fbgrab -a %s", ss_path);
            system(command);
            png_index++;
            set_rumble_level(0);
            sleep(1);
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

    close(epoll_fd);

    return 0;
}
