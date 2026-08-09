#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "rgb_args.h"

static int parse_integer(const char *value, int *result) {
    if (!value || !*value || !result) return -1;
    errno = 0;
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) return -1;
    *result = (int) parsed;
    return 0;
}

static int parse_backend(const char *value, backend_t *backend) {
    if (!value || !backend) return -1;
    if (strcasecmp(value, "auto") == 0)
        *backend = be_auto;
    else if (strcasecmp(value, "sysfs") == 0)
        *backend = be_sysfs;
    else if (strcasecmp(value, "serial") == 0)
        *backend = be_serial;
    else if (strcasecmp(value, "joypad") == 0)
        *backend = be_joypad;
    else
        return -1;
    return 0;
}

static int parse_flag_value(const char *flag, const char *value, flags_t *flags) {
    int *target = NULL;
    if (strcmp(flag, "--dur") == 0)
        target = &flags->dur_all;
    else if (strcmp(flag, "--dur-l") == 0)
        target = &flags->dur_l;
    else if (strcmp(flag, "--dur-r") == 0)
        target = &flags->dur_r;
    else if (strcmp(flag, "--dur-m") == 0)
        target = &flags->dur_m;
    else if (strcmp(flag, "--dur-f1") == 0)
        target = &flags->dur_f1;
    else if (strcmp(flag, "--dur-f2") == 0)
        target = &flags->dur_f2;
    else if (strcmp(flag, "--cycles") == 0)
        target = &flags->cyc_all;
    else if (strcmp(flag, "--cycles-l") == 0)
        target = &flags->cyc_l;
    else if (strcmp(flag, "--cycles-r") == 0)
        target = &flags->cyc_r;
    else if (strcmp(flag, "--cycles-m") == 0)
        target = &flags->cyc_m;
    else if (strcmp(flag, "--cycles-f1") == 0)
        target = &flags->cyc_f1;
    else if (strcmp(flag, "--cycles-f2") == 0)
        target = &flags->cyc_f2;
    return target ? parse_integer(value, target) : -1;
}

int murgb_args_parse(const int argc, char *argv[], murgb_args *args) {
    if (!args) return -1;
    memset(args, 0, sizeof(*args));
    args->backend = be_auto;
    args->flags.dur_all = args->flags.dur_l = args->flags.dur_r = -1;
    args->flags.dur_m = args->flags.dur_f1 = args->flags.dur_f2 = -1;
    args->flags.cyc_all = args->flags.cyc_l = args->flags.cyc_r = INT32_MIN;
    args->flags.cyc_m = args->flags.cyc_f1 = args->flags.cyc_f2 = INT32_MIN;
    if (!argv || argc < 2) return -1;

    if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "restore") == 0) {
        if (argc != 2) {
            args->invalid_argument = argv[2];
            return -1;
        }
        args->command = strcmp(argv[1], "off") == 0 ? rgb_command_off : rgb_command_restore;
        return 0;
    }

    int index = 1;
    while (index < argc) {
        const char *argument = argv[index];
        if (argument[0] != '-' || argument[1] == '\0') break;
        if (strcmp(argument, "--") == 0) {
            index++;
            break;
        }
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            if (argc != 2) {
                args->invalid_argument = argv[2];
                return -1;
            }
            args->command = rgb_command_help;
            return 0;
        }
        if (strcmp(argument, "-b") == 0) {
            if (index + 1 >= argc || parse_backend(argv[index + 1], &args->backend) != 0) {
                args->invalid_argument = index + 1 < argc ? argv[index + 1] : argument;
                return -1;
            }
            index += 2;
            continue;
        }
        if (index + 1 >= argc || parse_flag_value(argument, argv[index + 1], &args->flags) != 0) {
            args->invalid_argument = argument;
            return -1;
        }
        index += 2;
    }

    if (argc - index < 2 || parse_integer(argv[index], &args->mode) != 0
        || parse_integer(argv[index + 1], &args->brightness) != 0) {
        args->invalid_argument = index < argc ? argv[index] : NULL;
        return -1;
    }

    args->value_count = argc - index - 2;
    args->values = argv + index + 2;
    for (int i = 0; i < args->value_count; i++) {
        int value;
        if (parse_integer(args->values[i], &value) != 0) {
            args->invalid_argument = args->values[i];
            return -1;
        }
    }
    return 0;
}
