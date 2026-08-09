#include <string.h>
#include "lookup_args.h"

#define LOOKUP_ARGUMENT_MAX 1023

int mulookup_args_parse(const int argc, char *argv[], mulookup_args *args) {
    if (!args) return -1;
    memset(args, 0, sizeof(*args));
    if (!argv || argc < 2) return -1;

    int positional = 0;
    for (int i = 1; i < argc; i++) {
        if (!positional && strcmp(argv[i], "--") == 0) {
            positional = 1;
            continue;
        }
        if (!positional && strcmp(argv[i], "--gen-all") == 0) {
            args->generate = lookup_generate_all;
            continue;
        }
        if (!positional && strcmp(argv[i], "--gen-internal") == 0) {
            args->generate = lookup_generate_internal;
            continue;
        }
        if (!positional && strcmp(argv[i], "--gen-global") == 0) {
            args->generate = lookup_generate_global;
            continue;
        }
        if (!positional && strcmp(argv[i], "--gen-folder") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0] || strlen(argv[i + 1]) > LOOKUP_ARGUMENT_MAX) {
                args->invalid_argument = argv[i];
                return -1;
            }
            args->generate = lookup_generate_folder;
            args->folder = argv[++i];
            continue;
        }
        if (!positional && (strcmp(argv[i], "--folder") == 0 || strcmp(argv[i], "-f") == 0)) {
            if (i + 1 >= argc || !argv[i + 1][0] || strlen(argv[i + 1]) > LOOKUP_ARGUMENT_MAX) {
                args->invalid_argument = argv[i];
                return -1;
            }
            args->folder = argv[++i];
            continue;
        }
        if (!positional && argv[i][0] == '-') {
            args->invalid_argument = argv[i];
            return -1;
        }
        if (args->term || !argv[i][0] || strlen(argv[i]) > LOOKUP_ARGUMENT_MAX) {
            args->invalid_argument = argv[i];
            return -1;
        }
        args->term = argv[i];
    }

    if (args->generate == lookup_generate_folder && !args->folder) return -1;
    if (args->generate != lookup_generate_none) return 0;
    return args->term ? 0 : -1;
}
