#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "message_args.h"

static int parse_integer(const char *value, const int fallback) {
    if (!value || !*value) return fallback;
    errno = 0;
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) return fallback;
    return (int) parsed;
}

int muxmessage_args_parse(const int argc, char *argv[], muxmessage_args *args) {
    if (!args) return -1;

    memset(args, 0, sizeof(*args));
    args->progress = -1;

    if (!argv || argc < 2) return -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                args->unknown_option = argv[i];
                return -1;
            }
            args->delay = parse_integer(argv[++i], 0);
        } else if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                args->unknown_option = argv[i];
                return -1;
            }
            args->live_file = argv[++i];
        } else if (args->progress == -1) {
            args->progress = parse_integer(argv[i], 0);
        } else if (!args->message) {
            args->message = argv[i];
        } else {
            args->unknown_option = argv[i];
            return -1;
        }
    }

    return args->progress != -1 && (args->message || args->live_file) ? 0 : -1;
}
