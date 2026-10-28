#include "cpu_profile.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/base/options.h>

#define CPUFREQ_ROOT "/sys/devices/system/cpu/cpufreq"

static const char *profile_names[cpu_profile_count] = {
    "automatic", "powersave", "efficient", "balanced", "responsive", "turbo", "extreme",
};

static const struct {
    const char *name;
    int id;
} legacy_governors[] = {
    {"performance", cpu_profile_extreme},    {"low_power", cpu_profile_powersave},
    {"schedutil", cpu_profile_automatic},    {"ondemand", cpu_profile_automatic},
    {"conservative", cpu_profile_automatic}, {"interactive", cpu_profile_automatic},
    {"userspace", cpu_profile_automatic},    {"high_performance", cpu_profile_turbo},
};

static const char *adaptive_order[] = {"schedutil", "ondemand", "conservative", "interactive"};

static const char *ondemand_tunables[] = {"sampling_rate", "up_threshold", "sampling_down_factor", "io_is_busy"};

const char *cpu_profile_name(const int id) {
    if (id < 0 || id >= cpu_profile_count) return profile_names[cpu_profile_automatic];
    return profile_names[id];
}

static void trim_copy(char *out, const size_t size, const char *in) {
    if (!size) return;
    out[0] = '\0';
    if (!in) return;

    while (*in && isspace((unsigned char) *in))
        in++;

    size_t len = strlen(in);
    while (len && isspace((unsigned char) in[len - 1]))
        len--;
    if (len >= size) len = size - 1;

    for (size_t i = 0; i < len; i++)
        out[i] = (char) tolower((unsigned char) in[i]);
    out[len] = '\0';
}

int cpu_profile_parse(const char *name) {
    char value[CPU_GOVERNOR_MAX];
    trim_copy(value, sizeof(value), name);
    if (!value[0]) return -1;

    for (int i = 0; i < cpu_profile_count; i++)
        if (strcmp(value, profile_names[i]) == 0) return i;

    for (size_t i = 0; i < sizeof(legacy_governors) / sizeof(legacy_governors[0]); i++)
        if (strcmp(value, legacy_governors[i].name) == 0) return legacy_governors[i].id;

    return -1;
}

int cpu_profile_from_name(const char *name) {
    const int id = cpu_profile_parse(name);
    return id < 0 ? cpu_profile_automatic : id;
}

const char *cpu_profile_assign_value(const char *name) {
    char value[CPU_GOVERNOR_MAX];
    trim_copy(value, sizeof(value), name);
    if (!value[0] || strcmp(value, "default") == 0) return "default";
    return cpu_profile_name(cpu_profile_from_name(value));
}

void cpu_profile_windows(const int count, int lo[cpu_profile_count], int hi[cpu_profile_count]) {
    static const int small[6][4][2] = {
        {{0, 0}, {0, 0}, {0, 0}, {0, 0}}, {{0, 0}, {0, 0}, {0, 0}, {0, 0}}, {{0, 1}, {0, 1}, {0, 1}, {0, 1}},
        {{0, 1}, {0, 2}, {0, 2}, {1, 2}}, {{0, 1}, {1, 2}, {1, 3}, {2, 3}}, {{0, 1}, {1, 2}, {2, 3}, {3, 4}},
    };

    const int n = count < 1 ? 1 : count;

    lo[cpu_profile_automatic] = 0;
    hi[cpu_profile_automatic] = n - 1;
    lo[cpu_profile_powersave] = 0;
    hi[cpu_profile_powersave] = 0;
    lo[cpu_profile_extreme] = n - 1;
    hi[cpu_profile_extreme] = n - 1;

    for (int j = 0; j < 4; j++) {
        const int id = cpu_profile_efficient + j;
        if (n >= 6) {
            const int centre = 1 + (2 * j * (n - 3) + 3) / 6;
            lo[id] = centre - 1;
            hi[id] = centre + 1;
        } else {
            lo[id] = small[n][j][0];
            hi[id] = small[n][j][1];
        }
    }
}

