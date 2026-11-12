#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define PF_KTHREAD    0x00200000ULL
#define MAX_CORES     256
#define MAX_PROCESSES 32768
#define MAX_WIDTH     240
#define MAX_HEIGHT    100
#define BAR_WIDTH     22

struct cpu {
    unsigned number;
    uint64_t total, idle, wait;
    double usage;
};

struct process {
    pid_t pid, ppid;
    uid_t uid;
    unsigned threads;
    char state;
    bool muos;
    uint64_t ticks, started, rss_kib;
    double cpu_pct, mem_pct;
    char name[128], cmd[192], exe[192];
};

struct snapshot {
    struct process *proc;
    size_t count, cap;
    struct cpu cores[MAX_CORES], aggregate;
    size_t ncores;
    uint64_t mem_total, mem_avail, swap_total, swap_free;
    double load[3], uptime, iowait_pct;
    char device[96], theme[64], network[64];
    char battery[64], thermal[32], freq[32];
    bool idle, overlay_off, debug;
};

struct app {
    struct snapshot now, prev;
    struct process **visible;
    size_t visible_count, visible_cap;
    int sort, reverse, only_muos, commands, details, all_cores, help;
    pid_t selected;
    size_t scroll;
    int rows, cols, colour, interactive, gamepad_fd;
    struct termios original;
    bool terminal_raw;
};

struct screen {
    int row, rows, cols, colour;
};

enum action {
    ACT_NONE,
    ACT_QUIT,
    ACT_REFRESH,
    ACT_UP,
    ACT_DOWN,
    ACT_PGUP,
    ACT_PGDOWN,
    ACT_TOP,
    ACT_BOTTOM,
    ACT_DETAILS,
    ACT_HELP,
    ACT_FILTER,
    ACT_COMMAND,
    ACT_CORES,
    ACT_SORT_CPU,
    ACT_SORT_MEM,
    ACT_SORT_PID,
    ACT_SORT_NAME,
    ACT_REVERSE,
    ACT_NEXT_SORT,
    ACT_PREV_SORT,
    ACT_BACK
};

static volatile sig_atomic_t done, resized;
static struct app *active_app;
static int sort_mode, sort_reverse;

static void on_signal(const int sig) {
    if (sig == SIGWINCH) {
        resized = 1;
    } else {
        done = 1;
    }
}

static void sanitise(char *s) {
    for (; *s; ++s) {
        const unsigned char c = (unsigned char) *s;
        if (c < 32 || c >= 127) *s = '?';
    }
}

static bool read_text(const char *path, char *dst, const size_t size) {
    dst[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return false;

    size_t n = fread(dst, 1, size - 1, f);
    fclose(f);
    dst[n] = 0;
    while (n && (dst[n - 1] == '\n' || dst[n - 1] == '\r' || dst[n - 1] == 0 || dst[n - 1] == ' '))
        dst[--n] = 0;
    sanitise(dst);
    return n > 0;
}

static void read_link_or_text(const char *path, char *dst, const size_t size) {
    const ssize_t len = readlink(path, dst, size - 1);
    if (len >= 0) {
        dst[len] = 0;
        sanitise(dst);
    } else {
        read_text(path, dst, size);
    }
}

static bool read_cpus(struct snapshot *s) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return false;

    char line[512];
    while (fgets(line, sizeof(line), f) && !strncmp(line, "cpu", 3)) {
        unsigned long long v[10] = {0};
        char label[32];
        const int n = sscanf(
            line, "%31s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", label, &v[0], &v[1], &v[2], &v[3], &v[4],
            &v[5], &v[6], &v[7], &v[8], &v[9]
        );
        if (n < 5) continue;

        struct cpu *c;
        if (!strcmp(label, "cpu")) {
            c = &s->aggregate;
        } else {
            char *end;
            const unsigned long id = strtoul(label + 3, &end, 10);
            if (!label[3] || *end || id > UINT_MAX || s->ncores == MAX_CORES) continue;
            c = &s->cores[s->ncores++];
            c->number = (unsigned) id;
        }

        for (int i = 0; i < 8; ++i)
            c->total += v[i];
        c->idle = v[3] + v[4];
        c->wait = v[4];
    }

    fclose(f);
    return s->aggregate.total != 0;
}

static void read_memory(struct snapshot *s) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;

    uint64_t free_kib = 0, buffers = 0, cached = 0, reclaim = 0;
    bool available_seen = false;
    char key[48], line[256];
    unsigned long long value;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%47[^:]: %llu", key, &value) != 2) continue;
        if (!strcmp(key, "MemTotal")) {
            s->mem_total = value;
        } else if (!strcmp(key, "MemAvailable")) {
            s->mem_avail = value;
            available_seen = true;
        } else if (!strcmp(key, "MemFree")) {
            free_kib = value;
        } else if (!strcmp(key, "Buffers")) {
            buffers = value;
        } else if (!strcmp(key, "Cached")) {
            cached = value;
        } else if (!strcmp(key, "SReclaimable")) {
            reclaim = value;
        } else if (!strcmp(key, "SwapTotal")) {
            s->swap_total = value;
        } else if (!strcmp(key, "SwapFree")) {
            s->swap_free = value;
        }
    }

    fclose(f);
    if (!available_seen) s->mem_avail = free_kib + buffers + cached + reclaim;
    if (s->mem_avail > s->mem_total) s->mem_avail = s->mem_total;
}

