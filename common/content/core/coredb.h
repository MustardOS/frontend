#pragma once

#include <stddef.h>

enum core_runtime { core_runtime_pickles = 0, core_runtime_retroarch, core_runtime_external, core_runtime_count };

#define COREDB_NAME_MAX 128
#define COREDB_PATH_MAX 256

struct coredb_core {
    char id[COREDB_NAME_MAX];
    char name[COREDB_NAME_MAX];
    char core[COREDB_NAME_MAX];
    char launcher[COREDB_NAME_MAX];
    char governor[COREDB_NAME_MAX];
    char control[COREDB_NAME_MAX];
    enum core_runtime runtime;
    int bios_required;
};

struct coredb_system {
    char id[COREDB_NAME_MAX];
    char name[COREDB_NAME_MAX];
    char name_space[COREDB_NAME_MAX];
};

int coredb_load(void);

void coredb_free(void);

const char *coredb_runtime_label(enum core_runtime runtime);

void coredb_assign_tag(const char *id, enum core_runtime runtime, char *out, size_t out_size);

enum core_runtime coredb_assign_runtime(const char *stored, const char **id_out);

int coredb_assign_resolve(const char *system, const char *stored, char *id_out, size_t id_size,
                          enum core_runtime *runtime_out);

int coredb_namespace_count(void);

const char *coredb_namespace_at(int index);

int coredb_system_count(const char *name_space);

int coredb_system_at(const char *name_space, int index, struct coredb_system *out);

int coredb_system_namespace(const char *system, char *out, size_t out_size);

int coredb_system_default(const char *system, char *out, size_t out_size);

int coredb_system_catalogue(const char *system, char *out, size_t out_size);

int coredb_system_lookup(const char *system);

int coredb_system_governor(const char *system, char *out, size_t out_size);

int coredb_system_control(const char *system, char *out, size_t out_size);

int coredb_core_count(const char *system, enum core_runtime runtime);

int coredb_core_at(const char *system, enum core_runtime runtime, int index, struct coredb_core *out);

int coredb_core_find(const char *system, enum core_runtime runtime, const char *id, struct coredb_core *out);

int coredb_runtime_available(const char *system, enum core_runtime runtime);