static int read_text(const char *path, char *out, const size_t size) {
    if (!size) return 0;
    out[0] = '\0';

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    const size_t got = fread(out, 1, size - 1, file);
    out[got] = '\0';
    fclose(file);

    size_t len = strlen(out);
    while (len && isspace((unsigned char) out[len - 1]))
        out[--len] = '\0';

    return len > 0;
}

static unsigned read_unsigned(const char *path) {
    char text[64];
    if (!read_text(path, text, sizeof(text))) return 0;

    char *end = NULL;
    errno = 0;
    const unsigned long value = strtoul(text, &end, 10);
    if (errno || end == text || value > 0xFFFFFFFFUL) return 0;
    return (unsigned) value;
}

static int write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    if (!file) return 0;

    const int ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok;
}

static int write_unsigned(const char *path, const unsigned value) {
    char text[32];
    snprintf(text, sizeof(text), "%u", value);
    return write_text(path, text);
}

static int join_path(char *out, const size_t size, const char *dir, const char *name) {
    const int written = snprintf(out, size, "%s/%s", dir, name);
    return written > 0 && (size_t) written < size;
}

static int policy_file(char *out, const size_t size, const struct cpu_policy *policy, const char *name) {
    return join_path(out, size, policy->path, name);
}

static int compare_unsigned(const void *a, const void *b) {
    const unsigned x = *(const unsigned *) a;
    const unsigned y = *(const unsigned *) b;
    return (x > y) - (x < y);
}

static int compare_int(const void *a, const void *b) {
    const int x = *(const int *) a;
    const int y = *(const int *) b;
    return (x > y) - (x < y);
}

static int sorted_unique(unsigned *values, int count) {
    qsort(values, (size_t) count, sizeof(values[0]), compare_unsigned);

    int unique = 0;
    for (int i = 0; i < count; i++) {
        if (!values[i]) continue;
        if (unique && values[unique - 1] == values[i]) continue;
        values[unique++] = values[i];
    }

    return unique;
}

static unsigned device_number(const char *name) {
    char path[CPU_PATH_MAX];
    if (!join_path(path, sizeof(path), CONF_DEVICE_PATH "cpu", name)) return 0;
    return read_unsigned(path);
}

static unsigned frequency_cap(void) {
    const unsigned chosen = read_unsigned(CONF_CONFIG_PATH "danger/cpu_max");
    return chosen ? chosen : device_number("max_freq_default");
}

static void clamp_table(struct cpu_policy *policy) {
    const unsigned floor = device_number("min_freq_default");
    const unsigned cap = frequency_cap();

    unsigned kept[CPU_FREQ_MAX];
    int count = 0;

    for (int i = 0; i < policy->freq_count; i++) {
        if (floor && policy->freq[i] < floor) continue;
        if (cap && policy->freq[i] > cap) continue;
        kept[count++] = policy->freq[i];
    }

    if (!count) return;

    memcpy(policy->freq, kept, sizeof(kept[0]) * (size_t) count);
    policy->freq_count = count;
}

