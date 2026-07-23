#pragma once

#include <stddef.h>
#include "../core/libretro.h"

void core_input_meta_set_controller_info(const struct retro_controller_info *info);

void core_input_meta_set_input_descriptors(const struct retro_input_descriptor *descriptors);

void core_input_meta_clear(void);

int core_input_meta_port_type_count(int port);

int core_input_meta_port_type_get(int port, int index, char *desc_out, size_t desc_len, unsigned *id_out);

const char *core_input_meta_label(unsigned port, unsigned device, unsigned index, unsigned id);
