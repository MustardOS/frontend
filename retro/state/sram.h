#pragma once

void sram_bridge_init(const char *core_path_arg, const char *content_path);

void sram_bridge_save(void);

void sram_bridge_shutdown(void);