static void read_cmdline(const long pid, char *out, const size_t size) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);

    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;

    ssize_t n = read(fd, out, size - 1);
    close(fd);
    if (n <= 0) return;

    for (ssize_t i = 0; i < n; ++i) {
        if (!out[i]) {
            out[i] = ' ';
        } else if ((unsigned char) out[i] < 32 || (unsigned char) out[i] > 126) {
            out[i] = '?';
        }
    }

    while (n > 0 && out[n - 1] == ' ')
        --n;
    out[n] = 0;
}

static bool read_process(const char *pid_text, struct process *p, const long page_kib) {
    char path[80], line[4096];
    char *tail, *save, *token;
    unsigned long long flags = 0, utime = 0, stime = 0, start = 0;
    long long rss = 0;
    unsigned threads = 0;
    pid_t ppid = 0;

    errno = 0;
    const long id = strtol(pid_text, &tail, 10);
    if (errno || *tail || id <= 0 || id > INT_MAX) return false;
    p->pid = (pid_t) id;

    snprintf(path, sizeof(path), "/proc/%ld/stat", id);
    FILE *f = fopen(path, "r");
    if (!f) return false;

    const bool ok = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    if (!ok) return false;

    const char *open_paren = strchr(line, '(');
    char *close_paren = strrchr(line, ')');
    if (!open_paren || !close_paren || close_paren <= open_paren || close_paren[1] != ' ') return false;

    size_t name_len = (size_t) (close_paren - open_paren - 1);
    if (name_len >= sizeof(p->name)) name_len = sizeof(p->name) - 1;
    memcpy(p->name, open_paren + 1, name_len);
    p->name[name_len] = 0;
    sanitise(p->name);

    int field = 3;
    for (token = strtok_r(close_paren + 2, " \t\n", &save); token; token = strtok_r(NULL, " \t\n", &save), ++field) {
        switch (field) {
            case 3:
                p->state = token[0];
                break;
            case 4:
                ppid = (pid_t) strtol(token, NULL, 10);
                break;
            case 9:
                flags = strtoull(token, NULL, 10);
                break;
            case 14:
                utime = strtoull(token, NULL, 10);
                break;
            case 15:
                stime = strtoull(token, NULL, 10);
                break;
            case 20:
                threads = (unsigned) strtoul(token, NULL, 10);
                break;
            case 22:
                start = strtoull(token, NULL, 10);
                break;
            case 24:
                rss = strtoll(token, NULL, 10);
                break;
            default:
                break;
        }
        if (field == 24) break;
    }
    if (field != 24 || (flags & PF_KTHREAD)) return false;

    snprintf(path, sizeof(path), "/proc/%ld", id);
    struct stat st;
    if (stat(path, &st) == 0) p->uid = st.st_uid;

    p->ppid = ppid;
    p->threads = threads;
    p->ticks = utime + stime;
    p->started = start;
    p->rss_kib = rss > 0 ? (uint64_t) rss * (uint64_t) page_kib : 0;

    read_cmdline(id, p->cmd, sizeof(p->cmd));

    snprintf(path, sizeof(path), "/proc/%ld/exe", id);
    const ssize_t len = readlink(path, p->exe, sizeof(p->exe) - 1);
    if (len > 0) {
        p->exe[len] = 0;
        sanitise(p->exe);
    }

    p->muos = !strncmp(p->exe, "/opt/muos/", 10) || strstr(p->cmd, "/opt/muos/") != NULL || !strncmp(p->name, "mux", 3)
              || !strncmp(p->name, "muvar", 5) || !strncmp(p->name, "muhotkey", 8) || !strcasecmp(p->name, "pickles");
    return true;
}

static bool read_processes(struct snapshot *s) {
    DIR *dir = opendir("/proc");
    if (!dir) return false;

    long page_kib = sysconf(_SC_PAGESIZE) / 1024;
    if (page_kib <= 0) page_kib = 4;

    const struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s->count < MAX_PROCESSES) {
        if (!isdigit((unsigned char) entry->d_name[0])) continue;

        struct process p = {0};
        if (!read_process(entry->d_name, &p, page_kib)) continue;

        if (s->count == s->cap) {
            const size_t capacity = s->cap ? s->cap * 2 : 128;
            struct process *grown = realloc(s->proc, capacity * sizeof(*grown));
            if (!grown) {
                closedir(dir);
                return false;
            }
            s->proc = grown;
            s->cap = capacity;
        }
        s->proc[s->count++] = p;
    }

    closedir(dir);
    return true;
}

