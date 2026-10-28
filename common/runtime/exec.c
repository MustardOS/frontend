#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <sys/prctl.h>
#include <errno.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <common/runtime/exec.h>
#include <common/runtime/log.h>
#include <common/display/language.h>
#include <common/runtime/task_exec.h>
#include <common/ui/common.h>
#include <common/ui/image.h>
#include <common/display/theme.h>
#include <common/platform/device.h>
#include <common/platform/cpu_profile.h>

static pid_t pending_exec_pid = -1;
static exec_callback pending_exec_cb = NULL;

static int write_frequency(const char *path, const long long frequency) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    const int result = fprintf(fp, "%lld", frequency) < 0 ? -1 : 0;
    fclose(fp);
    return result;
}

static void sync_gpu_frequency(const char *governor) {
    glob_t found = {0};
    if (glob("/sys/class/devfreq/*gpu*/available_frequencies", 0, NULL, &found) != 0) {
        globfree(&found);
        return;
    }

    long long cap = strtoll(config.danger.gpu_max, NULL, 10);
    if (cap <= 0) cap = strtoll(device.gpu.max_freq_default, NULL, 10);

    for (size_t i = 0; i < found.gl_pathc; i++) {
        FILE *fp = fopen(found.gl_pathv[i], "r");
        if (!fp) continue;

        long long floor = LLONG_MAX;
        long long ceiling = 0;
        long long frequency = 0;
        while (fscanf(fp, "%lld", &frequency) == 1) {
            if (frequency <= 0 || (cap > 0 && frequency > cap)) continue;
            if (frequency < floor) floor = frequency;
            if (frequency > ceiling) ceiling = frequency;
        }
        fclose(fp);

        if (floor == LLONG_MAX || ceiling <= 0) continue;

        char directory[PATH_MAX];
        snprintf(directory, sizeof(directory), "%s", found.gl_pathv[i]);
        char *leaf = strrchr(directory, '/');
        if (!leaf) continue;
        *leaf = '\0';

        char minimum[PATH_MAX];
        char maximum[PATH_MAX];
        snprintf(minimum, sizeof(minimum), "%s/min_freq", directory);
        snprintf(maximum, sizeof(maximum), "%s/max_freq", directory);

        const long long target = strcmp(governor, "performance") == 0 ? ceiling : floor;
        const long long maximum_target = strcmp(governor, "powersave") == 0 ? floor : ceiling;

        write_frequency(minimum, floor);
        write_frequency(maximum, maximum_target);
        write_frequency(minimum, target);
    }

    globfree(&found);
}

const char **build_term_exec(const char **term_cmd, size_t *term_cnt) {
    size_t arg_count = 0;
    for (const char **p = term_cmd; p && *p; p++)
        arg_count++;

    const size_t total_args = 16 + arg_count + 2;
    const char **exec = malloc(sizeof(char *) * total_args);
    if (!exec) return NULL;

    size_t i = 0;
    exec[i++] = OPT_PATH "frontend/muxterm";
    exec[i++] = "-ro";
    exec[i++] = "-s";
    exec[i++] = (char *) theme.terminal.font_size;

    static char font_path[MAX_BUFFER_SIZE];
    if (load_terminal_resource("font", "ttf", font_path, sizeof(font_path))) {
        exec[i++] = "-f";
        exec[i++] = font_path;
    }

    exec[i++] = "--font-hinting";
    exec[i++] = (char *) theme.terminal.font_hint;

    static char image_path[MAX_BUFFER_SIZE];
    if (load_terminal_resource("image", "png", image_path, sizeof(image_path))) {
        exec[i++] = "-i";
        exec[i++] = image_path;
    }

    exec[i++] = "-bg";
    exec[i++] = (char *) theme.terminal.background;
    exec[i++] = "-fg";
    exec[i++] = (char *) theme.terminal.foreground;
    exec[i++] = "--";

    for (const char **p = term_cmd; p && *p; p++)
        exec[i++] = *p;

    exec[i] = NULL;
    if (term_cnt) *term_cnt = i;
    return exec;
}

int extract_archive(const char *filename, const char *screen, const char *title) {
    const char *argv[] = {OPT_PATH "script/mux/extract.sh", filename, screen, NULL};

    // We don't want half extracted archives leaving the system in a state we cannot recover from!
    const task_exec_spec spec = {
        .argv = argv,
        .argc = 3,
        .mode = task_mode_progress,
        .can_cancel = 0,
        .turbo = 1,
        .title = title,
    };

    return task_exec_start(&spec);
}

