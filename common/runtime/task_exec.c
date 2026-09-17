#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <common/runtime/exec.h>
#include <common/storage/fileio.h>
#include <common/runtime/log.h>
#include <common/runtime/task_exec.h>
#include <module/muxshare.h>

#define TASK_READ_BUDGET     8192
#define TASK_STDERR_BUDGET   2048
#define TASK_CANCEL_GRACE_MS 1500
#define TASK_KILL_GRACE_MS   3000

static task_exec_status status;

static pid_t child_pid = -1;
static pid_t child_pgid = -1;

static int fd_in = -1;
static int fd_out = -1;
static int fd_err = -1;

static task_parser parser;
static int turbo_held = 0;
static unsigned long cancel_requested_ms = 0;
static int sigterm_sent = 0;
static int task_failed = 0;

typedef struct {
    char **argv;
    size_t argc;
    task_exec_mode mode;
    int can_cancel;
    int turbo;
    char *title;
    char *success_message;
    char *failure_message;
} retained_task;

static retained_task retry_task;

static unsigned long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long) ts.tv_sec * 1000UL + (unsigned long) (ts.tv_nsec / 1000000L);
}

static void copy_text(char *dst, const size_t cap, const char *src) {
    snprintf(dst, cap, "%s", src ? src : "");
}

static void close_fd(int *fd) {
    if (*fd < 0) return;

    close(*fd);
    *fd = -1;
}

static void clear_retry_task(void) {
    for (size_t i = 0; i < retry_task.argc; i++)
        free(retry_task.argv[i]);
    free(retry_task.argv);
    free(retry_task.title);
    free(retry_task.success_message);
    free(retry_task.failure_message);
    memset(&retry_task, 0, sizeof(retry_task));
}

static int retain_task(const task_exec_spec *spec) {
    clear_retry_task();

    retry_task.argv = calloc(spec->argc + 1, sizeof(*retry_task.argv));
    if (!retry_task.argv) return -1;

    retry_task.argc = spec->argc;
    retry_task.mode = spec->mode;
    retry_task.can_cancel = spec->can_cancel;
    retry_task.turbo = spec->turbo;

    for (size_t i = 0; i < spec->argc; i++) {
        retry_task.argv[i] = strdup(spec->argv[i]);
        if (!retry_task.argv[i]) {
            clear_retry_task();
            return -1;
        }
    }

    if (spec->title) retry_task.title = strdup(spec->title);
    if (spec->success_message) retry_task.success_message = strdup(spec->success_message);
    if (spec->failure_message) retry_task.failure_message = strdup(spec->failure_message);

    if ((spec->title && !retry_task.title) || (spec->success_message && !retry_task.success_message)
        || (spec->failure_message && !retry_task.failure_message)) {
        clear_retry_task();
        return -1;
    }

    return 0;
}

static void release_turbo(void) {
    if (!turbo_held) return;

    turbo_time(0, 0);
    turbo_held = 0;
}