static void read_battery(struct snapshot *s) {
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return;

    char buf[128], path[256];
    const struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%.160s/type", e->d_name);
        if (!read_text(path, buf, sizeof(buf)) || strcmp(buf, "Battery")) continue;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%.160s/capacity", e->d_name);
        if (read_text(path, buf, sizeof(buf))) {
            const int pct = atoi(buf);
            if (pct >= 0 && pct <= 100) snprintf(s->battery, sizeof(s->battery), "%d%%", pct);
        }

        snprintf(path, sizeof(path), "/sys/class/power_supply/%.160s/status", e->d_name);
        if (s->battery[0] && read_text(path, buf, sizeof(buf))) {
            const size_t n = strlen(s->battery);
            snprintf(s->battery + n, sizeof(s->battery) - n, " %s", buf);
        }
        break;
    }

    closedir(d);
}

static void read_hardware(struct snapshot *s) {
    char buf[128];

    read_link_or_text("/sys/firmware/devicetree/base/model", s->device, sizeof(s->device));
    if (!s->device[0]) read_link_or_text("/proc/device-tree/model", s->device, sizeof(s->device));
    if (!s->device[0] && gethostname(s->device, sizeof(s->device) - 1) != 0) s->device[0] = 0;

    read_link_or_text("/run/muos/storage/theme/active", s->theme, sizeof(s->theme));
    const char *slash = strrchr(s->theme, '/');
    if (slash) memmove(s->theme, slash + 1, strlen(slash + 1) + 1);

    read_text("/run/muos/network.status", s->network, sizeof(s->network));

    s->idle = access("/run/muos/is_idle", F_OK) == 0;
    if (s->idle && read_text("/run/muos/is_idle", buf, sizeof(buf)) && !strcmp(buf, "0")) s->idle = false;
    s->overlay_off = access("/run/muos/overlay.disable", F_OK) == 0;
    s->debug = read_text("/opt/muos/config/system/debug_mode", buf, sizeof(buf)) && buf[0] == '1';

    if (read_text("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", buf, sizeof(buf))) {
        const unsigned long khz = strtoul(buf, NULL, 10);
        if (khz) snprintf(s->freq, sizeof(s->freq), "%.2fGHz", (double) khz / 1000000.0);
    }

    read_battery(s);

    if (read_text("/sys/class/thermal/thermal_zone0/temp", buf, sizeof(buf))) {
        const long val = strtol(buf, NULL, 10);
        if (val > -100000 && val < 200000) snprintf(s->thermal, sizeof(s->thermal), "%.1fC", (double) val / 1000.0);
    }
}

static bool capture(struct snapshot *s) {
    memset(s, 0, sizeof(*s));
    if (!read_cpus(s)) return false;
    read_memory(s);
    if (!read_processes(s)) return false;

    FILE *f = fopen("/proc/loadavg", "r");
    if (f) {
        if (fscanf(f, "%lf %lf %lf", &s->load[0], &s->load[1], &s->load[2]) != 3)
            s->load[0] = s->load[1] = s->load[2] = 0;
        fclose(f);
    }

    f = fopen("/proc/uptime", "r");
    if (f) {
        if (fscanf(f, "%lf", &s->uptime) != 1) s->uptime = 0;
        fclose(f);
    }

    read_hardware(s);
    return true;
}

static int by_pid(const void *a, const void *b) {
    const pid_t x = ((const struct process *) a)->pid;
    const pid_t y = ((const struct process *) b)->pid;
    return (x > y) - (x < y);
}

static const struct cpu *find_cpu(const struct snapshot *s, const unsigned id) {
    for (size_t i = 0; i < s->ncores; ++i) {
        if (s->cores[i].number == id) return &s->cores[i];
    }
    return NULL;
}

static double usage(const struct cpu *now, const struct cpu *prev) {
    if (!prev || now->total <= prev->total) return 0;

    const uint64_t dt = now->total - prev->total;
    uint64_t di = now->idle >= prev->idle ? now->idle - prev->idle : 0;
    if (di > dt) di = dt;
    return 100.0 * (double) (dt - di) / (double) dt;
}

