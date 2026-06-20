#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../options.h"
#include "../init.h"

extern char current_wall[MAX_BUFFER_SIZE];

enum wall_type {
    WALL_APPLICATION,
    WALL_ARCHIVE,
    WALL_GENERAL,
    WALL_TASK
};

struct ImageSettings {
    char *image_path;
    int16_t align;
    int16_t max_width;
    int16_t max_height;
    int16_t pad_left;
    int16_t pad_right;
    int16_t pad_top;
    int16_t pad_bottom;
};

int load_element_image_specifics(const char *mux_dim, const char *program, const char *image_type,
                                 const char *element, const char *element_fallback,
                                 const char *image_extension, char *image_path, size_t path_size);

int load_image_specifics(const char *mux_dim, const char *program, const char *image_type,
                         const char *image_extension, char *image_path, size_t path_size);

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated, int random, int wall_type);

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall,
                    lv_obj_t *ui_imgWall, int wall_type);

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, int wall_type);

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image);

void load_overlay_image_sdl(void);

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image);

int load_terminal_resource(const char *resource, const char *extension, char *buffer, size_t size);

void build_image_array(char *base_image_path);

void load_image_random(lv_obj_t *ui_imgWall, char *base_image_path);

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, int repeat_count, char *current_wall);

void unload_image_animation(void);

void update_image(lv_obj_t *ui_imgobj, struct ImageSettings image_settings);
