#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "../common/options.h"
#include "../common/log.h"

#define LOG_DIR "/opt/muos/log"
#define LOG_SCK "/run/muos/arborist.sock"
#define LOG_PID "/run/muos/arborist.pid"
#define MSG_BIN "/opt/muos/frontend/muxmessage"

#define DEBUG_FILE   "/opt/muos/config/system/debug_mode"
#define VERBOSE_FILE "/opt/muos/config/settings/advanced/verbose"

#define MODULE_SIZE    64
#define TITLE_SIZE     128
#define LOG_CACHE_MAX  16
#define MAX_ARG_SIZE   1024
#define MAX_LINE_SIZE  1280
#define FLUSH_MS       256
#define IDLE_EXIT_MS   4096
#define DAEMON_WAIT_MS 256

typedef enum {
    LVL_INFO = 0,
    LVL_WARN,
    LVL_ERROR,
    LVL_SUCCESS,
    LVL_DEBUG
} log_level_t;

typedef struct {
    int level;
    int progress;
    char module[MODULE_SIZE];
    char title[TITLE_SIZE];
    char message[MAX_BUFFER_SIZE];
} log_packet_t;

typedef struct {
    int used;
    int fd;
    int dirty;
    uint64_t last_used_ms;
    char path[PATH_MAX];
} log_cache_t;

static volatile sig_atomic_t daemon_stop = 0;

static uint64_t now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((uint64_t) ts.tv_sec * 1000ULL) + ((uint64_t) ts.tv_nsec / 1000000ULL);
}

static int read_flag_file(const char *path) {
    FILE *fp;
    char buf[8];

    fp = fopen(path, "r");
    if (!fp) return 0;

    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return buf[0] == '1';
}

static int debug_enabled(void) {
    return read_flag_file(DEBUG_FILE);
}

static int verbose_enabled(void) {
    return read_flag_file(VERBOSE_FILE);
}

static int mode_enabled(void) {
    return debug_enabled() || verbose_enabled();
}

static int parse_int_strict(const char *str, int *out) {
    char *end = NULL;
    long val;

    if (!str || !*str) return 0;

    errno = 0;
    val = strtol(str, &end, 10);

    if (end == str || *end != '\0') return 0;
    if (errno == ERANGE || val < INT_MIN || val > INT_MAX) return 0;

    *out = (int) val;
    return 1;
}

static int parse_level(const char *s, log_level_t *level) {
    if (!s || !*s) return 0;

    if (strcasecmp(s, "info") == 0) {
        *level = LVL_INFO;
        return 1;
    }
    if (strcasecmp(s, "warn") == 0) {
        *level = LVL_WARN;
        return 1;
    }
    if (strcasecmp(s, "error") == 0) {
        *level = LVL_ERROR;
        return 1;
    }
    if (strcasecmp(s, "success") == 0) {
        *level = LVL_SUCCESS;
        return 1;
    }
    if (strcasecmp(s, "debug") == 0) {
        *level = LVL_DEBUG;
        return 1;
    }

    return 0;
}

static const char *level_plain_symbol(log_level_t level) {
    switch (level) {
        case LVL_WARN:
            return "!";
        case LVL_ERROR:
            return "-";
        case LVL_SUCCESS:
            return "+";
        case LVL_DEBUG:
            return "?";
        case LVL_INFO:
        default:
            return "*";
    }
}

static const char *level_colour_symbol(log_level_t level) {
    switch (level) {
        case LVL_WARN:
            return WARN_SYMBOL;
        case LVL_ERROR:
            return ERROR_SYMBOL;
        case LVL_SUCCESS:
            return SUCCESS_SYMBOL;
        case LVL_DEBUG:
            return DEBUG_SYMBOL;
        case LVL_INFO:
        default:
            return INFO_SYMBOL;
    }
}

