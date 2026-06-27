#pragma once

typedef enum { screenshot_auto = 0, screenshot_fbdev, screenshot_drm } screenshot_mode;

typedef struct {
    int red;
    int green;
    int blue;
} screenshot_hue;

int screenshot_save(const char *path, screenshot_mode mode, screenshot_hue hue);