static void load_table(struct cpu_policy *policy) {
    char path[CPU_PATH_MAX];
    char text[1024];

    policy->freq_count = 0;
    policy->limited = 0;

    if (policy_file(path, sizeof(path), policy, "scaling_available_frequencies")
        && read_text(path, text, sizeof(text))) {
        char *save = NULL;
        for (char *token = strtok_r(text, " \t\n", &save); token && policy->freq_count < CPU_FREQ_MAX;
             token = strtok_r(NULL, " \t\n", &save)) {
            char *end = NULL;
            const unsigned long value = strtoul(token, &end, 10);
            if (end != token && *end == '\0' && value > 0 && value <= 0xFFFFFFFFUL)
                policy->freq[policy->freq_count++] = (unsigned) value;
        }
    }

    if (!policy->freq_count) {
        policy->limited = 1;

        unsigned low = 0;
        unsigned high = 0;
        if (policy_file(path, sizeof(path), policy, "cpuinfo_min_freq")) low = read_unsigned(path);
        if (policy_file(path, sizeof(path), policy, "cpuinfo_max_freq")) high = read_unsigned(path);

        if (low) policy->freq[policy->freq_count++] = low;
        if (high) policy->freq[policy->freq_count++] = high;
    }

    policy->freq_count = sorted_unique(policy->freq, policy->freq_count);
    clamp_table(policy);

    if (!policy_file(path, sizeof(path), policy, "scaling_available_governors")
        || !read_text(path, policy->governors, sizeof(policy->governors)))
        policy->governors[0] = '\0';
}

static int policy_usable(const char *dir) {
    char path[CPU_PATH_MAX];
    return join_path(path, sizeof(path), dir, "scaling_governor") && access(path, F_OK) == 0;
}

int cpu_policies_load(struct cpu_policy *out, const int max) {
    if (!out || max <= 0) return 0;

    int numbers[CPU_POLICY_MAX];
    int found = 0;

    DIR *dir = opendir(CPUFREQ_ROOT);
    if (dir) {
        const struct dirent *entry;
        while ((entry = readdir(dir)) && found < CPU_POLICY_MAX) {
            if (strncmp(entry->d_name, "policy", 6) != 0) continue;

            const char *digits = entry->d_name + 6;
            if (!*digits) continue;

            int valid = 1;
            for (const char *c = digits; *c; c++)
                if (!isdigit((unsigned char) *c)) valid = 0;
            if (valid) numbers[found++] = atoi(digits);
        }
        closedir(dir);
    }

    qsort(numbers, (size_t) found, sizeof(numbers[0]), compare_int);

    int count = 0;
    for (int i = 0; i < found && count < max; i++) {
        snprintf(out[count].path, sizeof(out[count].path), CPUFREQ_ROOT "/policy%d", numbers[i]);
        if (policy_usable(out[count].path)) count++;
    }

    if (!count) {
        char governor_path[CPU_PATH_MAX];
        if (!read_text(CONF_DEVICE_PATH "cpu/governor", governor_path, sizeof(governor_path)))
            snprintf(governor_path, sizeof(governor_path), "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");

        char *slash = strrchr(governor_path, '/');
        if (slash) *slash = '\0';

        snprintf(out[0].path, sizeof(out[0].path), "%s", governor_path);
        if (policy_usable(out[0].path)) count = 1;
    }

    for (int i = 0; i < count; i++)
        load_table(&out[i]);

    return count;
}

static int governor_available(const struct cpu_policy *policy, const char *governor) {
    if (!governor || !*governor || !policy->governors[0]) return 0;

    const size_t len = strlen(governor);
    for (const char *p = policy->governors; (p = strstr(p, governor)); p += len) {
        const int start = p == policy->governors || isspace((unsigned char) p[-1]);
        const int end = p[len] == '\0' || isspace((unsigned char) p[len]);
        if (start && end) return 1;
    }

    return 0;
}

static void adaptive_governor(const struct cpu_policy *policy, char *out, const size_t size) {
    char preferred[CPU_GOVERNOR_MAX];
    read_text(CONF_DEVICE_PATH "cpu/default", preferred, sizeof(preferred));

    if (governor_available(policy, preferred) && strcmp(preferred, "performance") != 0
        && strcmp(preferred, "powersave") != 0 && strcmp(preferred, "userspace") != 0) {
        snprintf(out, size, "%s", preferred);
        return;
    }

    for (size_t i = 0; i < sizeof(adaptive_order) / sizeof(adaptive_order[0]); i++) {
        if (governor_available(policy, adaptive_order[i])) {
            snprintf(out, size, "%s", adaptive_order[i]);
            return;
        }
    }

    if (governor_available(policy, preferred)) {
        snprintf(out, size, "%s", preferred);
        return;
    }

    out[0] = '\0';
}