void load_assign(
    const char *loader, const char *rom, const char *dir, const char *sys, const int forced, const int app
) {
    FILE *file = fopen(loader, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_open, loader);
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d\n%d", rom, dir, sys, forced, app);
    fclose(file);
}

void load_mux(const char *value) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_open, MUOS_ACT_LOAD);
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void run_exec(
    const char **args, const size_t size, const int background, const int turbo, const char *log_file,
    const exec_callback cb
) {
    if (!args || size == 0) return;

    const char *san[size + 1];

    if (turbo) turbo_time(1, 0);

    size_t j = 0;
    for (size_t i = 0; i < size; ++i) {
        if (args[i]) san[j++] = args[i];
    }

    if (j == 0) return;
    san[j] = NULL;

    /*
     * Debugging message to print arguments to check if nulls
     * are being sanitised or not.  They should but you never
     * know with C these days...
     *
     *  for (size_t k = 0; k < j; ++k) {
     *      printf("arg[%zu]: %s\n", k, san[k]);
     *  }
     */

    const pid_t pid = fork();
    if (pid == 0) {
        if (background && cb == NULL) {
            // If we run in the background lets disconnect from the parent...
            setsid();

            // Perform a second fork to ensure the background process is reaped by init,
            // preventing zombie processes from hanging around after completion...
            const pid_t pid2 = fork();
            if (pid2 < 0) _exit(EXIT_FAILURE);
            if (pid2 > 0) _exit(EXIT_SUCCESS);

            // Secondary child continues here, now fully detached from the parent
            int fd;
            if (log_file && *log_file) {
                fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) fd = open("/dev/null", O_RDWR);
            } else {
                fd = open("/dev/null", O_RDWR);
            }

            if (fd != -1) {
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);

                if (fd > 2) close(fd);
            }
        }

        execvp(san[0], (char *const *) san);
        _exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Foreground process, block until child finishes execution
        // Parent process does not wait for background process
        // Destroy the intermediate child immediately to avoid zombies... oh no
        if (!background || cb == NULL) {
            waitpid(pid, NULL, 0);
        } else {
            pending_exec_pid = pid;
            pending_exec_cb = cb;
        }
    }

    if (turbo) turbo_time(0, 0);
}

void exec_watch_task(void) {
    if (pending_exec_pid <= 0) return;

    int status;
    const pid_t r = waitpid(pending_exec_pid, &status, WNOHANG);
    if (r == 0) return;

    if (r < 0) {
        pending_exec_pid = -1;
        pending_exec_cb = NULL;
        return;
    }

    const int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (pending_exec_cb) pending_exec_cb(exit_code);

    pending_exec_pid = -1;
    pending_exec_cb = NULL;
}

int set_scaling_governor(const char *governor, const int show_done) {
    if (!governor) return -1;

    if (cpu_profile_set_governor(governor) != 0) {
        LOG_ERROR(mux_module, "Failed to switch governor to '%s'", governor);
        return -1;
    }

    if (show_done) LOG_SUCCESS(mux_module, "Governor switched to '%s' successfully", governor);

    sync_gpu_frequency(governor);
    return 0;
}

int cpu_performance_apply(const int profile) {
    char governor[CPU_GOVERNOR_MAX];
    const int result = profile < 0 ? cpu_profile_reapply(governor, sizeof(governor))
                                   : cpu_profile_apply(profile, 1, governor, sizeof(governor));

    if (result < 0) {
        LOG_ERROR(
            mux_module, "Failed to apply CPU performance '%s'", profile < 0 ? "session" : cpu_profile_name(profile)
        );
        return -1;
    }

    if (governor[0]) sync_gpu_frequency(governor);
    return 0;
}

void turbo_time(const int toggle, const int show_done) {
    if (cpu_performance_apply(toggle ? cpu_profile_extreme : -1) == 0 && show_done)
        LOG_SUCCESS(mux_module, "CPU performance %s", toggle ? "raised" : "restored");
}

void set_process_name(const char *module) {
    prctl(PR_SET_NAME, module, 0, 0, 0);
}

const char *module_from_func(const char *func) {
    static char module[64];
    const char *suffix = "_main";

    size_t len = strlen(func);
    const size_t slen = strlen(suffix);

    if (len > slen && strcmp(func + len - slen, suffix) == 0) len -= slen;
    if (len >= sizeof(module)) len = sizeof(module) - 1;

    memcpy(module, func, len);
    module[len] = '\0';

    return module;
}