static int set_non_blocking(const int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int task_exec_active(void) {
    return status.state != task_state_idle;
}

const task_exec_status *task_exec_get_status(void) {
    return &status;
}

static void handle_event(const task_event *event, void *user_data) {
    (void) user_data;

    if (event->malformed) {
        task_failed = 1;
        copy_text(status.error_code, sizeof(status.error_code), "protocol_malformed");
        copy_text(status.message, sizeof(status.message), "The task sent malformed progress data.");
        return;
    }

    switch (event->type) {
        case task_event_begin:
            if (event->title[0]) copy_text(status.title, sizeof(status.title), event->title);
            break;
        case task_event_status:
            copy_text(status.status, sizeof(status.status), event->text);
            break;
        case task_event_detail:
            copy_text(status.detail, sizeof(status.detail), event->text);
            break;
        case task_event_progress:
            status.value = event->has_value ? event->value : 0;
            status.max = event->has_max ? event->max : 0;
            status.determinate = event->has_max;
            break;
        case task_event_prompt:
            status.prompt = *event;
            status.prompt_active = 1;
            status.state = task_state_prompt;
            break;
        case task_event_log:
            if (event->text[0]) LOG_INFO("task", "%s", event->text);
            break;
        case task_event_complete:
            if (event->text[0]) copy_text(status.message, sizeof(status.message), event->text);
            break;
        case task_event_error:
            copy_text(status.error_code, sizeof(status.error_code), event->code);
            if (event->text[0]) copy_text(status.message, sizeof(status.message), event->text);
            task_failed = 1;
            break;
        default:
            LOG_WARN("task", "ignoring unknown task record");
            break;
    }
}

static void drain_output(void) {
    if (fd_out < 0) return;

    char buf[1024];
    size_t total = 0;

    while (total < TASK_READ_BUDGET) {
        const ssize_t got = read(fd_out, buf, sizeof(buf));
        if (got > 0) {
            total += (size_t) got;
            task_parser_feed(&parser, buf, (size_t) got, handle_event, NULL);
            continue;
        }

        if (got == 0) {
            close_fd(&fd_out);
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        if (errno == EINTR) continue;

        close_fd(&fd_out);
        return;
    }
}

static void drain_diagnostics(void) {
    if (fd_err < 0) return;

    char buf[512];
    size_t total = 0;

    while (total < TASK_STDERR_BUDGET) {
        const ssize_t got = read(fd_err, buf, sizeof(buf) - 1);
        if (got > 0) {
            buf[got] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            if (buf[0]) LOG_WARN("task", "%s", buf);
            total += (size_t) got;
            continue;
        }

        if (got == 0) {
            close_fd(&fd_err);
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        if (errno == EINTR) continue;

        close_fd(&fd_err);
        return;
    }
}

static void finish(const int exit_code) {
    status.exit_code = exit_code;
    status.prompt_active = 0;

    // Task scripts routinely change settings with SET_VAR, and the frontend holds config
    // in memory, so without this the change is invisible until the next restart!
    refresh_config = 1;

    if (status.cancelled || exit_code != 0 || task_failed) {
        status.state = task_state_error;
        if (!status.message[0])
            copy_text(
                status.message, sizeof(status.message),
                retry_task.failure_message ? retry_task.failure_message : "The task did not complete."
            );
    } else {
        status.state = task_state_complete;
        if (!status.message[0] && retry_task.success_message)
            copy_text(status.message, sizeof(status.message), retry_task.success_message);
    }

    child_pid = -1;
    child_pgid = -1;

    close_fd(&fd_in);
    close_fd(&fd_out);
    close_fd(&fd_err);

    release_turbo();
}

static int start_task(const task_exec_spec *spec, const int retain) {
    if (!spec || !spec->argv || spec->argc == 0) return -1;
    if (task_exec_active()) return -1;

    if (retain) (void) retain_task(spec);

    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};
    int pipe_err[2] = {-1, -1};

    if (pipe(pipe_in) != 0 || pipe(pipe_out) != 0 || pipe(pipe_err) != 0) {
        LOG_ERROR("task", "could not create task pipes: %s", strerror(errno));

        for (int i = 0; i < 2; i++) {
            if (pipe_in[i] >= 0) close(pipe_in[i]);
            if (pipe_out[i] >= 0) close(pipe_out[i]);
            if (pipe_err[i] >= 0) close(pipe_err[i]);
        }
        return -1;
    }

    memset(&status, 0, sizeof(status));
    task_parser_reset(&parser);

    copy_text(status.title, sizeof(status.title), spec->title);
    status.can_cancel = spec->can_cancel;

    // A script that declares itself uninterruptible overrides anything else.
    // No launch path can offer to stop work that cannot be safely stopped halfway
    // which includes things like backups and stuff.
    if (status.can_cancel) {
        char *never = get_script_value(spec->argv[0], "NEVER_CANCEL", "0");
        if (strcmp(never, "1") == 0) {
            LOG_INFO("task", "%s declares NEVER_CANCEL, cancellation withheld", spec->argv[0]);
            status.can_cancel = 0;
        }
        free(never);
    }
    status.state = task_state_starting;
    status.started_ms = now_ms();

    cancel_requested_ms = 0;
    sigterm_sent = 0;
    task_failed = 0;

    const pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("task", "could not fork for task: %s", strerror(errno));

        for (int i = 0; i < 2; i++) {
            close(pipe_in[i]);
            close(pipe_out[i]);
            close(pipe_err[i]);
        }

        memset(&status, 0, sizeof(status));
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_err[1], STDERR_FILENO);

        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        close(pipe_err[0]);
        close(pipe_err[1]);

        setenv("MUOS_TASK_UI", "1", 1);

        execvp(spec->argv[0], (char *const *) spec->argv);
        _exit(127);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);
    close(pipe_err[1]);

    fd_in = pipe_in[1];
    fd_out = pipe_out[0];
    fd_err = pipe_err[0];

    set_non_blocking(fd_out);
    set_non_blocking(fd_err);

    child_pid = pid;
    child_pgid = pid;

    setpgid(pid, pid);

    if (spec->turbo) {
        turbo_time(1, 0);
        turbo_held = 1;
    }

    status.state = task_state_running;
    return 0;
}

