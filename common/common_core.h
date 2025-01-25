#pragma once

#include "mini/mini.h"

enum core_gen_type {
    SINGLE,
    DIRECTORY,
    PARENT,
    DIRECTORY_NO_WIPE
};

void get_catalogue_name(char *sys_dir, char *content_label, char *catalogue_name, size_t catalogue_name_size);

char *get_catalogue_name_from_rom_path(char *sys_dir, char *content_label);

bool automatic_assign_core(char *rom_dir);
