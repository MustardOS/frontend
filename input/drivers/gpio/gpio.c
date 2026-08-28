#include "gpio.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int write_value(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t res = write(fd, value, strlen(value));
    close(fd);
    return res < 0 ? -1 : 0;
}

int gpio_export(int pin) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", pin);
    if (write_value("/sys/class/gpio/export", buf) < 0) {
        if (errno != EBUSY) {
            perror("gpio_export");
            return -1;
        }
    }
    return 0;
}

int gpio_set_direction(int pin, int is_output) {
    const char *dir = is_output ? "out" : "in";
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    if (write_value(path, dir) < 0) {
        perror("gpio_set_direction");
        return -1;
    }
    return 0;
}

int gpio_write(int pin, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    char v = value ? '1' : '0';
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("gpio_write");
        return -1;
    }
    if (write(fd, &v, 1) < 0) {
        perror("gpio_write");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int gpio_read(int pin, int *value_out) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("gpio_read");
        return -1;
    }
    char v = '0';
    ssize_t r = read(fd, &v, 1);
    close(fd);
    if (r <= 0) {
        perror("gpio_read");
        return -1;
    }
    *value_out = (v == '0') ? 0 : 1;
    return 0;
}
