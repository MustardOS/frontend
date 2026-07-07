#pragma once

#include "libretro.h"

bool rumble_bridge_get_interface(struct retro_rumble_interface *iface);

void rumble_bridge_shutdown(void);
