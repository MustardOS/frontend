#pragma once
#include <stdint.h>

#define SUNXI_GPIO_INPUT  0
#define SUNXI_GPIO_OUTPUT 1

int sunxi_gpio_initialise(void);

void sunxi_gpio_close(void);

int sunxi_gpio_input(uint32_t pin);

int sunxi_gpio_set_cfgpin(uint32_t pin, uint32_t val);
