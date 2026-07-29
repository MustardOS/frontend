#pragma once

typedef enum { screenshot_auto = 0, screenshot_fbdev, screenshot_drm } screenshot_mode;

typedef struct {
    int red;
    int green;
    int blue;
} screenshot_hue;

int screenshot_save(const char *path, screenshot_mode mode, screenshot_hue hue);

struct SDL_Renderer;

// Grabs the frame from the renderer so it never has to go on the screen first
int screenshot_save_renderer(struct SDL_Renderer *renderer, const char *path, screenshot_hue hue);