int task_exec_start(const task_exec_spec *spec) {
    return start_task(spec, 1);
}

int task_exec_can_retry(void) {
    return status.state == task_state_error && !status.cancelled && child_pid <= 0 && retry_task.argc > 0;
}

int task_exec_retry(void) {
    if (!task_exec_can_retry()) return -1;

    const task_exec_spec spec = {
        .argv = (const char *const *) retry_task.argv,
        .argc = retry_task.argc,
        .mode = retry_task.mode,
        .can_cancel = retry_task.can_cancel,
        .turbo = retry_task.turbo,
        .title = retry_task.title,
        .success_message = retry_task.success_message,
        .failure_message = retry_task.failure_message,
    };

    memset(&status, 0, sizeof(status));
    return start_task(&spec, 0);
}

int task_exec_respond(const char *prompt_id, const char *value) {
    if (fd_in < 0 || !prompt_id || !value) return -1;
    if (!status.prompt_active) return -1;

    char line[TASK_LINE_MAX];
    const int len = snprintf(line, sizeof(line), "MUOS:RESPONSE\tID=%s\tVALUE=%s\n", prompt_id, value);
    if (len <= 0 || (size_t) len >= sizeof(line)) return -1;

    const ssize_t written = write(fd_in, line, (size_t) len);

    status.prompt_active = 0;
    status.state = task_state_running;

    return written == len ? 0 : -1;
}

int task_exec_cancel(void) {
    if (!task_exec_active() || child_pid <= 0) return -1;
    if (!status.can_cancel) return -1;
    if (status.state == task_state_cancelling) return 0;

    status.state = task_state_cancelling;
    status.cancelled = 1;
    status.prompt_active = 0;
    cancel_requested_ms = now_ms();

    // Ask nicely first, the signals only follow if it will not leave in time!
    if (fd_in >= 0) {
        static const char msg[] = "MUOS:CANCEL\n";
        const ssize_t written = write(fd_in, msg, sizeof(msg) - 1);
        (void) written;
    }

    return 0;
}

static void escalate_cancel(void) {
    if (status.state != task_state_cancelling || child_pid <= 0) return;

    const unsigned long waited = now_ms() - cancel_requested_ms;

    if (!sigterm_sent && waited >= TASK_CANCEL_GRACE_MS) {
        kill(child_pgid > 0 ? -child_pgid : child_pid, SIGTERM);
        sigterm_sent = 1;
        return;
    }

    if (sigterm_sent && waited >= TASK_KILL_GRACE_MS) {
        kill(child_pgid > 0 ? -child_pgid : child_pid, SIGKILL);
    }
}

void task_exec_poll(void) {
    if (!task_exec_active() || child_pid <= 0) return;

    status.elapsed_ms = now_ms() - status.started_ms;

    drain_output();
    drain_diagnostics();
    escalate_cancel();

    int wait_status = 0;
    const pid_t reaped = waitpid(child_pid, &wait_status, WNOHANG);
    if (reaped == 0) return;

    if (reaped < 0) {
        if (errno == EINTR) return;
        finish(-1);
        return;
    }

    // Whatever is still buffered belongs to this run...
    drain_output();
    drain_diagnostics();

    finish(WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1);
}

void task_exec_acknowledge(void) {
    if (status.state != task_state_complete && status.state != task_state_error) return;

    memset(&status, 0, sizeof(status));
    status.state = task_state_idle;
    clear_retry_task();
}

void task_exec_shutdown(void) {
    if (child_pid > 0) {
        kill(child_pgid > 0 ? -child_pgid : child_pid, SIGKILL);

        int wait_status = 0;
        waitpid(child_pid, &wait_status, 0);
    }

    child_pid = -1;
    child_pgid = -1;

    close_fd(&fd_in);
    close_fd(&fd_out);
    close_fd(&fd_err);

    release_turbo();

    clear_retry_task();

    memset(&status, 0, sizeof(status));
}
