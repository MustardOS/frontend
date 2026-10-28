#pragma once

#include <stddef.h>

#define CPU_POLICY_MAX   8
#define CPU_FREQ_MAX     64
#define CPU_GOVERNOR_MAX 32
#define CPU_PATH_MAX     256

#define CPU_PROFILE_SESSION "/run/muos/cpu_profile"

enum cpu_profile_id {
    cpu_profile_automatic = 0,
    cpu_profile_powersave,
    cpu_profile_efficient,
    cpu_profile_balanced,
    cpu_profile_responsive,
    cpu_profile_turbo,
    cpu_profile_extreme,
    cpu_profile_count
};

struct cpu_policy {
    char path[CPU_PATH_MAX];
    unsigned freq[CPU_FREQ_MAX];
    int freq_count;
    int limited;
    char governors[512];
};

struct cpu_target {
    unsigned min;
    unsigned max;
    char governor[CPU_GOVERNOR_MAX];
};

const char *cpu_profile_name(int id);

int cpu_profile_from_name(const char *name);

const char *cpu_profile_assign_value(const char *name);

int cpu_profile_parse(const char *name);

void cpu_profile_windows(int count, int lo[cpu_profile_count], int hi[cpu_profile_count]);

int cpu_policies_load(struct cpu_policy *out, int max);

int cpu_profile_resolve(const struct cpu_policy *policy, int id, struct cpu_target *target);

int cpu_profile_setting(const char *key);

int cpu_profile_session(void);

int cpu_profile_apply(int id, int temporary, char *governor, size_t governor_size);

int cpu_profile_reapply(char *governor, size_t governor_size);

int cpu_profile_set_governor(const char *governor);

int cpu_profile_range(int id, unsigned *min, unsigned *max);
