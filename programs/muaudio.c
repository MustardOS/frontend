#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include "common.h"
#include "options.h"

#define JOY_HOTKEY JOY_MENU
#define MAX_EVENTS 4

int js_fd;
int js_hk;
int hotkey = 0;
int vol_muted = 0;
int vol_new = -1;

void read_joystick_events() {
    struct input_event ev;

    int vol_current = get_volume(VOL_SPK_MASTER);
    if (vol_new == -1)
        vol_new = vol_current;

    while (read(js_fd, &ev, sizeof(struct input_event)) > 0) {
        if (hotkey == 1) {
            if (ev.type == EV_KEY && ev.value == 1 && ev.code == JOY_DOWN) {
                vol_muted = (vol_muted == 1) ? 0 : 1;
#ifdef RG35XX
                set_speaker_power(VOL_SPK_SWITCH, vol_muted);
#endif
#ifdef RG35XXPLUS
                set_volume(VOL_SPK_MASTER, 0);
#endif
            }
        } else {
            if (ev.type == EV_KEY && ev.value == 1) {
                if (ev.code == JOY_PLUS) {
                    vol_new = vol_current + VOL_INC;
                    vol_new = (vol_new > VOL_MAX) ? VOL_MAX : vol_new;
                } else if (ev.code == JOY_MINUS) {
                    vol_new = vol_current - VOL_INC;
                    vol_new = (vol_new < VOL_MIN) ? VOL_MIN : vol_new;
                }
            }
        }
    }

    if (hotkey == 0 && vol_new != vol_current) {
        set_volume(VOL_SPK_MASTER, vol_new);
    }
}

void open_joystick_device() {
    js_fd = open(JOY_DEVICE, O_RDONLY | O_NONBLOCK);
    js_hk = open(JOY_DEVICE, O_RDONLY | O_NONBLOCK);

    if (js_fd == -1 || js_hk == -1) {
        perror("Failed to open joystick device");
        exit(1);
    }
}

void *get_hotkey_task(void *arg) {
    struct input_event ev;

    while (1) {
        while (read(js_hk, &ev, sizeof(struct input_event)) > 0) {
            if (ev.type == EV_KEY && ev.code == JOY_HOTKEY) {
                hotkey = ev.value == 1 ? 1 : 0;
            }
        }

        usleep(100000);
    }

    return NULL;
}

int main() {
    setup_background_process();
    open_joystick_device();

    pthread_t get_hotkey_thread;
    if (pthread_create(&get_hotkey_thread, NULL, get_hotkey_task, NULL) != 0) {
        perror("Failed to create get hotkey thread");
        exit(1);
    }

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

    pthread_join(get_hotkey_thread, NULL);
    close(epoll_fd);

    return 0;
}
