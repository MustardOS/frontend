#include "i2c.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int i2c_open(const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        return -1;
    }
    ioctl(fd, I2C_TIMEOUT, 5);
    ioctl(fd, I2C_RETRIES, 1);
    return fd;
}

void i2c_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int i2c_read_reg(int fd, uint8_t addr7, uint8_t reg, uint8_t *buf, size_t len) {
    if (fd < 0 || !buf) {
        return -1;
    }

    uint8_t reg_buf[1] = {reg};
    struct i2c_msg msgs[2] = {
        {.addr = addr7, .flags = 0, .len = 1, .buf = reg_buf},
        {.addr = addr7, .flags = I2C_M_RD, .len = (__u16) len, .buf = buf},
    };
    struct i2c_rdwr_ioctl_data xfer = {.msgs = msgs, .nmsgs = 2};

    memset(buf, 0, len);
    if (ioctl(fd, I2C_RDWR, &xfer) < 0) {
        return -1;
    }
    return 0;
}
