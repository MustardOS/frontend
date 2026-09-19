#pragma once

enum preset_catalogue_kind { preset_catalogue_filter = 0, preset_catalogue_shader, preset_catalogue_overlay };

void preset_catalogue_init(void);

void preset_catalogue_open(enum preset_catalogue_kind kind);

int preset_catalogue_tick(void);