static int ensure_dir(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }

    if (mkdir(path, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;

    return -1;
}

static void sanitise_module_name(char *s) {
    char *base;
    char *dot;
    char *p;

    if (!s || !*s) return;

    base = strrchr(s, '/');
    if (base) memmove(s, base + 1, strlen(base + 1) + 1);

    dot = strrchr(s, '.');
    if (dot) *dot = '\0';

    for (p = s; *p; ++p) if (!(isalnum((unsigned char) *p) || *p == '_' || *p == '-')) *p = '_';

    if (!*s) snprintf(s, MODULE_SIZE, "unknown");
}

static void detect_module(char *module) {
    const char *env;
    char path[64];
    FILE *fp;

    env = getenv("MUOS_MODULE");
    if (env && *env) {
        snprintf(module, MODULE_SIZE, "%s", env);
        sanitise_module_name(module);
        return;
    }

    snprintf(path, sizeof(path), "/proc/%d/cmdline", getppid());
    fp = fopen(path, "r");
    if (fp) {
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);

        if (n > 0) {
            buf[n] = '\0';

            snprintf(module, MODULE_SIZE, "%s", buf);
            sanitise_module_name(module);

            return;
        }
    }

    snprintf(path, sizeof(path), "/proc/%d/comm", getppid());
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(module, MODULE_SIZE, fp)) {
            module[strcspn(module, "\r\n")] = '\0';
            fclose(fp);
            sanitise_module_name(module);
            return;
        }

        fclose(fp);
    }

    snprintf(module, MODULE_SIZE, "unknown");
}

static void join_args(char *out, int argc, char *argv[], int start_index) {
    int i;
    size_t used = 0;

    out[0] = '\0';

    for (i = start_index; i < argc; i++) {
        int wrote;

        if (used >= MAX_ARG_SIZE - 1) break;

        if (i > start_index) {
            wrote = snprintf(out + used, MAX_ARG_SIZE - used, " ");
            if (wrote < 0) break;
            if ((size_t) wrote >= MAX_ARG_SIZE - used) {
                out[MAX_ARG_SIZE - 1] = '\0';
                return;
            }
            used += (size_t) wrote;
        }

        wrote = snprintf(out + used, MAX_ARG_SIZE - used, "%s", argv[i]);
        if (wrote < 0) break;
        if ((size_t) wrote >= MAX_ARG_SIZE - used) {
            out[MAX_ARG_SIZE - 1] = '\0';
            return;
        }

        used += (size_t) wrote;
    }
}

static int write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t wr = write(fd, buf, len);

        if (wr < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        buf += wr;
        len -= (size_t) wr;
    }

    return 0;
}

