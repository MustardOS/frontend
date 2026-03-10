#pragma once

typedef enum {
    SCREENSHOT_AUTO = 0,
    SCREENSHOT_FBDEV,
    SCREENSHOT_DRM
} screenshot_mode;

int screenshot_save(const char *path, screenshot_mode mode);
