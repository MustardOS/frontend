#pragma once

#include <stddef.h>
#include <common/base/options.h>

#define LAB_FILES_MAX      128
#define LAB_NAME_SIZE      256
#define LAB_GLYPH_SIZE     64
#define LAB_SCREENS_MAX    128
#define LAB_GLYPH_LIMIT    128
#define LAB_GRID_LIMIT     256
#define LAB_BOX_LIMIT      480
#define LAB_PREVIEWS       3
#define LAB_DROP_PATH      RUN_STORAGE_PATH "lab"
#define LAB_GLYPH_PATH     LAB_DROP_PATH "/glyph"
#define LAB_ART_PATH       LAB_DROP_PATH "/art"
#define LAB_FONT_PATH      LAB_DROP_PATH "/font"
#define LAB_SHARE_PATH     LAB_DROP_PATH "/share"
#define LAB_MEDIA_PATH     LAB_DROP_PATH "/media"
#define LAB_OVERLAY_PATH   LAB_DROP_PATH "/overlay"
#define LAB_SCREENS        RUN_PATH "lab_screens"
#define LAB_SCHEME_EXPORT  RUN_PATH "lab_scheme.json"
#define LAB_SCREEN_CAPTURE RUN_PATH "dash_screenshot.png"
#define LAB_SCRIPT         OPT_PATH "script/mux/lab.sh"
#define LAB_SHOT_PREFIX    "screenshot: "
#define LAB_SCREEN_SOURCE  "screen"

int lab_wall_apply(const char *theme_folder, const char *slot, const char *source);

int lab_glyph_apply(const char *theme_folder, const char *screen, const char *name, const char *source);

void lab_export_screens(void);

int lab_art_apply(const char *theme_folder, const char *kind, const char *type, const char *name, const char *source);

int lab_font_apply(const char *theme_folder, const char *slot, const char *source);

int lab_share_export(const char *theme_folder, char *out, const size_t out_size);

int lab_share_import(const char *package, char *out, const size_t out_size);

int lab_theme_set_credit(const char *theme_folder, const char *credit);

int lab_media_apply(const char *theme_folder, const char *slot, const char *source);

int lab_music_apply(const char *theme_folder, char (*tracks)[LAB_NAME_SIZE], const int track_count);

int lab_preview_apply(const char *theme_folder, const int slot, const char *source);

int lab_overlay_apply(const char *theme_folder, const char *slot, const char *source);

int lab_scheme_apply(
    const char *theme_folder, const char *screen, const char *section, const char *key, const char *value
);

int lab_scheme_export(const char *theme_folder, const char *screen);

int lab_colour_apply(const char *theme_folder, const char *role, const char *value);

int lab_variant_apply(const char *theme_folder, const char *op, const char *name, const char *source);

int lab_light_apply(const char *theme_folder, const char *side, const char *value);

int lab_splash_apply(const char *theme_folder, const char *field, const char *value);

int lab_scheme_to_json(const char *theme_folder);

void lab_set_target(const char *dim);

int lab_resolution_apply(const char *theme_folder, const char *op, const char *dim);
