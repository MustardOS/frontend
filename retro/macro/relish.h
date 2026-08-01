#pragma once

#include "macro.h"

void relish_registry_load(const char *macro_dir);

void relish_registry_finalise(const struct macro_entry *entries, int count, const char *macro_dir);

int relish_registry_lookup(const char *rls_path);

void relish_registry_record(const char *rls_path, int index);

int relish_compile_file(const char *path, struct macro_entry *out_entry);
