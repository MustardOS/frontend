#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <linux/joystick.h>
#include <string.h>
#include <pthread.h>
#include "common.h"
#include "options.h"

#define JOY_HOTKEY JOY_MENU
#define MAX_EVENTS 4

#define DISP_LCD_SET_BRIGHTNESS 0x102
#define DISP_LCD_GET_BRIGHTNESS 0x103

struct disp_bright_value {
    int screen;
    int brightness;
};

int js_fd;
int js_hk;
int hotkey = 0;

int read_brightness() {
    int current_brightness = -1;
    int disp = open("/dev/disp", O_RDWR);

    if (disp >= 0) {
        unsigned long b_val[3];
        memset(b_val, 0, sizeof(b_val));
        b_val[0] = 0;
        current_brightness = ioctl(disp, DISP_LCD_GET_BRIGHTNESS, (void *) b_val);
        close(disp);
    }

    return current_brightness;
}

void set_brightness(int brightness) {
    int disp = open("/dev/disp", O_RDWR);

    if (disp >= 0) {
        unsigned long b_val[3];
        memset(b_val, 0, sizeof(b_val));
        b_val[0] = 0;
        b_val[1] = brightness;
        ioctl(disp, DISP_LCD_SET_BRIGHTNESS, (void *) b_val);
        close(disp);
    }
}

void save_brightness(int brightness) {
    FILE *file = fopen(BL_RST_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%d", brightness);
        fclose(file);
    } else {
        perror("Failed to open brightness restore file");
        exit(1);
    }
}

void read_joystick_events() {
    struct input_event ev;
    int b_new = -1;

    int b_current = read_brightness();

    if (b_new == -1) {
        b_new = b_current;
    }

    while (read(js_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == JS_EVENT_BUTTON && ev.value == 1 && hotkey == 1) {
            if (ev.code == JOY_PLUS) {
                b_new = b_current + BL_INC;
                b_new = (b_new > BL_MAX) ? BL_MAX : b_new;
            } else if (ev.code == JOY_MINUS) {
                b_new = b_current - BL_INC;
                b_new = (b_new < BL_MIN) ? BL_MIN : b_new;
            }
        }
    }

    if (hotkey == 1 && b_new != b_current) {
        set_brightness(b_new);
        save_brightness(b_new);
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

void restore_brightness() {
    int b_default = BL_DEF;
    int b_file;

    FILE *file = fopen(BL_RST_FILE, "r");
    if (file != NULL) {
        fscanf(file, "%d", &b_file);
        fclose(file);
        if (b_file == 0) {
            b_file = b_default;
        } else {
            b_file = (b_file > BL_MAX) ? BL_MAX : b_file;
            set_brightness(b_file);
        }
    } else {
        set_brightness(b_default);
    }
}

void *get_hotkey_task(void *arg) {
    struct input_event ev;

    while (1) {
        while (read(js_hk, &ev, sizeof(struct input_event)) > 0) {
            if (ev.type == JS_EVENT_BUTTON && ev.code == JOY_HOTKEY) {
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
    restore_brightness();

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
