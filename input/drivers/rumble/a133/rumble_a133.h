#pragma once

#define RUMBLE_A133_GPIO_PIN 227

struct rumble_a133_hw {
    int value_fd;
    int initialised;
};

int rumble_a133_initialise(struct rumble_a133_hw *hw);

int rumble_a133_set(struct rumble_a133_hw *hw, int on);

void rumble_a133_close(struct rumble_a133_hw *hw);
