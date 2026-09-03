#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENT_DEVICES       64
#define MAX_CPU_POLICIES        16
#define MAX_CPUS                32
#define MAX_QUIESCE_NAMES       16
#define MAX_QUIESCED_PROCESSES  64
#define ARM_DELAY_MILLISECONDS  350
#define TIMER_SLACK_NANOSECONDS 50000000UL

#define BITS_PER_LONG (sizeof(unsigned long) * 8U)
#define BIT_WORD(bit) ((bit) / BITS_PER_LONG)
#define BIT_MASK(bit) (1UL << ((bit) % BITS_PER_LONG))

enum suspend_result {
    suspend_wake = 0,
    suspend_error = 1,
    suspend_timeout = 2,
};

struct suspend_options {
    const char *state;
    const char *power_device;
    const char *quiesce_names[MAX_QUIESCE_NAMES];
    size_t quiesce_count;
    int timeout_milliseconds;
    int optimise;
};

struct cpu_policy_snapshot {
    char governor_path[PATH_MAX];
    char governor[64];
    char maximum_path[PATH_MAX];
    char maximum[64];
    int governor_changed;
    int maximum_changed;
};

struct optimisation_snapshot {
    struct cpu_policy_snapshot policies[MAX_CPU_POLICIES];
    size_t policy_count;
    int offlined_cpus[MAX_CPUS];
    size_t offlined_count;
    pid_t quiesced_processes[MAX_QUIESCED_PROCESSES];
    size_t quiesced_count;
};

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(const int signal_number) {
    (void) signal_number;
    stop_requested = 1;
}

static int read_text(const char *path, char *value, const size_t value_size) {
    if (!path || !value) {
        errno = EINVAL;
        return -1;
    }

    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return -1;

    ssize_t length;
    do {
        length = read(descriptor, value, value_size - 1);
    } while (length < 0 && errno == EINTR);

    const int saved_errno = errno;
    close(descriptor);
    errno = saved_errno;
    if (length < 0) return -1;

    value[length] = '\0';
    while (length > 0 && isspace((unsigned char) value[length - 1]))
        value[--length] = '\0';
    return 0;
}

static int write_text(const char *path, const char *value) {
    const int descriptor = open(path, O_WRONLY | O_CLOEXEC);
    if (descriptor < 0) return -1;

    const size_t expected = strlen(value);
    size_t written = 0;
    while (written < expected) {
        const ssize_t result = write(descriptor, value + written, expected - written);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            const int saved_errno = result == 0 ? EIO : errno;
            close(descriptor);
            errno = saved_errno;
            return -1;
        }
        written += (size_t) result;
    }

    return close(descriptor);
}

