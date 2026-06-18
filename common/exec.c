#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <errno.h>
#include "init.h"
#include "exec.h"
#include "log.h"
#include "language.h"
#include "ui/common.h"
#include "ui/image.h"
#include "theme.h"
#include "device.h"

static pid_t pending_exec_pid = -1;
static exec_callback pending_exec_cb = NULL;

const char **build_term_exec(const char **term_cmd, size_t *term_cnt) {
    size_t arg_count = 0;
    for (const char **p = term_cmd; p && *p; p++) arg_count++;

    size_t total_args = 16 + arg_count + 2;
    const char **exec = malloc(sizeof(char *) * total_args);
    if (!exec) return NULL;

    size_t i = 0;
    exec[i++] = (OPT_PATH "bin/muterm");
    exec[i++] = "-ro";
    exec[i++] = "-s";
    exec[i++] = (char *) theme.TERMINAL.FONT_SIZE;

    static char font_path[MAX_BUFFER_SIZE];
    if (load_terminal_resource("font", "ttf", font_path, sizeof(font_path))) {
        exec[i++] = "-f";
        exec[i++] = font_path;
    }

    exec[i++] = "--font-hinting";
    exec[i++] = (char *) theme.TERMINAL.FONT_HINT;

    static char image_path[MAX_BUFFER_SIZE];
    if (load_terminal_resource("image", "png", image_path, sizeof(image_path))) {
        exec[i++] = "-i";
        exec[i++] = image_path;
    }

    exec[i++] = "-bg";
    exec[i++] = (char *) theme.TERMINAL.BACKGROUND;
    exec[i++] = "-fg";
    exec[i++] = (char *) theme.TERMINAL.FOREGROUND;
    exec[i++] = "--";

    for (const char **p = term_cmd; p && *p; p++) exec[i++] = *p;

    exec[i] = NULL;
    if (term_cnt) *term_cnt = i;
    return exec;
}

void extract_archive(const char *filename, const char *screen) {
    size_t exec_count;
    const char *args[] = {(OPT_PATH "script/mux/extract.sh"), filename, screen, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        fade_out_screen();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);
}

void update_bootlogo(const char *next_screen) {
    size_t exec_count;
    const char *args[] = {(OPT_PATH "script/package/theme.sh"), "bootlogo", next_screen, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        fade_out_screen();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);
}

void load_assign(const char *loader, const char *rom, const char *dir, const char *sys, int forced, int app) {
    FILE *file = fopen(loader, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, loader);
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d\n%d", rom, dir, sys, forced, app);
    fclose(file);
}

void load_mux(const char *value) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, MUOS_ACT_LOAD);
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void run_exec(const char *args[], size_t size, int background, int turbo, const char *log_file, exec_callback cb) {
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

    pid_t pid = fork();
    if (pid == 0) {
        if (background && cb == NULL) {
            // If we run in the background lets disconnect from the parent...
            setsid();

            // Perform a second fork to ensure the background process is reaped by init,
            // preventing zombie processes from hanging around after completion...
            pid_t pid2 = fork();
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
        if (!background) {
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
    pid_t r = waitpid(pending_exec_pid, &status, WNOHANG);
    if (r == 0) return;

    if (r < 0) {
        pending_exec_pid = -1;
        pending_exec_cb = NULL;
        return;
    }

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (pending_exec_cb) pending_exec_cb(exit_code);

    pending_exec_pid = -1;
    pending_exec_cb = NULL;
}

int set_scaling_governor(const char *governor, int show_done) {
    if (!governor) return -1;

    FILE *fp = fopen(device.CPU.GOVERNOR, "w");
    if (!fp) {
        LOG_ERROR(mux_module, "Failed to open %s: %s", device.CPU.GOVERNOR, strerror(errno));
        return -1;
    }

    if (fprintf(fp, "%s", governor) < 0) {
        LOG_ERROR(mux_module, "Failed to write '%s' to %s: %s", governor, device.CPU.GOVERNOR, strerror(errno));
        fclose(fp);
        return -1;
    }

    if (show_done) LOG_SUCCESS(mux_module, "Governor switched to '%s' successfully", governor);

    fclose(fp);
    return 0;
}

void turbo_time(int toggle, int show_done) {
    set_scaling_governor(toggle ? "performance" : device.CPU.DEFAULT, show_done);
}

void set_process_name(const char *module) {
    prctl(PR_SET_NAME, module, 0, 0, 0);
}

const char *get_process_name(void) {
    static char module[16]; // Apparently process names can only be 16 char?
    prctl(PR_GET_NAME, module);
    module[15] = '\0';
    return module;
}

const char *module_from_func(const char *func) {
    static char module[64];
    const char *suffix = "_main";

    size_t len = strlen(func);
    size_t slen = strlen(suffix);

    if (len > slen && strcmp(func + len - slen, suffix) == 0) len -= slen;
    if (len >= sizeof(module)) len = sizeof(module) - 1;

    memcpy(module, func, len);
    module[len] = '\0';

    return module;
}
