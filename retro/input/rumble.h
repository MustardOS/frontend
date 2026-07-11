#pragma once

#include "../core/libretro.h"

bool rumble_bridge_get_interface(struct retro_rumble_interface *iface);

void rumble_bridge_shutdown(void);

void rumble_bridge_set_suppressed(int suppressed);
