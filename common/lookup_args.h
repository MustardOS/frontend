#pragma once

typedef enum {
    lookup_generate_none = 0,
    lookup_generate_all,
    lookup_generate_internal,
    lookup_generate_global,
    lookup_generate_folder
} lookup_generate_mode;

typedef struct {
    lookup_generate_mode generate;
    char *folder;
    char *term;
    const char *invalid_argument;
} mulookup_args;

int mulookup_args_parse(int argc, char *argv[], mulookup_args *args);