static void calculate(struct snapshot *s, const struct snapshot *prev) {
    s->aggregate.usage = usage(&s->aggregate, &prev->aggregate);
    for (size_t i = 0; i < s->ncores; ++i)
        s->cores[i].usage = usage(&s->cores[i], find_cpu(prev, s->cores[i].number));

    const uint64_t delta = s->aggregate.total > prev->aggregate.total ? s->aggregate.total - prev->aggregate.total : 0;
    const uint64_t wait = s->aggregate.wait >= prev->aggregate.wait ? s->aggregate.wait - prev->aggregate.wait : 0;
    s->iowait_pct = delta && wait <= delta ? 100.0 * (double) wait / (double) delta : 0;

    if (prev->count) qsort(prev->proc, prev->count, sizeof(prev->proc[0]), by_pid);
    const double cores = (double) (s->ncores ? s->ncores : 1);

    for (size_t i = 0; i < s->count; ++i) {
        struct process *p = &s->proc[i];
        const struct process probe = {.pid = p->pid};
        const struct process *old =
            prev->count ? bsearch(&probe, prev->proc, prev->count, sizeof(prev->proc[0]), by_pid) : NULL;
        if (old && old->started == p->started && p->ticks >= old->ticks && delta) {
            p->cpu_pct = 100.0 * (double) (p->ticks - old->ticks) * cores / (double) delta;
        }
        if (s->mem_total) p->mem_pct = 100.0 * (double) p->rss_kib / (double) s->mem_total;
    }
}

static int compare_process(const void *pa, const void *pb) {
    const struct process *a = pa, *b = pb;
    int c;

    switch (sort_mode) {
        case 0:
            c = (b->cpu_pct > a->cpu_pct) - (b->cpu_pct < a->cpu_pct);
            break;
        case 1:
            c = (b->rss_kib > a->rss_kib) - (b->rss_kib < a->rss_kib);
            break;
        case 2:
            c = (a->pid > b->pid) - (a->pid < b->pid);
            break;
        default:
            c = strcasecmp(a->name, b->name);
            break;
    }

    if (sort_reverse) c = -c;
    return c ? c : (a->pid > b->pid) - (a->pid < b->pid);
}

static void update_visible(struct app *a) {
    if (a->visible_cap < a->now.count) {
        struct process **grown = realloc(a->visible, a->now.count * sizeof(struct process *));
        if (!grown) {
            a->visible_count = 0;
            return;
        }
        a->visible = grown;
        a->visible_cap = a->now.count;
    }

    a->visible_count = 0;
    for (size_t i = 0; i < a->now.count; ++i) {
        if (!a->only_muos || a->now.proc[i].muos) a->visible[a->visible_count++] = &a->now.proc[i];
    }
}

static void sort_processes(struct app *a) {
    sort_mode = a->sort;
    sort_reverse = a->reverse;
    qsort(a->now.proc, a->now.count, sizeof(a->now.proc[0]), compare_process);
    update_visible(a);
}

static size_t selection_index(struct app *a) {
    for (size_t i = 0; i < a->visible_count; ++i) {
        if (a->visible[i]->pid == a->selected) return i;
    }
    a->selected = a->visible_count ? a->visible[0]->pid : 0;
    return 0;
}

static void terminal_size(struct app *a) {
    struct winsize ws = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col) {
        a->rows = ws.ws_row;
        a->cols = ws.ws_col;
    } else {
        a->rows = 24;
        a->cols = 80;
    }

    if (a->rows > MAX_HEIGHT) a->rows = MAX_HEIGHT;
    if (a->cols > MAX_WIDTH) a->cols = MAX_WIDTH;
    if (a->rows < 8) a->rows = 8;
    if (a->cols < 24) a->cols = 24;
}

__attribute__((format(printf, 3, 4))) static void line(struct screen *s, const int style, const char *format, ...) {
    if (s->row >= s->rows) return;

    char text[1024];
    va_list ap;
    va_start(ap, format);
    vsnprintf(text, sizeof(text), format, ap);
    va_end(ap);

    const int width = s->cols - 1;
    if (style && s->colour) printf("\033[%dm", style);
    printf("%-*.*s", width, width, text);
    if (style && s->colour) fputs("\033[0m", stdout);
    if (++s->row < s->rows) fputs("\r\n", stdout);
}

static void progress_bar(char *out, const size_t size, double pct, int width) {
    if (width < 2) width = 2;
    if (width > BAR_WIDTH) width = BAR_WIDTH;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    const int used = (int) (pct * width / 100.0 + 0.5);
    size_t pos = 0;
    if (pos + 1 < size) out[pos++] = '[';
    for (int i = 0; i < width && pos + 1 < size; ++i)
        out[pos++] = i < used ? '#' : '.';
    if (pos + 1 < size) out[pos++] = ']';
    out[pos] = 0;
}

static void summary_bar(struct screen *s, const char *label, const double pct, const char *suffix) {
    char bar[BAR_WIDTH + 4];
    progress_bar(bar, sizeof(bar), pct, s->cols >= 75 ? 20 : s->cols >= 52 ? 12 : 6);
    line(s, 0, "%-4s %s %5.1f%% %s", label, bar, pct, suffix);
}

static void uptime_label(const double seconds, char *out, const size_t size) {
    const unsigned long total = seconds > 0 ? (unsigned long) seconds : 0;
    const unsigned long days = total / 86400, hours = total / 3600 % 24, minutes = total / 60 % 60;
    if (days) {
        snprintf(out, size, "%lud%luh", days, hours);
    } else {
        snprintf(out, size, "%luh%lum", hours, minutes);
    }
}

