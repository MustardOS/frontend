#pragma once

#include <stddef.h>

typedef enum {
    wasabi_asset_filter = 0,
    wasabi_asset_shader,
    wasabi_asset_overlay,
    wasabi_asset_kind_count
} wasabi_asset_kind;

typedef enum {
    wasabi_asset_row_download = 0,
    wasabi_asset_row_collection,
    wasabi_asset_row_none,
    wasabi_asset_row_directory,
    wasabi_asset_row_item,
    wasabi_asset_row_empty
} wasabi_asset_row_type;

void wasabi_assets_refresh(wasabi_asset_kind kind);
int wasabi_assets_count(wasabi_asset_kind kind);
const char *wasabi_asset_key(wasabi_asset_kind kind, int index);
const char *wasabi_asset_label(wasabi_asset_kind kind, int index);
const char *wasabi_asset_path(wasabi_asset_kind kind, int index);
const char *wasabi_asset_display_value(wasabi_asset_kind kind);
int wasabi_asset_selected(wasabi_asset_kind kind);
int wasabi_asset_find(wasabi_asset_kind kind, const char *key);
int wasabi_asset_preview(wasabi_asset_kind kind, int index);
int wasabi_asset_select(wasabi_asset_kind kind, int index);
void wasabi_asset_browser_open(wasabi_asset_kind kind);
int wasabi_asset_browser_count(void);
wasabi_asset_row_type wasabi_asset_browser_type(int row);
const char *wasabi_asset_browser_label(int row);
const char *wasabi_asset_browser_glyph(int row);
int wasabi_asset_browser_item(int row);
int wasabi_asset_browser_focus(void);
int wasabi_asset_browser_enter(int row);
int wasabi_asset_browser_back(void);
int wasabi_asset_browser_collected(int row);
int wasabi_asset_browser_toggle_collection(int row);
int wasabi_asset_browser_removable(int row);
int wasabi_asset_browser_delete(int row);