static int join_path(char *destination, const size_t destination_size, const char *directory, const char *name) {
    const size_t directory_length = strlen(directory);
    const size_t name_length = strlen(name);
    if (directory_length + 1 + name_length + 1 > destination_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(destination, directory, directory_length);
    destination[directory_length] = '/';
    memcpy(destination + directory_length + 1, name, name_length + 1);
    return 0;
}

static int contains_word(const char *words, const char *wanted) {
    if (!wanted || !*wanted) return 0;

    const size_t wanted_length = strlen(wanted);
    for (const char *word = words; *word;) {
        while (*word && isspace((unsigned char) *word))
            word++;
        const char *end = word;
        while (*end && !isspace((unsigned char) *end))
            end++;

        if ((size_t) (end - word) == wanted_length && strncmp(word, wanted, wanted_length) == 0) return 1;
        word = end;
    }

    return 0;
}

static int parse_timeout(const char *value, int *timeout_milliseconds) {
    errno = 0;
    char *end = NULL;
    const long seconds = strtol(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0' || seconds < 0) return 0;
    if (seconds > (long) (INT_MAX / 1000)) {
        *timeout_milliseconds = INT_MAX;
    } else {
        *timeout_milliseconds = (int) seconds * 1000;
    }

    return 1;
}

static int event_has_power_key(const int descriptor) {
    unsigned long event_bits[BIT_WORD(EV_MAX) + 1] = {0};
    if (ioctl(descriptor, EVIOCGBIT(0, sizeof(event_bits)), event_bits) < 0) return 0;
    if ((event_bits[BIT_WORD(EV_KEY)] & BIT_MASK(EV_KEY)) == 0) return 0;

    unsigned long key_bits[BIT_WORD(KEY_MAX) + 1] = {0};
    if (ioctl(descriptor, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) return 0;
    return (key_bits[BIT_WORD(KEY_POWER)] & BIT_MASK(KEY_POWER)) != 0;
}

static int find_power_device(
    const char *wanted_name, char *path, const size_t path_size, char *found_name, const size_t found_name_size
) {
    for (int index = 0; index < MAX_EVENT_DEVICES; ++index) {
        const int length = snprintf(path, path_size, "/dev/input/event%d", index);
        if (length < 0 || (size_t) length >= path_size) return -1;

        const int descriptor = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (descriptor < 0) continue;

        char name[128] = {0};
        const int named = ioctl(descriptor, EVIOCGNAME(sizeof(name)), name);
        const int name_matches = !wanted_name || (named >= 0 && strcmp(name, wanted_name) == 0);
        if (name_matches && event_has_power_key(descriptor)) {
            snprintf(found_name, found_name_size, "%s", name);
            return descriptor;
        }

        close(descriptor);
    }

    errno = ENODEV;
    return -1;
}

static void drain_events(const int descriptor) {
    struct input_event event;
    while (read(descriptor, &event, sizeof(event)) == (ssize_t) sizeof(event)) {
    }
}

static long long milliseconds_since(const struct timespec *started) {
    struct timespec current = {0};
    clock_gettime(CLOCK_BOOTTIME, &current);
    return (current.tv_sec - started->tv_sec) * 1000LL + (current.tv_nsec - started->tv_nsec) / 1000000LL;
}

static int wait_for_power_key(const int descriptor, const int timeout_milliseconds) {
    struct pollfd input = {
        .fd = descriptor,
        .events = POLLIN,
    };

    int remaining = timeout_milliseconds;
    struct timespec started = {0};
    if (remaining >= 0) clock_gettime(CLOCK_BOOTTIME, &started);

    for (;;) {
        if (stop_requested) {
            errno = EINTR;
            return suspend_error;
        }

        const int result = poll(&input, 1, remaining);
        if (result == 0) return suspend_timeout;
        if (result < 0) {
            if (errno == EINTR && !stop_requested) continue;
            return suspend_error;
        }
        if ((input.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return suspend_error;

        struct input_event event;
        while (read(descriptor, &event, sizeof(event)) == (ssize_t) sizeof(event)) {
            if (event.type == EV_KEY && event.code == KEY_POWER && event.value == 1) return suspend_wake;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) return suspend_error;

        if (timeout_milliseconds >= 0) {
            const long long elapsed = milliseconds_since(&started);
            if (elapsed >= timeout_milliseconds) return suspend_timeout;
            remaining = timeout_milliseconds - (int) elapsed;
        }
    }
}

static int process_matches(const pid_t process_id, const char *const *names, const size_t name_count) {
    char executable_path[64];
    snprintf(executable_path, sizeof(executable_path), "/proc/%ld/exe", (long) process_id);

    char executable[PATH_MAX];
    const ssize_t length = readlink(executable_path, executable, sizeof(executable) - 1);
    if (length < 0) return 0;
    executable[length] = '\0';

    const char *base = strrchr(executable, '/');
    base = base ? base + 1 : executable;
    for (size_t index = 0; index < name_count; ++index) {
        if (strcmp(base, names[index]) == 0) return 1;
    }
    return 0;
}

static int process_is_stopped(const pid_t process_id) {
    char status_path[64];
    snprintf(status_path, sizeof(status_path), "/proc/%ld/status", (long) process_id);

    FILE *status = fopen(status_path, "r");
    if (!status) return 0;

    char line[256];
    int stopped = 0;
    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "State:", 6) == 0) {
            const char *state = line + 6;
            while (*state && isspace((unsigned char) *state))
                state++;
            stopped = *state == 'T' || *state == 't';
            break;
        }
    }

    fclose(status);
    return stopped;
}

static void
quiesce_processes(struct optimisation_snapshot *snapshot, const char *const *names, const size_t name_count) {
    if (name_count == 0) return;

    DIR *processes = opendir("/proc");
    if (!processes) return;

    const pid_t own_process = getpid();
    struct dirent *entry;
    while ((entry = readdir(processes)) != NULL && snapshot->quiesced_count < MAX_QUIESCED_PROCESSES) {
        if (!isdigit((unsigned char) entry->d_name[0])) continue;

        char *end = NULL;
        const long parsed = strtol(entry->d_name, &end, 10);
        if (!end || *end != '\0' || parsed <= 1 || parsed > INT_MAX) continue;

        const pid_t process_id = (pid_t) parsed;
        if (process_id == own_process || process_is_stopped(process_id)) continue;
        if (!process_matches(process_id, names, name_count)) continue;

        if (kill(process_id, SIGSTOP) == 0) {
            snapshot->quiesced_processes[snapshot->quiesced_count++] = process_id;
        }
    }

    closedir(processes);
}

static void optimise_cpu_policies(struct optimisation_snapshot *snapshot) {
    glob_t paths = {0};
    if (glob("/sys/devices/system/cpu/cpufreq/policy*/scaling_governor", 0, NULL, &paths) != 0) return;

    for (size_t index = 0; index < paths.gl_pathc && snapshot->policy_count < MAX_CPU_POLICIES; ++index) {
        struct cpu_policy_snapshot *policy = &snapshot->policies[snapshot->policy_count];
        const size_t governor_path_length = strlen(paths.gl_pathv[index]);
        if (governor_path_length >= sizeof(policy->governor_path)) continue;
        memcpy(policy->governor_path, paths.gl_pathv[index], governor_path_length + 1);
        if (read_text(policy->governor_path, policy->governor, sizeof(policy->governor)) < 0) continue;

        char policy_directory[PATH_MAX];
        snprintf(policy_directory, sizeof(policy_directory), "%s", policy->governor_path);
        char *separator = strrchr(policy_directory, '/');
        if (!separator) continue;
        *separator = '\0';

        char available_path[PATH_MAX];
        char available[256];
        if (join_path(available_path, sizeof(available_path), policy_directory, "scaling_available_governors") < 0) {
            continue;
        }
        if (read_text(available_path, available, sizeof(available)) == 0 && contains_word(available, "powersave")
            && strcmp(policy->governor, "powersave") != 0 && write_text(policy->governor_path, "powersave") == 0) {
            policy->governor_changed = 1;
        }

        char minimum_path[PATH_MAX];
        char minimum[64];
        if (join_path(policy->maximum_path, sizeof(policy->maximum_path), policy_directory, "scaling_max_freq") < 0
            || join_path(minimum_path, sizeof(minimum_path), policy_directory, "scaling_min_freq") < 0) {
            continue;
        }
        if (read_text(policy->maximum_path, policy->maximum, sizeof(policy->maximum)) == 0
            && read_text(minimum_path, minimum, sizeof(minimum)) == 0 && strcmp(policy->maximum, minimum) != 0
            && write_text(policy->maximum_path, minimum) == 0) {
            policy->maximum_changed = 1;
        }

        snapshot->policy_count++;
    }

    globfree(&paths);
}

static void offline_secondary_cpus(struct optimisation_snapshot *snapshot) {
    long cpu_count = sysconf(_SC_NPROCESSORS_CONF);
    if (cpu_count < 1) return;
    if (cpu_count > MAX_CPUS) cpu_count = MAX_CPUS;

    for (int cpu = 1; cpu < cpu_count; ++cpu) {
        char online_path[PATH_MAX];
        char online[16];
        snprintf(online_path, sizeof(online_path), "/sys/devices/system/cpu/cpu%d/online", cpu);
        if (read_text(online_path, online, sizeof(online)) < 0 || strcmp(online, "1") != 0) continue;
        if (write_text(online_path, "0") == 0) snapshot->offlined_cpus[snapshot->offlined_count++] = cpu;
    }
}

static void begin_optimisation(
    struct optimisation_snapshot *snapshot, const char *const *quiesce_names, const size_t quiesce_count
) {
    memset(snapshot, 0, sizeof(*snapshot));
    prctl(PR_SET_TIMERSLACK, TIMER_SLACK_NANOSECONDS, 0, 0, 0);
    quiesce_processes(snapshot, quiesce_names, quiesce_count);
    optimise_cpu_policies(snapshot);
    offline_secondary_cpus(snapshot);
}

static void restore_optimisation(const struct optimisation_snapshot *snapshot) {
    for (size_t index = snapshot->offlined_count; index > 0; --index) {
        char online_path[PATH_MAX];
        snprintf(
            online_path, sizeof(online_path), "/sys/devices/system/cpu/cpu%d/online", snapshot->offlined_cpus[index - 1]
        );
        write_text(online_path, "1");
    }

    for (size_t index = snapshot->policy_count; index > 0; --index) {
        const struct cpu_policy_snapshot *policy = &snapshot->policies[index - 1];
        if (policy->maximum_changed) write_text(policy->maximum_path, policy->maximum);
        if (policy->governor_changed) write_text(policy->governor_path, policy->governor);
    }

    for (size_t index = 0; index < snapshot->quiesced_count; ++index) {
        kill(snapshot->quiesced_processes[index], SIGCONT);
    }
}

static int wait_in_userspace(const struct suspend_options *options) {
    char path[64] = {0};
    char name[128] = {0};
    const int descriptor = find_power_device(options->power_device, path, sizeof(path), name, sizeof(name));
    if (descriptor < 0) {
        fprintf(
            stderr, "mususpend: cannot find %s power-key device: %s\n",
            options->power_device ? options->power_device : "a", strerror(errno)
        );
        return suspend_error;
    }

    const struct timespec arm_delay = {
        .tv_sec = 0,
        .tv_nsec = ARM_DELAY_MILLISECONDS * 1000000L,
    };
    nanosleep(&arm_delay, NULL);
    drain_events(descriptor);

    struct optimisation_snapshot optimisation = {0};
    if (options->optimise) {
        begin_optimisation(&optimisation, options->quiesce_names, options->quiesce_count);
    }

    const int result = wait_for_power_key(descriptor, options->timeout_milliseconds);
    const int saved_errno = errno;

    if (options->optimise) restore_optimisation(&optimisation);
    close(descriptor);
    errno = saved_errno;

    if (result == suspend_error) {
        fprintf(stderr, "mususpend: power-key wait failed on %s (%s): %s\n", path, name, strerror(errno));
    }
    return result;
}

static int resolve_kernel_state(const char *requested, char *resolved, const size_t resolved_size) {
    char supported[256];
    if (read_text("/sys/power/state", supported, sizeof(supported)) < 0) return -1;

    const char *state = requested;
    if (strcmp(requested, "auto") == 0) {
        if (contains_word(supported, "mem")) {
            state = "mem";
        } else if (contains_word(supported, "freeze")) {
            state = "freeze";
        } else if (contains_word(supported, "standby")) {
            state = "standby";
        } else {
            errno = ENOTSUP;
            return -1;
        }
    } else if (!contains_word(supported, requested)) {
        errno = ENOTSUP;
        return -1;
    }

    if (snprintf(resolved, resolved_size, "%s", state) < 0) return -1;
    return 0;
}

static int arm_wakeup_count(void) {
    if (access("/sys/power/wakeup_count", R_OK | W_OK) != 0) return 0;

    for (int attempt = 0; attempt < 3; ++attempt) {
        char wakeup_count[64];
        if (read_text("/sys/power/wakeup_count", wakeup_count, sizeof(wakeup_count)) < 0) return -1;
        if (write_text("/sys/power/wakeup_count", wakeup_count) == 0) return 0;
        if (errno != EAGAIN && errno != EBUSY) return -1;
    }

    return -1;
}

static int enter_kernel_suspend(const struct suspend_options *options) {
    char state[32];
    if (resolve_kernel_state(options->state, state, sizeof(state)) < 0) {
        fprintf(stderr, "mususpend: unsupported kernel sleep state '%s': %s\n", options->state, strerror(errno));
        return suspend_error;
    }

    if (arm_wakeup_count() < 0) {
        fprintf(stderr, "mususpend: wakeup-count handshake failed: %s\n", strerror(errno));
        return suspend_error;
    }

    struct timespec started = {0};
    if (options->timeout_milliseconds >= 0) clock_gettime(CLOCK_BOOTTIME, &started);

    if (write_text("/sys/power/state", state) < 0) {
        fprintf(stderr, "mususpend: could not enter '%s': %s\n", state, strerror(errno));
        return suspend_error;
    }

    if (options->timeout_milliseconds >= 0 && milliseconds_since(&started) >= options->timeout_milliseconds) {
        return suspend_timeout;
    }
    return suspend_wake;
}

static void usage(FILE *stream, const char *program) {
    fprintf(
        stream,
        "Usage: %s [--state userspace|auto|mem|freeze|standby] [--timeout seconds]\n"
        "          [--power-device name] [--optimise] [--quiesce executable]\n",
        program
    );
}

static int parse_options(const int argc, char **argv, struct suspend_options *options) {
    *options = (struct suspend_options) {
        .timeout_milliseconds = -1,
    };

    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--state") == 0 && index + 1 < argc) {
            options->state = argv[++index];
        } else if (strcmp(argv[index], "--timeout") == 0 && index + 1 < argc) {
            if (!parse_timeout(argv[++index], &options->timeout_milliseconds)) {
                fprintf(stderr, "mususpend: invalid timeout: %s\n", argv[index]);
                return 0;
            }
        } else if (strcmp(argv[index], "--power-device") == 0 && index + 1 < argc) {
            options->power_device = argv[++index];
        } else if (strcmp(argv[index], "--optimise") == 0) {
            options->optimise = 1;
        } else if (strcmp(argv[index], "--quiesce") == 0 && index + 1 < argc) {
            if (options->quiesce_count >= MAX_QUIESCE_NAMES) {
                fprintf(stderr, "mususpend: too many --quiesce arguments\n");
                return 0;
            }
            options->quiesce_names[options->quiesce_count++] = argv[++index];
        } else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
            usage(stdout, argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "mususpend: unknown or incomplete option: %s\n", argv[index]);
            return 0;
        }
    }

    if (!options->state) {
        fprintf(stderr, "mususpend: --state is required\n");
        return 0;
    }
    if (strcmp(options->state, "userspace") != 0 && strcmp(options->state, "auto") != 0
        && strcmp(options->state, "mem") != 0 && strcmp(options->state, "freeze") != 0
        && strcmp(options->state, "standby") != 0) {
        fprintf(stderr, "mususpend: invalid sleep state: %s\n", options->state);
        return 0;
    }
    if (strcmp(options->state, "userspace") != 0 && (options->optimise || options->quiesce_count > 0)) {
        fprintf(stderr, "mususpend: --optimise and --quiesce require --state userspace\n");
        return 0;
    }

    return 1;
}

int main(const int argc, char **argv) {
    struct suspend_options options;
    if (!parse_options(argc, argv, &options)) {
        usage(stderr, argv[0]);
        return suspend_error;
    }

    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    if (strcmp(options.state, "userspace") == 0) return wait_in_userspace(&options);
    return enter_kernel_suspend(&options);
}
