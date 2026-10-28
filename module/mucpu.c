#include <stdio.h>
#include <string.h>

#include <common/platform/cpu_profile.h>

static int usage(void) {
    fprintf(
        stderr, "Usage: mucpu apply <profile|default|idle> [--temporary]\n"
                "       mucpu reapply\n"
                "       mucpu status\n"
                "       mucpu list\n"
                "Profiles:"
    );
    for (int i = 0; i < cpu_profile_count; i++)
        fprintf(stderr, " %s", cpu_profile_name(i));
    fprintf(stderr, "\n");
    return 2;
}

static unsigned policy_value(const struct cpu_policy *policy, const char *name) {
    char path[CPU_PATH_MAX * 2];
    const int written = snprintf(path, sizeof(path), "%s/%s", policy->path, name);
    if (written <= 0 || (size_t) written >= sizeof(path)) return 0;

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    unsigned value = 0;
    if (fscanf(file, "%u", &value) != 1) value = 0;
    fclose(file);
    return value;
}

static void policy_governor(const struct cpu_policy *policy, char *out, const size_t size) {
    char path[CPU_PATH_MAX * 2];
    out[0] = '\0';

    const int written = snprintf(path, sizeof(path), "%s/scaling_governor", policy->path);
    if (written <= 0 || (size_t) written >= sizeof(path)) return;

    FILE *file = fopen(path, "r");
    if (!file) return;

    if (!fgets(out, (int) size, file)) out[0] = '\0';
    fclose(file);
    out[strcspn(out, " \t\r\n")] = '\0';
}

static int show_result(const int result, const char *governor) {
    printf("%s\n", governor[0] ? governor : "unknown");
    if (result < 0) fprintf(stderr, "mucpu: profile could not be applied to every policy\n");
    if (result > 0) fprintf(stderr, "mucpu: the kernel adjusted the requested limits\n");
    return result < 0 ? 1 : 0;
}

static int command_apply(const int argc, char **argv) {
    if (argc < 3) return usage();

    int temporary = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--temporary") == 0) {
            temporary = 1;
        } else {
            return usage();
        }
    }

    int id;
    if (strcmp(argv[2], "default") == 0) {
        id = cpu_profile_setting("gov_default");
    } else if (strcmp(argv[2], "idle") == 0) {
        id = cpu_profile_setting("gov_idle");
    } else {
        id = cpu_profile_parse(argv[2]);
    }

    if (id < 0) {
        fprintf(stderr, "mucpu: unknown profile '%s'\n", argv[2]);
        return usage();
    }

    char governor[CPU_GOVERNOR_MAX];
    return show_result(cpu_profile_apply(id, temporary, governor, sizeof(governor)), governor);
}

static int command_reapply(void) {
    char governor[CPU_GOVERNOR_MAX];
    return show_result(cpu_profile_reapply(governor, sizeof(governor)), governor);
}

static int command_status(void) {
    struct cpu_policy policies[CPU_POLICY_MAX];
    const int count = cpu_policies_load(policies, CPU_POLICY_MAX);

    printf("session: %s\n", cpu_profile_name(cpu_profile_session()));
    printf("default: %s\n", cpu_profile_name(cpu_profile_setting("gov_default")));
    printf("idle: %s\n", cpu_profile_name(cpu_profile_setting("gov_idle")));

    for (int i = 0; i < count; i++) {
        char governor[CPU_GOVERNOR_MAX];
        policy_governor(&policies[i], governor, sizeof(governor));

        printf(
            "%s: governor=%s min=%u max=%u current=%u points=%d%s\n", policies[i].path, governor,
            policy_value(&policies[i], "scaling_min_freq"), policy_value(&policies[i], "scaling_max_freq"),
            policy_value(&policies[i], "scaling_cur_freq"), policies[i].freq_count,
            policies[i].limited ? " limited" : ""
        );
    }

    return count ? 0 : 1;
}

static int command_list(void) {
    struct cpu_policy policies[CPU_POLICY_MAX];
    const int count = cpu_policies_load(policies, CPU_POLICY_MAX);

    for (int id = 0; id < cpu_profile_count; id++) {
        printf("%s:", cpu_profile_name(id));

        for (int i = 0; i < count; i++) {
            struct cpu_target target;
            if (!cpu_profile_resolve(&policies[i], id, &target)) {
                printf(" [unavailable]");
                continue;
            }

            printf(
                " [%u-%u MHz %s]", target.min / 1000, target.max / 1000,
                target.governor[0] ? target.governor : "unchanged"
            );
        }

        printf("\n");
    }

    return count ? 0 : 1;
}

int main(const int argc, char **argv) {
    if (argc < 2) return usage();

    if (strcmp(argv[1], "apply") == 0) return command_apply(argc, argv);
    if (strcmp(argv[1], "reapply") == 0 && argc == 2) return command_reapply();
    if (strcmp(argv[1], "status") == 0 && argc == 2) return command_status();
    if (strcmp(argv[1], "list") == 0 && argc == 2) return command_list();

    return usage();
}