static void make_date_time(char *date_buf, size_t date_sz, char *time_buf, size_t time_sz) {
    struct tm tm_now;
    time_t now = time(NULL);

    localtime_r(&now, &tm_now);

    if (date_buf) {
        snprintf(date_buf, date_sz, "%04d_%02d_%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
    }

    if (time_buf) {
        snprintf(time_buf, time_sz, "%04d-%02d-%02d %02d:%02d:%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                 tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    }
}

static int build_line(char *line, log_level_t level, const char *module, const char *message) {
    char time_buffer[20];
    char truncated_module[20];
    struct timespec ts;
    double uptime = 0.0;

    make_date_time(NULL, 0, time_buffer, sizeof(time_buffer));

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        uptime = (double) ts.tv_sec + ((double) ts.tv_nsec / 1000000000.0);
    }

    snprintf(truncated_module, sizeof(truncated_module), "%.19s", module);

    return snprintf(line, MAX_LINE_SIZE, "[%.2f]\t[%s] [%s] [%s]\t%s\n",
                    uptime, time_buffer, level_plain_symbol(level),
                    truncated_module, message);
}

static void emit_stderr(log_level_t level, const char *module, const char *message) {
    char time_buffer[20];
    char truncated_module[20];
    struct timespec ts;
    double uptime = 0.0;

    make_date_time(NULL, 0, time_buffer, sizeof(time_buffer));

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        uptime = (double) ts.tv_sec + ((double) ts.tv_nsec / 1000000000.0);
    }

    snprintf(truncated_module, sizeof(truncated_module), "%.19s", module);

    fprintf(stderr, "[%.2f]\t[%s] [%s] [%s]\t%s\n",
            uptime, time_buffer, level_colour_symbol(level),
            truncated_module, message);
    fflush(stderr);
}

static void show_message(int progress, const char *title, const char *message) {
    static uint64_t last_msg_time = 0;
    uint64_t now = now_ms();

    if (now - last_msg_time < FLUSH_MS) return;
    last_msg_time = now;

    pid_t pid;

    char progress_str[16];
    char msg_buffer[MAX_BUFFER_SIZE];

    if (progress < 0) return;
    if (!verbose_enabled()) return;
    if (access(MSG_BIN, X_OK) != 0) return;

    snprintf(progress_str, sizeof(progress_str), "%d", progress);
    snprintf(msg_buffer, sizeof(msg_buffer), "%s\n\n%s", title, message);

    pid = fork();
    if (pid != 0) return;

    setsid();

    {
        int the_void = open("/dev/null", O_RDWR);
        if (the_void >= 0) {
            dup2(the_void, STDIN_FILENO);
            dup2(the_void, STDOUT_FILENO);
            dup2(the_void, STDERR_FILENO);
            if (the_void > 2) close(the_void);
        }
    }

    execl(MSG_BIN, "muxmessage", progress_str, msg_buffer, (char *) NULL);
    _exit(127);
}

static int direct_write_packet(const log_packet_t *pkt) {
    char date_buffer[16];

    char logfile[PATH_MAX];
    char line[MAX_BUFFER_SIZE + 256];

    int fd;
    int line_len;

    if (ensure_dir(LOG_DIR) < 0) return -1;

    make_date_time(date_buffer, sizeof(date_buffer), NULL, 0);

    snprintf(logfile, sizeof(logfile), "%s/%s_%s.log", LOG_DIR, date_buffer, pkt->module);

    line_len = build_line(line, (log_level_t) pkt->level, pkt->module, pkt->message);
    if (line_len < 0) return -1;
    if ((size_t) line_len >= sizeof(line)) line_len = (int) sizeof(line) - 1;

    fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) return -1;

    if (write_all(fd, line, (size_t) line_len) < 0) {
        close(fd);
        return -1;
    }

    if (fdatasync(fd) < 0) {
        close(fd);
        return -1;
    }

    close(fd);

    emit_stderr((log_level_t) pkt->level, pkt->module, pkt->message);
    show_message(pkt->progress, pkt->title, pkt->message);

    return 0;
}

static void close_cache_entry(log_cache_t *entry) {
    if (!entry || !entry->used) return;

    if (entry->fd >= 0) {
        if (entry->dirty) fdatasync(entry->fd);
        close(entry->fd);
    }

    entry->used = 0;
    entry->fd = -1;
    entry->dirty = 0;
    entry->last_used_ms = 0;
    entry->path[0] = '\0';
}

static int cache_find(log_cache_t cache[], const char *path) {
    int i;
    for (i = 0; i < LOG_CACHE_MAX; i++) if (cache[i].used && strcmp(cache[i].path, path) == 0) return i;

    return -1;
}

static int cache_alloc(log_cache_t cache[]) {
    int i;
    int oldest = -1;
    uint64_t oldest_ms = UINT64_MAX;

    for (i = 0; i < LOG_CACHE_MAX; i++) {
        if (!cache[i].used) return i;
    }

    for (i = 0; i < LOG_CACHE_MAX; i++) {
        if (cache[i].last_used_ms < oldest_ms) {
            oldest_ms = cache[i].last_used_ms;
            oldest = i;
        }
    }

    if (oldest >= 0) close_cache_entry(&cache[oldest]);
    return oldest;
}

