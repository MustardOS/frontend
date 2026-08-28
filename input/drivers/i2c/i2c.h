#pragma once

#include <stddef.h>
#include <stdint.h>

int i2c_open(const char *path);

void i2c_close(int fd);

int i2c_read_reg(int fd, uint8_t addr7, uint8_t reg, uint8_t *buf, size_t len);
