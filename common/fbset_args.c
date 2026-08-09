#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "fbset_args.h"

static int parse_integer(const char *value, int *result) {
    if (!value || !*value || !result) return -1;
    errno = 0;
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) return -1;
    *result = (int) parsed;
    return 0;
}

int mufbset_args_parse(const int argc, char *argv[], mufbset_args *args) {
    if (!args) return -1;
    memset(args, 0, sizeof(*args));
    args->ignore_double_height = 2;
    args->rotation = -1;
    args->grab_mode = screenshot_auto;
    if (!argv || argc < 1) return -1;

    static const struct option options[] = {
        {"width", required_argument, NULL, 'w'},   {"height", required_argument, NULL, 'h'},
        {"depth", required_argument, NULL, 'd'},  {"hsync", required_argument, NULL, 'x'},
        {"vsync", required_argument, NULL, 'y'},  {"rotate", required_argument, NULL, 'r'},
        {"ignore", no_argument, NULL, 'i'},       {"modes", no_argument, NULL, 'm'},
        {"show", no_argument, NULL, 's'},         {"clear", no_argument, NULL, 'c'},
        {"grab", required_argument, NULL, 'g'},   {"method", required_argument, NULL, 'M'},
        {"verbose", no_argument, NULL, 'v'},      {"help", no_argument, NULL, 'H'},
        {NULL, 0, NULL, 0}
    };

    optind = 1;
    opterr = 0;
    int option;
    while ((option = getopt_long(argc, argv, "w:h:d:x:y:r:imscg:M:vH", options, NULL)) != -1) {
        int *target = NULL;
        switch (option) {
            case 'w': target = &args->width; break;
            case 'h': target = &args->height; break;
            case 'd': target = &args->depth; break;
            case 'x': target = &args->hsync_len; break;
            case 'y': target = &args->vsync_len; break;
            case 'r': target = &args->rotation; break;
            case 'i': args->ignore_double_height = 1; break;
            case 'm': args->show_modes = 1; break;
            case 's': args->show_info = 1; break;
            case 'c': args->clear_screen = 1; break;
            case 'g': args->grab_path = optarg; break;
            case 'M':
                if (strcmp(optarg, "auto") == 0)
                    args->grab_mode = screenshot_auto;
                else if (strcmp(optarg, "fbdev") == 0)
                    args->grab_mode = screenshot_fbdev;
                else if (strcmp(optarg, "drm") == 0)
                    args->grab_mode = screenshot_drm;
                else {
                    args->invalid_argument = optarg;
                    return -1;
                }
                break;
            case 'v': args->verbose = 1; break;
            case 'H': args->help = 1; break;
            default:
                args->invalid_argument = optind > 0 && optind <= argc ? argv[optind - 1] : NULL;
                return -1;
        }
        if (target && parse_integer(optarg, target) != 0) {
            args->invalid_argument = optarg;
            return -1;
        }
    }

    if (optind != argc) {
        args->invalid_argument = argv[optind];
        return -1;
    }
    if (args->rotation < -1 || args->rotation > 3) {
        args->invalid_argument = "rotation";
        return -1;
    }
    return 0;
}