static int daemon_write_packet(log_cache_t cache[], const log_packet_t *pkt) {
    char date_buffer[16];

    char logfile[PATH_MAX];
    char line[MAX_BUFFER_SIZE + 256];

    int idx;
    int line_len;

    if (ensure_dir(LOG_DIR) < 0) return -1;

    make_date_time(date_buffer, sizeof(date_buffer), NULL, 0);
    snprintf(logfile, sizeof(logfile), "%s/%s_%s.log", LOG_DIR, date_buffer, pkt->module);

    idx = cache_find(cache, logfile);
    if (idx < 0) {
        idx = cache_alloc(cache);
        if (idx < 0) return -1;

        cache[idx].fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (cache[idx].fd < 0) {
            cache[idx].used = 0;
            return -1;
        }

        cache[idx].used = 1;
        cache[idx].dirty = 0;
        cache[idx].last_used_ms = now_ms();
        snprintf(cache[idx].path, sizeof(cache[idx].path), "%s", logfile);
    }

    line_len = build_line(line, (log_level_t) pkt->level, pkt->module, pkt->message);
    if (line_len < 0) return -1;
    if ((size_t) line_len >= sizeof(line)) line_len = (int) sizeof(line) - 1;

    if (write_all(cache[idx].fd, line, (size_t) line_len) < 0) return -1;

    cache[idx].dirty = 1;
    cache[idx].last_used_ms = now_ms();

    emit_stderr((log_level_t) pkt->level, pkt->module, pkt->message);
    show_message(pkt->progress, pkt->title, pkt->message);

    return 0;
}

static void flush_dirty(log_cache_t cache[]) {
    int i;

    for (i = 0; i < LOG_CACHE_MAX; i++) {
        if (cache[i].used && cache[i].dirty) {
            fdatasync(cache[i].fd);
            cache[i].dirty = 0;
        }
    }
}

static void close_all_cache(log_cache_t cache[]) {
    int i;
    for (i = 0; i < LOG_CACHE_MAX; i++) close_cache_entry(&cache[i]);
}

static void sigterm_handler(int sig) {
    (void) sig;
    daemon_stop = 1;
}

static int daemon_socket_open(void) {
    int fd;
    struct sockaddr_un addr;

    unlink(LOG_SCK);

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", LOG_SCK);

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    chmod(LOG_SCK, 0666);
    return fd;
}

static void write_pid_file(void) {
    FILE *fp = fopen(LOG_PID, "w");
    if (!fp) return;
    fprintf(fp, "%ld\n", (long) getpid());
    fclose(fp);
}

static void cleanup_daemon(void) {
    unlink(LOG_SCK);
    unlink(LOG_PID);
}

static int run_daemon(void) {
    int sock;
    struct pollfd pfd;
    log_cache_t cache[LOG_CACHE_MAX];
    uint64_t last_rx_ms;
    int i;

    if (ensure_dir("/run/muos") < 0) return 1;
    if (ensure_dir(LOG_DIR) < 0) return 1;

    memset(cache, 0, sizeof(cache));
    for (i = 0; i < LOG_CACHE_MAX; i++) cache[i].fd = -1;

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);

    sock = daemon_socket_open();
    if (sock < 0) return 1;

    write_pid_file();

    pfd.fd = sock;
    pfd.events = POLLIN;
    last_rx_ms = now_ms();

    while (!daemon_stop) {
        int pr = poll(&pfd, 1, FLUSH_MS);

        if (pr > 0 && (pfd.revents & POLLIN)) {
            for (;;) {
                log_packet_t pkt;
                ssize_t rd = recv(sock, &pkt, sizeof(pkt), MSG_DONTWAIT);

                if (rd < 0) {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    break;
                }

                if ((size_t) rd < offsetof(log_packet_t, message)) continue;

                pkt.module[MODULE_SIZE - 1] = '\0';
                pkt.title[TITLE_SIZE - 1] = '\0';
                pkt.message[MAX_BUFFER_SIZE - 1] = '\0';

                sanitise_module_name(pkt.module);
                daemon_write_packet(cache, &pkt);
                last_rx_ms = now_ms();
            }
        }

        flush_dirty(cache);

        if (!mode_enabled() && (now_ms() - last_rx_ms) >= IDLE_EXIT_MS) {
            break;
        }
    }

    flush_dirty(cache);
    close_all_cache(cache);
    close(sock);
    cleanup_daemon();

    return 0;
}

