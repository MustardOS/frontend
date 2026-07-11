#pragma once

#include "../core/libretro.h"

bool vfs_bridge_get_interface(struct retro_vfs_interface_info *info);

int vfs_bridge_is_active(void);
