#pragma once

typedef enum {
    SCREENSHOT_AUTO = 0,
    SCREENSHOT_FBDEV,
    SCREENSHOT_DRM
} screenshot_mode;

typedef struct {
    int red;
    int green;
    int blue;
} screenshot_hue;

int screenshot_save(const char *path, screenshot_mode mode, screenshot_hue hue);
