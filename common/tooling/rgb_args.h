#pragma once

typedef enum { be_auto, be_sysfs, be_serial, be_joypad } backend_t;

typedef struct {
    int dur_all;
    int dur_l;
    int dur_r;
    int dur_m;
    int dur_f1;
    int dur_f2;
    int cyc_all;
    int cyc_l;
    int cyc_r;
    int cyc_m;
    int cyc_f1;
    int cyc_f2;
} flags_t;

typedef enum { rgb_command_wire = 0, rgb_command_off, rgb_command_restore, rgb_command_help } rgb_command;

typedef struct {
    rgb_command command;
    backend_t backend;
    flags_t flags;
    int mode;
    int brightness;
    int value_count;
    char **values;
    const char *invalid_argument;
} murgb_args;

int murgb_args_parse(int argc, char *argv[], murgb_args *args);
