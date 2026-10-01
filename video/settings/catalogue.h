#pragma once

#include "assets.h"

int wasabi_catalogue_open(wasabi_asset_kind kind);
int wasabi_catalogue_active(void);
int wasabi_catalogue_count(void);
const char *wasabi_catalogue_label(int row);
const char *wasabi_catalogue_value(int row);
const char *wasabi_catalogue_glyph(int row);
const char *wasabi_catalogue_action(int row);
int wasabi_catalogue_confirm(int row);
int wasabi_catalogue_back(void);
unsigned wasabi_catalogue_revision(void);
void wasabi_catalogue_tick(void);