static void print_help(struct screen *s) {
    static const char *const help[] = {
        " System Usage, Statistics, and Hardware Insight",
        " Manual sampling; no automatic refresh or polling.",
        " SPACE  Refresh CPU/memory/process snapshot",
        " c / m  Sort CPU / RSS memory (descending)",
        " p / n  Sort PID / name (ascending)",
        " r      Reverse sort direction",
        " j / k  Move selection (also Up / Down)",
        " PgUp / PgDn  Scroll a page; g / G top / bottom",
        " ENTER / d  Show selected process details",
        " x      Toggle process name / command line",
        " o      Show all userland / MustardOS processes",
        " t      Toggle per-core graph density",
        " h      Close help; q quit",
        "",
        " Counts contain process leaders only; kernel threads excluded.",
        " CPU% is per logical core: multi-threaded tasks can exceed 100%.",
        " MEM% uses RSS / physical RAM; RAM bar uses MemAvailable.",
        " MustardOS filter: /opt/muos/ executable/command and known names.",
        " System status is read-only; no service or process changes.",
        "",
    };

    line(s, 1, " SUSHI | HELP");
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); ++i)
        line(s, 0, "%s", help[i]);
    line(s, 2, " h / B  Return  |  q Quit");
}

static void render_processes(struct app *a, struct screen *sc) {
    const size_t count = a->visible_count;
    const int detail_rows = a->details && a->rows >= 17 ? 2 : 0;
    int height = sc->rows - sc->row - 1 - detail_rows;
    if (height < 0) height = 0;

    const size_t selected_index = selection_index(a);
    if (selected_index < a->scroll) a->scroll = selected_index;
    if (height > 0 && selected_index >= a->scroll + (size_t) height) a->scroll = selected_index - (size_t) height + 1;
    if (count && a->scroll >= count) a->scroll = count - 1;

    for (int i = 0; i < height; ++i) {
        const size_t at = a->scroll + (size_t) i;
        if (at >= count) {
            line(sc, 0, "%s", "");
            continue;
        }

        const struct process *p = a->visible[at];
        const char *cmd = a->commands && p->cmd[0] ? p->cmd : p->name;
        const bool chosen = p->pid == a->selected;
        const int style = chosen ? 7 : 0;
        const char mark = chosen ? '>' : ' ';
        const double rss_mib = (double) p->rss_kib / 1024;

        if (a->cols >= 72) {
            line(
                sc, style, " %c%6ld %6.1f %6.1f %8.1f %4u %c  %s", mark, (long) p->pid, p->cpu_pct, p->mem_pct, rss_mib,
                p->threads, p->state, cmd
            );
        } else if (a->cols >= 56) {
            line(
                sc, style, " %c%6ld %6.1f %6.1f %8.1f %c %s", mark, (long) p->pid, p->cpu_pct, p->mem_pct, rss_mib,
                p->state, cmd
            );
        } else {
            line(sc, style, " %c%6ld %6.1f %8.1f %s", mark, (long) p->pid, p->cpu_pct, rss_mib, cmd);
        }
    }

    if (!detail_rows) return;

    if (count) {
        const struct process *p = a->visible[selected_index];
        line(
            sc, 2, " PID %ld / PPID %ld / UID %lu / %u threads / %c / %s", (long) p->pid, (long) p->ppid,
            (unsigned long) p->uid, p->threads, p->state, p->exe
        );
        line(sc, 0, " CMD: %s", p->cmd[0] ? p->cmd : p->name);
    } else {
        line(sc, 0, "%s", "");
        line(sc, 0, "%s", "");
    }
}

