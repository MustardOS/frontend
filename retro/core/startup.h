#pragma once

#include <stdint.h>
#include <stdio.h>

typedef struct startup_options {
    const char *core_path;
    const char *content_path;
    int fresh;
    int restarting;
    int netplay_host;
    int netplay_invalid;
    const char *unknown_option;
    char netplay_address[256];
    uint16_t netplay_port;
} startup_options;

void startup_options_print_usage(FILE *stream, const char *program);

int startup_options_parse(int argc, char *argv[], startup_options *options);