int cpu_profile_resolve(const struct cpu_policy *policy, const int id, struct cpu_target *target) {
    if (!policy || !target || policy->freq_count < 1) return 0;

    int lo[cpu_profile_count];
    int hi[cpu_profile_count];
    cpu_profile_windows(policy->freq_count, lo, hi);

    const int profile = id < 0 || id >= cpu_profile_count ? cpu_profile_automatic : id;
    target->min = policy->freq[lo[profile]];
    target->max = policy->freq[hi[profile]];

    if (profile == cpu_profile_powersave && governor_available(policy, "powersave")) {
        snprintf(target->governor, sizeof(target->governor), "powersave");
    } else if (profile == cpu_profile_extreme && governor_available(policy, "performance")) {
        snprintf(target->governor, sizeof(target->governor), "performance");
    } else {
        adaptive_governor(policy, target->governor, sizeof(target->governor));
    }

    return 1;
}

int cpu_profile_setting(const char *key) {
    const int fallback = key && strcmp(key, "gov_idle") == 0 ? cpu_profile_powersave : cpu_profile_automatic;
    if (!key) return fallback;

    char path[CPU_PATH_MAX];
    char value[CPU_GOVERNOR_MAX];
    if (!join_path(path, sizeof(path), CONF_CONFIG_PATH "settings/power", key)
        || !read_text(path, value, sizeof(value)))
        return fallback;

    const int id = cpu_profile_parse(value);
    return id < 0 ? fallback : id;
}

int cpu_profile_session(void) {
    char value[CPU_GOVERNOR_MAX];
    if (read_text(CPU_PROFILE_SESSION, value, sizeof(value))) {
        const int id = cpu_profile_parse(value);
        if (id >= 0) return id;
    }

    return cpu_profile_setting("gov_default");
}

static void apply_ondemand_tunables(const struct cpu_policy *policy) {
    for (size_t i = 0; i < sizeof(ondemand_tunables) / sizeof(ondemand_tunables[0]); i++) {
        char name[CPU_PATH_MAX];
        char value[64];
        char target[CPU_PATH_MAX];
        char ondemand[CPU_PATH_MAX];

        const int named = snprintf(name, sizeof(name), CONF_DEVICE_PATH "cpu/%s_default", ondemand_tunables[i]);
        if (named <= 0 || (size_t) named >= sizeof(name) || !read_text(name, value, sizeof(value))) continue;

        if (!join_path(name, sizeof(name), CONF_DEVICE_PATH "cpu", ondemand_tunables[i])
            || !read_text(name, target, sizeof(target)) || access(target, W_OK) != 0) {
            if (!join_path(ondemand, sizeof(ondemand), policy->path, "ondemand")
                || !join_path(target, sizeof(target), ondemand, ondemand_tunables[i]) || access(target, W_OK) != 0) {
                if (!join_path(target, sizeof(target), CPUFREQ_ROOT "/ondemand", ondemand_tunables[i])) continue;
            }
        }

        if (access(target, W_OK) == 0) write_text(target, value);
    }
}

static int write_bounds(const struct cpu_policy *policy, const unsigned min, const unsigned max) {
    char min_path[CPU_PATH_MAX];
    char max_path[CPU_PATH_MAX];
    if (!policy_file(min_path, sizeof(min_path), policy, "scaling_min_freq")
        || !policy_file(max_path, sizeof(max_path), policy, "scaling_max_freq"))
        return 0;

    const unsigned old_max = read_unsigned(max_path);

    int ok;
    if (old_max && min > old_max) {
        ok = write_unsigned(max_path, max);
        ok = write_unsigned(min_path, min) && ok;
    } else {
        ok = write_unsigned(min_path, min);
        ok = write_unsigned(max_path, max) && ok;
    }

    return ok;
}

