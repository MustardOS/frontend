#pragma once

#include "../mini/mini.h"

enum gen_type { casn_single, casn_dir, casn_parent, casn_dir_nowipe };

void get_catalogue_name(char *sys_dir, const char *content_label, char *catalogue_name, size_t catalogue_name_size);

char *get_catalogue_name_from_rom_path(char *sys_dir, char *content_label);

void create_core_assignment(
    char *def_core, char *rom_dir, char *core, char *sys, char *cat, char *rom, char *gov, char *con, char *rac,
    int lookup, enum gen_type method
);

int automatic_assign_core(char *rom_dir);

int core_uses_muxretro(const char *assign_dir, const char *item_name);

int core_external_uses_stage_overlay(const char *core);

const char *format_core_name(const char *core, int use_lang, int is_muxretro);

int remove_content_config(const char *name, const char *core);

int remove_dir_config(const char *dir, const char *core);

int remove_core_config(const char *core);

int remove_muxretro_content_config(const char *content_name);

int remove_muxretro_dir_config(const char *dir);

int remove_muxretro_core_config(const char *core);
