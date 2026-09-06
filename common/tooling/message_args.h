#pragma once

typedef struct {
    int progress;
    int delay;
    char *message;
    char *live_file;
    const char *unknown_option;
} muxmessage_args;

int muxmessage_args_parse(int argc, char *argv[], muxmessage_args *args);