static int apply_policy(const struct cpu_policy *policy, const struct cpu_target *target) {
    char governor_path[CPU_PATH_MAX];
    char min_path[CPU_PATH_MAX];
    char max_path[CPU_PATH_MAX];
    if (!policy_file(governor_path, sizeof(governor_path), policy, "scaling_governor")
        || !policy_file(min_path, sizeof(min_path), policy, "scaling_min_freq")
        || !policy_file(max_path, sizeof(max_path), policy, "scaling_max_freq"))
        return -1;

    char old_governor[CPU_GOVERNOR_MAX];
    read_text(governor_path, old_governor, sizeof(old_governor));
    const unsigned old_min = read_unsigned(min_path);
    const unsigned old_max = read_unsigned(max_path);

    const int bounds_written = write_bounds(policy, target->min, target->max);
    const int governor_written = !target->governor[0] || write_text(governor_path, target->governor);

    if (!bounds_written || !governor_written) {
        if (old_min && old_max) write_bounds(policy, old_min, old_max);
        if (old_governor[0]) write_text(governor_path, old_governor);
        return -1;
    }

    if (strcmp(target->governor, "ondemand") == 0) apply_ondemand_tunables(policy);

    char now_governor[CPU_GOVERNOR_MAX];
    read_text(governor_path, now_governor, sizeof(now_governor));

    const int exact = read_unsigned(min_path) == target->min && read_unsigned(max_path) == target->max
                      && (!target->governor[0] || strcmp(now_governor, target->governor) == 0);

    return exact ? 0 : 1;
}

int cpu_profile_apply(const int id, const int temporary, char *governor, const size_t governor_size) {
    if (governor && governor_size) governor[0] = '\0';

    struct cpu_policy policies[CPU_POLICY_MAX];
    const int count = cpu_policies_load(policies, CPU_POLICY_MAX);
    if (!count) return -1;

    int result = 0;
    for (int i = 0; i < count; i++) {
        struct cpu_target target;
        if (!cpu_profile_resolve(&policies[i], id, &target)) {
            result = -1;
            continue;
        }

        const int applied = apply_policy(&policies[i], &target);
        if (applied < 0) {
            result = -1;
        } else if (applied > 0 && result == 0) {
            result = 1;
        }

        if (i == 0 && governor && governor_size) {
            char path[CPU_PATH_MAX];
            if (policy_file(path, sizeof(path), &policies[i], "scaling_governor"))
                read_text(path, governor, governor_size);
        }
    }

    if (!temporary && result >= 0) {
        if (access(RUN_PATH, W_OK) == 0) write_text(CPU_PROFILE_SESSION, cpu_profile_name(id));
    }

    return result;
}

int cpu_profile_reapply(char *governor, const size_t governor_size) {
    return cpu_profile_apply(cpu_profile_session(), 1, governor, governor_size);
}

int cpu_profile_set_governor(const char *governor) {
    if (!governor || !*governor) return -1;

    struct cpu_policy policies[CPU_POLICY_MAX];
    const int count = cpu_policies_load(policies, CPU_POLICY_MAX);

    int written = 0;
    for (int i = 0; i < count; i++) {
        if (policies[i].governors[0] && !governor_available(&policies[i], governor)) continue;

        char path[CPU_PATH_MAX];
        if (policy_file(path, sizeof(path), &policies[i], "scaling_governor") && write_text(path, governor)) written++;
    }

    return written ? 0 : -1;
}

int cpu_profile_range(const int id, unsigned *min, unsigned *max) {
    struct cpu_policy policies[CPU_POLICY_MAX];
    if (!cpu_policies_load(policies, CPU_POLICY_MAX)) return 0;

    struct cpu_target target;
    if (!cpu_profile_resolve(&policies[0], id, &target)) return 0;

    if (min) *min = target.min;
    if (max) *max = target.max;
    return 1;
}
