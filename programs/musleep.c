#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/epoll.h>
#include "common.h"
#include "options.h"

int main() {
    int js_fd = open(SYS_DEVICE, O_RDONLY | O_NONBLOCK);

    if (js_fd == -1) {
        perror("Failed to open joystick device");
        exit(1);
    }

    int epoll_fd = epoll_init(js_fd);
    struct epoll_event events[1];

    while (1) {
        for (int i = 0; i < epoll_wait_events(epoll_fd, events, 1); ++i) {
            if (events[i].data.fd == js_fd && (events[i].events & EPOLLIN)) {
                struct input_event ev;
                read(js_fd, &ev, sizeof(struct input_event));
                if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 2) {
                    usleep(100000);
                    char *const command[] = {"sh", "-c", "echo freeze > /sys/power/state", NULL};
                    execvp(command[0], command);
                    exit(EXIT_SUCCESS);
                }
            }
        }
    }
}