static void render(struct app *a) {
    terminal_size(a);
    struct screen sc = {.rows = a->rows, .cols = a->cols, .colour = a->colour};
    if (a->interactive) fputs("\033[H\033[2J", stdout);

    if (a->help) {
        print_help(&sc);
        fflush(stdout);
        return;
    }

    const struct snapshot *s = &a->now;
    static const char *const order[] = {"CPU", "MEM", "PID", "NAME"};
    char uptime[32], extra[192];
    uptime_label(s->uptime, uptime, sizeof(uptime));

    size_t threads = 0;
    for (size_t i = 0; i < a->visible_count; ++i)
        threads += a->visible[i]->threads;

    line(
        &sc, 1, " SUSHI / mutop      %s      %zu userland / %zu threads", a->only_muos ? "MUOS ONLY" : "ALL PROCESSES",
        a->visible_count, threads
    );
    line(
        &sc, 0, " %.*s  UP %s   LOAD %.2f %.2f %.2f",
        a->cols >= 90   ? 42
        : a->cols >= 60 ? 20
                        : 9,
        s->device[0] ? s->device : "Linux", uptime, s->load[0], s->load[1], s->load[2]
    );

    snprintf(extra, sizeof(extra), "IO %.1f%%  %s", s->iowait_pct, s->freq);
    summary_bar(&sc, "CPU", s->aggregate.usage, extra);

    int core_limit = 0;
    if (a->rows >= 21 && a->cols >= 45) {
        core_limit = a->all_cores ? a->rows - 15 : a->rows >= 32 ? 6 : a->rows >= 25 ? 4 : 2;
        if (core_limit < 0) core_limit = 0;
        if (core_limit > (int) s->ncores) core_limit = (int) s->ncores;
    }

    for (int i = 0; i < core_limit; ++i) {
        char label[16];
        snprintf(label, sizeof(label), "C%u", s->cores[i].number);
        summary_bar(&sc, label, s->cores[i].usage, "");
    }
    if (core_limit > 0 && (size_t) core_limit < s->ncores) {
        line(&sc, 2, " %zu more cores (t: toggle density)", s->ncores - (size_t) core_limit);
    }

    const uint64_t mem_used = s->mem_total > s->mem_avail ? s->mem_total - s->mem_avail : 0;
    const double mem_pct = s->mem_total ? 100.0 * (double) mem_used / (double) s->mem_total : 0;
    snprintf(extra, sizeof(extra), "%.0f / %.0f MiB", (double) mem_used / 1024, (double) s->mem_total / 1024);
    summary_bar(&sc, "RAM", mem_pct, extra);

    if (s->swap_total) {
        const uint64_t swap_used = s->swap_total > s->swap_free ? s->swap_total - s->swap_free : 0;
        snprintf(extra, sizeof(extra), "%.0f / %.0f MiB", (double) swap_used / 1024, (double) s->swap_total / 1024);
        summary_bar(&sc, "SWAP", 100.0 * (double) swap_used / (double) s->swap_total, extra);
    }

    if (a->cols >= 45 && a->rows >= 16) {
        line(
            &sc, 0, " BAT %s  TEMP %s  %s%s%s", s->battery[0] ? s->battery : "--", s->thermal[0] ? s->thermal : "--",
            s->idle ? "IDLE " : "", s->overlay_off ? "OVERLAY OFF " : "", s->debug ? "DEBUG" : ""
        );
    }
    if (a->cols >= 60 && a->rows >= 22 && (s->network[0] || s->theme[0])) {
        line(&sc, 0, " NETWORK %.30s   THEME %.28s", s->network, s->theme);
    }

    line(
        &sc, 1, " PROCESSES  sort:%s%s  view:%s  %zu shown", order[a->sort], a->reverse ? " reversed" : "",
        a->commands ? "command" : "name", a->visible_count
    );

    if (a->cols >= 72) {
        line(&sc, 2, "%s", "     PID   CPU%   MEM%  RSS MiB  THR S   COMMAND");
    } else if (a->cols >= 56) {
        line(&sc, 2, "%s", "     PID   CPU%   MEM%  RSS MiB S  COMMAND");
    } else {
        line(&sc, 2, "%s", "     PID   CPU%  RSS MiB  COMMAND");
    }

    render_processes(a, &sc);

    if (a->interactive) {
        line(&sc, 2, "%s", " SPACE refresh  c/m/p/n sort  arrows move  d details  o filter  h help  q quit");
    } else {
        line(&sc, 2, "%s", " One-shot snapshot: mutop -1   |   Userland process leaders only");
        fputc('\n', stdout);
    }
    fflush(stdout);
}

static bool refresh(struct app *a) {
    struct snapshot next;
    if (!capture(&next)) {
        free(next.proc);
        return false;
    }

    calculate(&next, &a->prev);
    free(a->prev.proc);
    a->prev = a->now;
    a->now = next;
    sort_processes(a);
    return true;
}

static void sleep_millis(const unsigned ms) {
    struct timespec delay = {.tv_sec = ms / 1000, .tv_nsec = (long) (ms % 1000) * 1000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR && !done) {
    }
}

static void restore_terminal(void) {
    const struct app *a = active_app;
    if (!a) return;
    if (a->terminal_raw) tcsetattr(STDIN_FILENO, TCSAFLUSH, &a->original);
    if (a->interactive) {
        fputs("\033[?25h\033[?1049l", stdout);
        fflush(stdout);
    }
}

static bool initialise_terminal(struct app *a) {
    const bool stdin_tty = isatty(STDIN_FILENO);
    if (!isatty(STDOUT_FILENO) || (!stdin_tty && a->gamepad_fd < 0)) return false;

    if (stdin_tty) {
        if (tcgetattr(STDIN_FILENO, &a->original) < 0) return false;
        struct termios raw = a->original;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_iflag &= ~IXON;
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return false;
        a->terminal_raw = true;
    }

    const char *term = getenv("TERM");
    a->interactive = 1;
    a->colour = getenv("NO_COLOR") == NULL && (!term || strcmp(term, "dumb"));
    active_app = a;
    atexit(restore_terminal);
    fputs("\033[?1049h\033[?25l", stdout);
    fflush(stdout);
    return true;
}

