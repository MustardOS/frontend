#pragma once

int gpio_export(int pin);

int gpio_set_direction(int pin, int is_output);

int gpio_write(int pin, int value);

int gpio_read(int pin, int *value_out);
