#pragma once

#include <stdbool.h>
#include "../mini/mini.h"

enum gen_type {
    SINGLE,
    DIRECTORY,
    PARENT,
    DIRECTORY_NO_WIPE
};

void get_catalogue_name(char *sys_dir, char *content_label, char *catalogue_name, size_t catalogue_name_size);

char *get_catalogue_name_from_rom_path(char *sys_dir, char *content_label);

void create_core_assignment(char *def_core, char *rom_dir, char *core, char *sys, char *cat, char *rom,
                            char *gov, char *con, char *rac, int lookup, enum gen_type method);

bool automatic_assign_core(char *rom_dir);

const char *format_core_name(const char *core, int use_lang);