static int next_byte(const int fd, const int timeout_ms) {
    fd_set inputs;
    FD_ZERO(&inputs);
    FD_SET(fd, &inputs);

    struct timeval tv = {.tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000};
    if (select(fd + 1, &inputs, NULL, NULL, &tv) <= 0) return -1;

    unsigned char ch;
    return read(fd, &ch, 1) == 1 ? ch : -1;
}

static enum action escape_action(void) {
    const int second = next_byte(STDIN_FILENO, 30);
    if (second != '[' && second != 'O') return ACT_BACK;

    const int third = next_byte(STDIN_FILENO, 30);
    switch (third) {
        case 'A':
            return ACT_UP;
        case 'B':
            return ACT_DOWN;
        case 'C':
            return ACT_NEXT_SORT;
        case 'D':
            return ACT_PREV_SORT;
        case 'H':
            return ACT_TOP;
        case 'F':
            return ACT_BOTTOM;
        case '5':
        case '6':
            next_byte(STDIN_FILENO, 30);
            return third == '5' ? ACT_PGUP : ACT_PGDOWN;
        default:
            return ACT_NONE;
    }
}

static enum action key_action(const int key) {
    switch (key) {
        case 'q':
        case 'Q':
            return ACT_QUIT;
        case ' ':
            return ACT_REFRESH;
        case 'c':
        case 'C':
            return ACT_SORT_CPU;
        case 'm':
        case 'M':
            return ACT_SORT_MEM;
        case 'p':
        case 'P':
            return ACT_SORT_PID;
        case 'n':
        case 'N':
            return ACT_SORT_NAME;
        case 'r':
        case 'R':
            return ACT_REVERSE;
        case 'o':
        case 'O':
            return ACT_FILTER;
        case 'x':
        case 'X':
            return ACT_COMMAND;
        case 't':
        case 'T':
            return ACT_CORES;
        case 'h':
        case 'H':
            return ACT_HELP;
        case 'd':
        case 'D':
        case '\r':
        case '\n':
            return ACT_DETAILS;
        case 'j':
            return ACT_DOWN;
        case 'k':
            return ACT_UP;
        case 'g':
            return ACT_TOP;
        case 'G':
            return ACT_BOTTOM;
        case 27:
            return escape_action();
        default:
            return ACT_NONE;
    }
}

static enum action gamepad_action(const int fd) {
    struct input_event event;
    if (read(fd, &event, sizeof(event)) != (ssize_t) sizeof(event) || event.type != EV_KEY || event.value == 0) {
        return ACT_NONE;
    }

    switch (event.code) {
        case KEY_UP:
        case BTN_DPAD_UP:
            return ACT_UP;
        case KEY_DOWN:
        case BTN_DPAD_DOWN:
            return ACT_DOWN;
        case KEY_LEFT:
        case BTN_DPAD_LEFT:
        case BTN_TL:
            return ACT_PREV_SORT;
        case KEY_RIGHT:
        case BTN_DPAD_RIGHT:
        case BTN_TR:
            return ACT_NEXT_SORT;
        case BTN_SOUTH:
        case KEY_ENTER:
            return ACT_DETAILS;
        case BTN_EAST:
        case KEY_ESC:
            return ACT_BACK;
        case BTN_START:
            return ACT_REFRESH;
        case BTN_SELECT:
            return ACT_FILTER;
        default:
            return ACT_NONE;
    }
}

static enum action wait_action(const struct app *a) {
    const bool stdin_tty = isatty(STDIN_FILENO);

