#pragma once

#include "../core/libretro.h"

enum vfs_archive_mode { vfs_archive_none, vfs_archive_streamed, vfs_archive_extracted };

bool vfs_bridge_get_interface(struct retro_vfs_interface_info *info);

int vfs_bridge_is_active(void);

enum vfs_archive_mode vfs_bridge_archive_mode(void);

const struct retro_vfs_interface *vfs_bridge_interface(void);
