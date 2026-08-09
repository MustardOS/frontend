#pragma once

#include "screenshot.h"

typedef struct {
    int width;
    int height;
    int depth;
    int hsync_len;
    int vsync_len;
    int ignore_double_height;
    int show_modes;
    int show_info;
    int clear_screen;
    int rotation;
    int verbose;
    int help;
    const char *grab_path;
    screenshot_mode grab_mode;
    const char *invalid_argument;
} mufbset_args;

int mufbset_args_parse(int argc, char *argv[], mufbset_args *args);