static int socket_exists(void) {
    struct stat st;
    return stat(LOG_SCK, &st) == 0 && S_ISSOCK(st.st_mode);
}

static int spawn_daemon(const char *self_path) {
    pid_t pid;

    if (!self_path || !*self_path) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) return 0;

    if (setsid() < 0) _exit(1);

    pid = fork();
    if (pid < 0) _exit(1);
    if (pid > 0) _exit(0);

    {
        int the_void = open("/dev/null", O_RDWR);
        if (the_void >= 0) {
            dup2(the_void, STDIN_FILENO);
            dup2(the_void, STDOUT_FILENO);
            dup2(the_void, STDERR_FILENO);
            if (the_void > 2) close(the_void);
        }
    }

    execl(self_path, self_path, "--daemon", (char *) NULL);
    _exit(127);
}

static int ensure_daemon_running(const char *self_path) {
    uint64_t start_ms;

    if (socket_exists()) return 0;
    if (spawn_daemon(self_path) < 0) return -1;

    start_ms = now_ms();
    while ((now_ms() - start_ms) < DAEMON_WAIT_MS) {
        if (socket_exists()) return 0;
        usleep(10 * 1000);
    }

    return socket_exists() ? 0 : -1;
}

static int send_packet(const log_packet_t *pkt) {
    int fd;
    struct sockaddr_un addr;
    ssize_t wr;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", LOG_SCK);

    wr = sendto(fd, pkt, sizeof(*pkt), 0, (struct sockaddr *) &addr, sizeof(addr));
    close(fd);

    return (wr == (ssize_t) sizeof(*pkt)) ? 0 : -1;
}

static int build_packet_from_args(int argc, char *argv[], log_packet_t *pkt) {
    log_level_t level;

    int progress;
    int arg_num;

    if (argc < 5) return -1;
    if (!parse_level(argv[1], &level)) return -1;

    memset(pkt, 0, sizeof(*pkt));
    pkt->level = (int) level;
    pkt->progress = -1;

    if (parse_int_strict(argv[2], &progress)) {
        detect_module(pkt->module);
        pkt->progress = progress;
        arg_num = 3;
    } else {
        snprintf(pkt->module, sizeof(pkt->module), "%s", argv[2]);
        sanitise_module_name(pkt->module);

        if (argc < 6) return -1;
        if (!parse_int_strict(argv[3], &progress)) return -1;

        pkt->progress = progress;
        arg_num = 4;
    }

    if (argc <= arg_num) return -1;
    snprintf(pkt->title, sizeof(pkt->title), "%s", argv[arg_num++]);

    if (argc <= arg_num) return -1;
    join_args(pkt->message, argc, argv, arg_num);

    return 0;
}

int main(int argc, char *argv[]) {
    log_packet_t pkt;

    if (argc == 2 && strcmp(argv[1], "--daemon") == 0) {
        if (!mode_enabled()) return 0;
        return run_daemon();
    }

    if (!mode_enabled()) return 0;

    if (build_packet_from_args(argc, argv, &pkt) < 0) return 1;

    if (ensure_daemon_running(argv[0]) == 0) {
        if (send_packet(&pkt) == 0) return 0;
    }

    if (direct_write_packet(&pkt) < 0) {
        emit_stderr(LVL_ERROR, "arborist", "Failed to write log file");
        return 1;
    }

    return 0;
}