    for (;;) {
        if (resized || done) return ACT_NONE;

        fd_set inputs;
        FD_ZERO(&inputs);
        int nfds = -1;
        if (stdin_tty) {
            FD_SET(STDIN_FILENO, &inputs);
            nfds = STDIN_FILENO;
        }
        if (a->gamepad_fd >= 0) {
            FD_SET(a->gamepad_fd, &inputs);
            if (a->gamepad_fd > nfds) nfds = a->gamepad_fd;
        }
        if (nfds < 0) return ACT_QUIT;

        if (select(nfds + 1, &inputs, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            return ACT_QUIT;
        }

        if (stdin_tty && FD_ISSET(STDIN_FILENO, &inputs)) {
            unsigned char ch;
            if (read(STDIN_FILENO, &ch, 1) <= 0) return ACT_QUIT;
            return key_action(ch);
        }

        if (a->gamepad_fd >= 0 && FD_ISSET(a->gamepad_fd, &inputs)) return gamepad_action(a->gamepad_fd);
    }
}

static void navigate(struct app *a, const long amount, const int absolute) {
    const size_t count = a->visible_count;
    if (!count) return;

    const size_t current = selection_index(a);
    long dest = absolute == 1 ? 0 : absolute == 2 ? (long) count - 1 : (long) current + amount;
    if (dest < 0) dest = 0;
    if ((size_t) dest >= count) dest = (long) count - 1;
    a->selected = a->visible[dest]->pid;
}

static void set_sort(struct app *a, const int sort) {
    a->sort = sort;
    a->reverse = 0;
    sort_processes(a);
}

static void perform(struct app *a, const enum action key) {
    if (a->help && key != ACT_HELP && key != ACT_QUIT && key != ACT_BACK) return;

    const long page = a->rows - 13 > 1 ? a->rows - 13 : 1;
    switch (key) {
        case ACT_QUIT:
            done = 1;
            break;
        case ACT_REFRESH:
            refresh(a);
            break;
        case ACT_HELP:
            a->help = !a->help;
            break;
        case ACT_BACK:
            if (a->help) {
                a->help = 0;
            } else if (a->details) {
                a->details = 0;
            } else {
                done = 1;
            }
            break;
        case ACT_UP:
            navigate(a, -1, 0);
            break;
        case ACT_DOWN:
            navigate(a, 1, 0);
            break;
        case ACT_PGUP:
            navigate(a, -page, 0);
            break;
        case ACT_PGDOWN:
            navigate(a, page, 0);
            break;
        case ACT_TOP:
            navigate(a, 0, 1);
            break;
        case ACT_BOTTOM:
            navigate(a, 0, 2);
            break;
        case ACT_DETAILS:
            a->details = !a->details;
            break;
        case ACT_FILTER:
            a->only_muos = !a->only_muos;
            a->scroll = 0;
            a->selected = 0;
            update_visible(a);
            break;
        case ACT_COMMAND:
            a->commands = !a->commands;
            break;
        case ACT_CORES:
            a->all_cores = !a->all_cores;
            break;
        case ACT_REVERSE:
            a->reverse = !a->reverse;
            sort_processes(a);
            break;
        case ACT_SORT_CPU:
        case ACT_SORT_MEM:
        case ACT_SORT_PID:
        case ACT_SORT_NAME:
            set_sort(a, key - ACT_SORT_CPU);
            break;
        case ACT_NEXT_SORT:
            set_sort(a, (a->sort + 1) % 4);
            break;
        case ACT_PREV_SORT:
            set_sort(a, (a->sort + 3) % 4);
            break;
        default:
            break;
    }
}

static void usage_text(FILE *out) {
    fputs(
        "SUSHI - System Usage, Statistics, and Hardware Insight\n"
        "Usage: mutop [-1] [-e /dev/input/eventN] [-s cpu|mem|pid|name]\n"
        "  -1  Print a single plain-text snapshot (no terminal required)\n"
        "  -e  Optional read-only Linux evdev controller (never grabbed)\n"
        "  -s  Initial process sort (default: cpu)\n"
        "  -h  Show this help\n"
        "Interactive: SPACE refresh, c/m/p/n sort, arrows navigate, d details,\n"
        "             o MustardOS filter, x command, t cores, h help, q quit.\n",
        out
    );
}

static int parse_sort(const char *name) {
    static const char *const names[] = {"cpu", "mem", "pid", "name"};
    for (int i = 0; i < 4; ++i) {
        if (!strcmp(name, names[i])) return i;
    }
    return -1;
}

int main(const int argc, char **argv) {
    struct app a = {.gamepad_fd = -1};
    bool once = false;
    int opt;

    while ((opt = getopt(argc, argv, "1e:s:h")) != -1) {
        switch (opt) {
            case '1':
                once = true;
                break;
            case 'e':
                a.gamepad_fd = open(optarg, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (a.gamepad_fd < 0) {
                    perror(optarg);
                    return 1;
                }
                break;
            case 's':
                a.sort = parse_sort(optarg);
                if (a.sort < 0) {
                    usage_text(stderr);
                    return 2;
                }
                break;
            case 'h':
                usage_text(stdout);
                return 0;
            default:
                usage_text(stderr);
                return 2;
        }
    }
    if (optind != argc) {
        usage_text(stderr);
        return 2;
    }

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);

    if (!capture(&a.prev)) {
        fprintf(stderr, "mutop: cannot read /proc; procfs must be mounted.\n");
        return 1;
    }

    sleep_millis(200);
    if (!refresh(&a)) {
        fprintf(stderr, "mutop: unable to read process statistics.\n");
        free(a.prev.proc);
        return 1;
    }

    if (!once) initialise_terminal(&a);

    do {
        render(&a);
        if (!a.interactive) break;
        resized = 0;
        const enum action action = wait_action(&a);
        if (!done) perform(&a, action);
    } while (!done);

    if (a.gamepad_fd >= 0) close(a.gamepad_fd);
    free(a.prev.proc);
    free(a.now.proc);
    free(a.visible);
    return 0;
}
