#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <common/base/options.h>
#include <common/base/json_tree.h>
#include <common/content/activity_record.h>
#include <common/storage/fileio.h>

#ifndef TRACK_DIR
#define TRACK_DIR INFO_ACT_PATH
#endif
#ifndef HISTORY_DIR
#define HISTORY_DIR INFO_HIS_PATH
#endif
#ifndef WASABI_HISTORY
#define WASABI_HISTORY RUN_STORAGE_PATH "wasabi/history.tsv"
#endif
#ifndef LOCK_FILE
#define LOCK_FILE RUN_PATH "mutrack.lock"
#endif

#define CONTENT_INFO_DIR OPT_PATH "share/info/content"
#define BATTERY_DIR      RUN_PATH "battery"
#define LOG_BIN          TOOL_BIN_PATH "mulog"
#define DEBUG_FILE       CONF_CONFIG_PATH "system/debug_mode"

#define LEGACY_JSON    TRACK_DIR "/playtime_data.json"
#define LEGACY_LOCK    TRACK_DIR "/.playtime.lock"
#define HIGHWATER_FILE TRACK_DIR "/.time_highwater"
#define CURRENT_FILE   TRACK_DIR "/.current_session"
#define RUNTIME_MARKER TRACK_DIR "/.runtime_backfill_v1"
#define UNION_MARKER   TRACK_DIR "/.union_path_migration_v2"
#define DAILY_FILE     TRACK_DIR "/daily.json"

#define RECORD_VERSION 1
#define DAILY_VERSION  1
#define SESSION_LIMIT  50

#define TIME_FLOOR             1735689600LL
#define TIME_CEILING           4102444799LL
#define TIME_BACKWARD_GRACE    300
#define CLOCK_DELTA_TOLERANCE  120
#define MAX_CONTIGUOUS_SECONDS 21600
#define MAX_SESSION_SECONDS    86400
#define LOCK_WAIT_STEPS        60
#define TRACK_LOG_LIMIT        262144
#define CHECKPOINT_MIN_SECONDS 60

enum log_level { log_debug, log_info, log_warn, log_error };

typedef struct {
    const char *state;
    long long wall;
    long long uptime;
    const char *boot;
    long long accumulated;
} active_info;

typedef struct {
    char *path;
    long long time;
} seen_time;

typedef struct {
    char hash[16];
    size_t order;
    long long last_played;
    json_node *record;
} legacy_entry;

static const char *track_name = "";
static const char *track_core = "";
static const char *track_file = "";
static const char *track_action = "";
static const char *track_mode;
static const char *track_runtime;
static char *track_board;
static char *track_log;
static char *boot_id;
static char *session_id;
static long long now;
static long long uptime_now;
static int now_valid;

static char *format(const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    const int length = vsnprintf(NULL, 0, pattern, args);
    va_end(args);
    if (length < 0) return NULL;

    char *out = malloc((size_t) length + 1);
    if (!out) return NULL;

    va_start(args, pattern);
    vsnprintf(out, (size_t) length + 1, pattern, args);
    va_end(args);
    return out;
}

static char *read_line(const char *path, const size_t number, const int strip_cr) {
    FILE *file = fopen(path, "r");
    if (!file) return NULL;

    char *line = NULL;
    size_t size = 0;
    ssize_t length = -1;
    for (size_t i = 0; i < number; i++) {
        length = getline(&line, &size, file);
        if (length < 0) break;
    }
    fclose(file);

    if (length < 0) {
        free(line);
        return NULL;
    }

    if (length > 0 && line[length - 1] == '\n') line[--length] = '\0';
    if (strip_cr && length > 0 && line[length - 1] == '\r') line[length - 1] = '\0';
    return line;
}

static char *get_var(const char *base, const char *name) {
    char *path = format("%s%s", base, name);
    char *value = path ? read_line(path, 1, 1) : NULL;
    free(path);
    return value ? value : strdup("");
}

static int is_uint(const char *text) {
    if (!text || !*text) return 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
    }
    return 1;
}

static int mkdir_parents(const char *path) {
    char *copy = strdup(path);
    if (!copy) return 0;

    for (char *p = copy + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
            free(copy);
            return 0;
        }
        *p = '/';
    }

    const int made = mkdir(copy, 0755) == 0 || errno == EEXIST;
    free(copy);
    return made;
}

static int debug_mode(void) {
    char *value = read_line(DEBUG_FILE, 1, 1);
    const int mode = is_uint(value) ? atoi(value) : 0;
    free(value);
    return mode;
}

static void system_log(const enum log_level level, const char *message) {
    static const char *names[] = {"debug", "info", "warn", "error"};

    const pid_t pid = fork();
    if (pid == 0) {
        execl(LOG_BIN, LOG_BIN, names[level], "mutrack", "0", "TRACK", message, (char *) NULL);
        _exit(127);
    }
    if (pid > 0) waitpid(pid, NULL, 0);
}

static void track_log_write(const enum log_level level, const char *message) {
    char *dir = strdup(track_log);
    char *slash = dir ? strrchr(dir, '/') : NULL;
    if (slash) {
        *slash = '\0';
        mkdir_parents(dir);
    }
    free(dir);

    struct stat info;
    if (stat(track_log, &info) == 0 && info.st_size > TRACK_LOG_LIMIT) {
        char *old = format("%s.old", track_log);
        if (old) file_rename(track_log, old);
        free(old);
    }

    FILE *file = fopen(track_log, "a");
    if (!file) return;

    char stamp[32] = "";
    const time_t clock = time(NULL);
    struct tm local;
    if (localtime_r(&clock, &local)) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);

    fprintf(file, "%s [%s] %s\n", stamp, level == log_error ? "ERROR" : "WARN", message);
    fclose(file);
}

static void log_message(const enum log_level level, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    char message[2048];
    vsnprintf(message, sizeof(message), pattern, args);
    va_end(args);

    if (debug_mode() > 0) system_log(level, message);
    if (level >= log_warn) track_log_write(level, message);
}

static int write_text(const char *path, const char *text) {
    char *temp = format("%s.tmp.%ld", path, (long) getpid());
    if (!temp) return 0;

    FILE *file = fopen(temp, "w");
    int written = 0;
    if (file) {
        written = fputs(text, file) != EOF;
        written = fclose(file) == 0 && written;
    }

    if (written && file_rename(temp, path) == 0) {
        free(temp);
        return 1;
    }

    unlink(temp);
    free(temp);
    return 0;
}

static long long read_uptime(void) {
    FILE *file = fopen("/proc/uptime", "r");
    if (!file) return 0;

    unsigned long long seconds = 0;
    const int read = fscanf(file, "%llu", &seconds);
    fclose(file);
    return read == 1 ? (long long) seconds : 0;
}

static json_node *read_number(const char *path) {
    char *value = read_line(path, 1, 0);
    json_node *node = is_uint(value) ? json_tree_number(value) : json_tree_null();
    free(value);
    return node;
}

static char *read_governor(void) {
    char *path = get_var(CONF_DEVICE_PATH, "cpu/governor");
    if (!*path) {
        free(path);
        path = strdup("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
    }

    char *value = path ? read_line(path, 1, 0) : NULL;
    free(path);
    return value ? value : strdup("");
}

static int clock_in_range(const long long value) {
    return value >= TIME_FLOOR && value <= TIME_CEILING;
}

static void read_highwater(long long *wall, long long *uptime, char *boot, const size_t boot_size) {
    *wall = 0;
    *uptime = 0;
    boot[0] = '\0';

    char *line = read_line(HIGHWATER_FILE, 1, 0);
    if (!line) return;

    char *save = NULL;
    const char *fields[3] = {NULL, NULL, NULL};
    char *token = strtok_r(line, " ", &save);
    for (int i = 0; token && i < 3; i++) {
        fields[i] = token;
        token = strtok_r(NULL, i == 1 ? "" : " ", &save);
    }

    if (is_uint(fields[0])) *wall = atoll(fields[0]);
    if (is_uint(fields[1])) *uptime = atoll(fields[1]);
    if (fields[2]) snprintf(boot, boot_size, "%s", fields[2]);
    free(line);
}

static int clock_is_sane(void) {
    if (!clock_in_range(now)) return 0;

    long long wall;
    long long uptime;
    char boot[128];
    read_highwater(&wall, &uptime, boot, sizeof(boot));

    if (wall > 0 && now < wall - TIME_BACKWARD_GRACE) return 0;

    if (boot[0] && strcmp(boot, boot_id) == 0 && wall > 0 && uptime > 0 && uptime_now >= uptime) {
        long long difference = (now - wall) - (uptime_now - uptime);
        if (difference < 0) difference = -difference;
        if (difference > CLOCK_DELTA_TOLERANCE) return 0;
    }

    return 1;
}

static void update_highwater(void) {
    long long wall;
    long long uptime;
    char boot[128];
    read_highwater(&wall, &uptime, boot, sizeof(boot));
    if (now <= wall) return;

    char *text = format("%lld %lld %s\n", now, uptime_now, boot_id);
    if (!text || !write_text(HIGHWATER_FILE, text)) log_message(log_warn, "Unable to update clock high-water file");
    free(text);
}

static void clear_legacy_lock(void) {
    unlink(LEGACY_LOCK "/pid");
    unlink(LEGACY_LOCK "/time");
    unlink(LEGACY_LOCK "/boot");
    rmdir(LEGACY_LOCK);
}

static int acquire_lock(void) {
    const int fd = open(LOCK_FILE, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) {
        log_message(log_error, "Unable to open the activity tracker lock");
        return 0;
    }

    for (int step = 0; step < LOCK_WAIT_STEPS; step++) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            clear_legacy_lock();
            return 1;
        }
        usleep(100000);
    }

    close(fd);
    log_message(log_error, "Timed out waiting for activity tracker lock");
    return 0;
}

static char *content_key(const char *path) {
    const size_t size = strlen(path) + 1;
    char *key = malloc(size);
    if (key) activity_content_key(path, key, size);
    return key;
}

static char *record_path(const char *path) {
    return format("%s/%08X.json", TRACK_DIR, activity_key_hash(path));
}

static int is_record_name(const char *name) {
    if (strlen(name) != 13 || strcmp(name + 8, ".json") != 0) return 0;

    for (int i = 0; i < 8; i++) {
        const char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return 0;
    }

    return 1;
}

static int file_has_data(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && info.st_size > 0;
}

static json_node *load_record(const char *path) {
    if (!file_has_data(path)) return NULL;

    json_node *root = json_tree_load(path);
    if (root && root->type == JSON_OBJECT) return root;

    json_tree_free(root);
    return NULL;
}

static int save_record(const json_node *root, const char *path) {
    if (json_tree_write(root, path)) return 1;

    log_message(log_error, "Atomic record update failed for action '%s' and record '%s'", track_action, path);
    return 0;
}

static void set_string(json_node *object, const char *key, const char *value) {
    json_tree_set(object, key, json_tree_string(value));
}

static void set_int(json_node *object, const char *key, const long long value) {
    json_tree_set(object, key, json_tree_int(value));
}

static void add_int(json_node *object, const char *key, const long long value) {
    set_int(object, key, json_tree_int_or(json_tree_get(object, key), 0) + value);
}

static void add_to_map(json_node *root, const char *map, const char *name, const char *field, const long long value) {
    add_int(json_tree_member_object(json_tree_member_object(root, map), name), field, value);
}

static int ensure_record(const char *record, const char *path) {
    if (file_has_data(record)) {
        json_node *root = load_record(record);
        if (root) {
            json_tree_free(root);
            return 1;
        }

        char *corrupt = format("%s.corrupt.%lld", record, uptime_now);
        if (corrupt) rename(record, corrupt);
        log_message(log_error, "Invalid tracker record preserved as '%s'", corrupt ? corrupt : record);
        free(corrupt);
    }

    char *key = content_key(path);
    json_node *root = json_tree_object();
    if (!key || !root) {
        free(key);
        json_tree_free(root);
        return 0;
    }

    set_int(root, "version", RECORD_VERSION);
    set_string(root, "key", key);
    set_string(root, "path", path);

    static const char *strings[] = {"name", "system"};
    static const char *numbers[] = {"launches",    "total_time",   "first_played",
                                    "last_played", "last_session", "longest_session"};
    static const char *details[] = {"last_core", "last_device", "last_mode", "last_runtime"};
    static const char *maps[] = {"cores", "devices", "modes", "runtimes", "endings"};

    for (size_t i = 0; i < sizeof(strings) / sizeof(*strings); i++)
        set_string(root, strings[i], "");
    for (size_t i = 0; i < sizeof(numbers) / sizeof(*numbers); i++)
        set_int(root, numbers[i], 0);
    for (size_t i = 0; i < sizeof(details) / sizeof(*details); i++)
        set_string(root, details[i], "");
    for (size_t i = 0; i < sizeof(maps) / sizeof(*maps); i++)
        json_tree_set(root, maps[i], json_tree_object());
    json_tree_set(root, "sessions", json_tree_array());
    json_tree_set(root, "active", json_tree_null());

    const int written = json_tree_write(root, record);
    json_tree_free(root);
    free(key);
    return written;
}

static int read_active(const json_node *root, active_info *active) {
    const json_node *node = json_tree_get(root, "active");
    if (!json_tree_truthy(node)) return 0;

    active->state = json_tree_string_or(json_tree_get(node, "state"), "");
    active->wall = json_tree_int_or(json_tree_get(node, "start_time"), 0);
    active->uptime = json_tree_int_or(json_tree_get(node, "start_uptime"), 0);
    active->boot = json_tree_string_or(json_tree_get(node, "start_boot_id"), "-");
    if (!*active->boot) active->boot = "-";
    active->accumulated = json_tree_int_or(json_tree_get(node, "accumulated"), 0);
    if (active->accumulated < 0) active->accumulated = 0;
    return 1;
}

static int segment(const active_info *active, long long *seconds) {
    *seconds = 0;

    if (strcmp(active->boot, "-") != 0 && strcmp(active->boot, boot_id) != 0) return 0;

    if (active->uptime > 0 && uptime_now >= active->uptime && strcmp(active->boot, boot_id) == 0) {
        *seconds = uptime_now - active->uptime;
    } else if (now_valid && clock_in_range(active->wall) && now >= active->wall) {
        *seconds = now - active->wall;
    } else {
        return 0;
    }

    if (*seconds <= MAX_CONTIGUOUS_SECONDS) return 1;

    *seconds = 0;
    return 0;
}

static json_node *average_temperature(const json_node *active) {
    const long long count = json_tree_int_or(json_tree_get(active, "temperature_count"), 0);
    if (count <= 0) return json_tree_null();

    const json_node *sum = json_tree_get(active, "temperature_sum");
    const double total = sum && sum->type == JSON_NUMBER ? strtod(sum->text, NULL) : 0;

    char text[32];
    snprintf(text, sizeof(text), "%.1f", floor(total / (double) count / 100) / 10);
    const size_t length = strlen(text);
    if (length > 2 && strcmp(text + length - 2, ".0") == 0) text[length - 2] = '\0';
    return json_tree_number(text);
}

static void add_temperature(json_node *active, const json_node *temperature) {
    if (temperature->type == JSON_NULL) return;

    add_int(active, "temperature_sum", json_tree_int_or(temperature, 0));
    add_int(active, "temperature_count", 1);
}

static int local_date(const long long when, char *out, const size_t size) {
    const time_t clock = (time_t) when;
    struct tm local;
    return localtime_r(&clock, &local) && strftime(out, size, "%Y-%m-%d", &local) > 0;
}

static json_node *daily_load(void) {
    json_node *root = load_record(DAILY_FILE);
    if (!root) root = json_tree_object();

    set_int(root, "version", DAILY_VERSION);
    json_tree_member_object(root, "days");
    return root;
}

static void
daily_bump(json_node *days, const char *date, const char *runtime, const long long seconds, const long long launches) {
    json_node *day = json_tree_member_object(days, date);
    add_int(day, "time", seconds);
    add_int(day, "launches", launches);

    if (!*runtime) return;

    json_node *entry = json_tree_member_object(json_tree_member_object(day, "runtimes"), runtime);
    add_int(entry, "time", seconds);
    add_int(entry, "launches", launches);
}

static void daily_add(const long long seconds, const long long launches, const char *runtime) {
    char date[16];
    if (!now_valid || (seconds <= 0 && launches <= 0) || !local_date(now, date, sizeof(date))) return;

    json_node *root = daily_load();
    daily_bump(json_tree_get(root, "days"), date, runtime, seconds > 0 ? seconds : 0, launches);
    if (!json_tree_write(root, DAILY_FILE)) log_message(log_warn, "Unable to update the daily activity log");
    json_tree_free(root);
}

static void daily_backfill(void) {
    if (access(DAILY_FILE, F_OK) == 0) return;

    json_node *root = daily_load();
    json_node *days = json_tree_get(root, "days");
    int count = 0;

    DIR *dir = opendir(TRACK_DIR);
    if (dir) {
        const struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (!is_record_name(entry->d_name)) continue;

            char *path = format("%s/%s", TRACK_DIR, entry->d_name);
            json_node *record = path ? load_record(path) : NULL;
            const json_node *sessions = json_tree_get(record, "sessions");

            for (const json_node *session = sessions ? sessions->child : NULL; session; session = session->next) {
                const long long start = json_tree_int_or(json_tree_get(session, "start"), 0);
                const long long length = json_tree_int_or(json_tree_get(session, "length"), 0);
                char date[16];
                if (!clock_in_range(start) || !local_date(start, date, sizeof(date))) continue;

                daily_bump(
                    days, date, json_tree_string_or(json_tree_get(session, "runtime"), ""), length > 0 ? length : 0, 1
                );
                count++;
            }

            json_tree_free(record);
            free(path);
        }
        closedir(dir);
    }

    if (json_tree_write(root, DAILY_FILE)) {
        log_message(log_info, "Built the daily activity log from %d earlier sessions", count);
    } else {
        log_message(log_warn, "Unable to write the daily activity log");
    }
    json_tree_free(root);
}

static int finalise(const char *record, const char *ended, const char *exit_code) {
    json_node *root = load_record(record);
    active_info info;
    if (!root || !read_active(root, &info)) {
        json_tree_free(root);
        return 1;
    }

    long long seconds = info.accumulated;
    long long counted = 0;
    if (strcmp(info.state, "active") == 0) {
        if (segment(&info, &counted)) {
            seconds += counted;
        } else {
            log_message(log_warn, "Discarded unsafe final timing segment for '%s'", record);
        }
    }

    if (seconds > MAX_SESSION_SECONDS) {
        counted = 0;
        log_message(
            log_warn, "Rejected complete session for '%s' because it exceeded %d seconds", record, MAX_SESSION_SECONDS
        );
        seconds = 0;
    }

    const json_node *active = json_tree_get(root, "active");
    json_node *capacity = read_number(BATTERY_DIR "/capacity");
    json_node *charging = read_number(BATTERY_DIR "/charging");
    const json_node *start_capacity = json_tree_get(active, "capacity");

    json_node *battery;
    if (start_capacity && start_capacity->type != JSON_NULL && capacity->type != JSON_NULL
        && json_tree_int_or(json_tree_get(active, "charging"), 1) == 0 && json_tree_int_or(charging, 1) == 0) {
        const long long drain = json_tree_int_or(start_capacity, 0) - json_tree_int_or(capacity, 0);
        battery = json_tree_int(drain > 0 ? drain : 0);
    } else {
        battery = json_tree_null();
    }

    const char *core = json_tree_string_or(json_tree_get(active, "core"), "");
    const char *device = json_tree_string_or(json_tree_get(active, "device"), "");
    const char *mode = json_tree_string_or(json_tree_get(active, "mode"), "");
    const char *runtime = json_tree_string_or(json_tree_get(active, "runtime"), "");

    add_int(root, "total_time", seconds);
    set_int(root, "last_session", seconds);
    const long long longest = json_tree_int_or(json_tree_get(root, "longest_session"), 0);
    set_int(root, "longest_session", longest > seconds ? longest : seconds);
    add_to_map(root, "cores", core, "time", seconds);
    add_to_map(root, "devices", device, "time", seconds);
    add_to_map(root, "modes", mode, "time", seconds);
    if (*runtime) add_to_map(root, "runtimes", runtime, "time", seconds);
    add_int(json_tree_member_object(root, "endings"), ended, 1);

    json_node *session = json_tree_object();
    json_tree_set(session, "start", json_tree_value_or(json_tree_get(active, "started"), json_tree_int(0)));
    set_int(session, "length", seconds);
    set_string(session, "core", core);
    set_string(session, "runtime", runtime);
    set_string(session, "device", device);
    set_string(session, "mode", mode);
    json_tree_set(session, "battery", battery);
    json_tree_set(session, "temperature", average_temperature(active));
    json_tree_set(session, "governor", json_tree_value_or(json_tree_get(active, "governor"), json_tree_string("")));
    json_tree_set(session, "suspends", json_tree_value_or(json_tree_get(active, "suspends"), json_tree_int(0)));
    set_string(session, "ended", ended);
    json_tree_set(session, "exit", exit_code ? json_tree_number(exit_code) : json_tree_null());

    json_node *sessions = json_tree_get(root, "sessions");
    if (!sessions || sessions->type != JSON_ARRAY) sessions = json_tree_set(root, "sessions", json_tree_array());
    json_tree_push(sessions, session);
    json_tree_keep_last(sessions, SESSION_LIMIT);

    json_tree_set(root, "active", json_tree_null());

    const int saved = save_record(root, record);
    if (saved) {
        daily_add(counted, 0, runtime);
        log_message(log_debug, "Accepted session time for '%s': %lld seconds (%s)", record, seconds, ended);
    }

    json_tree_free(capacity);
    json_tree_free(charging);
    json_tree_free(root);
    return saved;
}

static void close_orphan(void) {
    char *file = read_line(CURRENT_FILE, 3, 0);
    char *hash = read_line(CURRENT_FILE, 4, 0);
    char *record = NULL;

    if (hash && *hash && !strchr(hash, '/')) {
        record = format("%s/%s", TRACK_DIR, hash);
    } else if (file && *file) {
        record = record_path(file);
    }

    json_node *root = record ? load_record(record) : NULL;
    active_info info;
    if (root && read_active(root, &info)) {
        const int lost = strcmp(info.boot, boot_id) != 0 && strcmp(info.state, "active") == 0;
        json_tree_free(root);
        root = NULL;
        finalise(record, lost ? "lost" : "interrupted", NULL);
    }

    json_tree_free(root);
    free(record);
    free(hash);
    free(file);
}

static int write_current(const char *record) {
    const char *name = strrchr(record, '/');
    char *text = format("%s\n%s\n%s\n%s\n", track_name, track_core, track_file, name ? name + 1 : record);

    if (text && write_text(CURRENT_FILE, text)) {
        free(text);
        return 1;
    }

    free(text);
    log_message(log_warn, "Unable to record the current tracking session");
    return 0;
}

static int start_tracking(void) {
    close_orphan();

    char *record = record_path(track_file);
    if (!record || !ensure_record(record, track_file)) {
        free(record);
        return 0;
    }

    long long wall = 0;
    if (now_valid) {
        wall = now;
    } else {
        log_message(
            log_warn, "System clock is unsafe for '%s'; session time uses uptime and no date is recorded", track_name
        );
    }

    json_node *root = load_record(record);
    if (!root) {
        log_message(log_error, "Atomic record update failed for action '%s' and record '%s'", track_action, record);
        free(record);
        return 0;
    }

    char *key = content_key(track_file);
    const char *system = getenv("TRACK_SYSTEM");

    set_int(root, "version", RECORD_VERSION);
    set_string(root, "key", key ? key : "");
    set_string(root, "path", track_file);
    set_string(root, "name", track_name);
    if (system && *system) {
        set_string(root, "system", system);
    } else {
        json_tree_set(root, "system", json_tree_value_or(json_tree_get(root, "system"), json_tree_string("")));
    }
    add_int(root, "launches", 1);
    if (json_tree_int_or(json_tree_get(root, "first_played"), 0) <= 0) set_int(root, "first_played", wall);
    if (wall > 0) {
        set_int(root, "last_played", wall);
    } else {
        json_tree_set(root, "last_played", json_tree_value_or(json_tree_get(root, "last_played"), json_tree_int(0)));
    }
    set_string(root, "last_core", track_core);
    set_string(root, "last_device", track_board);
    set_string(root, "last_mode", track_mode);
    set_string(root, "last_runtime", track_runtime);
    add_to_map(root, "runtimes", track_runtime, "launches", 1);
    add_to_map(root, "cores", track_core, "launches", 1);
    add_to_map(root, "devices", track_board, "launches", 1);
    add_to_map(root, "modes", track_mode, "launches", 1);

    char *governor = read_governor();
    json_node *temperature = read_number("/sys/class/thermal/thermal_zone0/temp");
    json_node *active = json_tree_object();

    set_string(active, "id", session_id);
    set_string(active, "state", "active");
    set_int(active, "started", wall);
    set_int(active, "start_time", wall);
    set_int(active, "start_uptime", uptime_now);
    set_string(active, "start_boot_id", boot_id);
    set_int(active, "accumulated", 0);
    json_tree_set(active, "capacity", read_number(BATTERY_DIR "/capacity"));
    json_tree_set(active, "charging", read_number(BATTERY_DIR "/charging"));
    set_string(active, "governor", governor ? governor : "");
    set_int(active, "suspends", 0);
    json_tree_set(
        active, "temperature_sum", temperature->type == JSON_NULL ? json_tree_int(0) : json_tree_clone(temperature)
    );
    set_int(active, "temperature_count", temperature->type == JSON_NULL ? 0 : 1);
    set_string(active, "core", track_core);
    set_string(active, "device", track_board);
    set_string(active, "mode", track_mode);
    set_string(active, "runtime", track_runtime);
    json_tree_set(root, "active", active);

    int done = save_record(root, record);
    if (done) {
        daily_add(0, 1, track_runtime);
        done = write_current(record);
    }

    json_tree_free(temperature);
    json_tree_free(root);
    free(governor);
    free(key);
    free(record);
    return done;
}

static json_node *load_active_record(const char *record, const char *step, active_info *info) {
    json_node *root = record ? load_record(record) : NULL;
    if (root && read_active(root, info)) return root;

    json_tree_free(root);
    log_message(log_warn, "No active session found on %s for '%s'", step, track_file);
    return NULL;
}

static int suspend_tracking(void) {
    char *record = record_path(track_file);
    active_info info;
    json_node *root = load_active_record(record, "suspend", &info);
    if (!root) {
        free(record);
        return 1;
    }

    long long accumulated = info.accumulated;
    long long counted = 0;
    if (strcmp(info.state, "active") == 0) {
        if (segment(&info, &counted)) {
            accumulated += counted;
        } else {
            log_message(log_warn, "Discarded unsafe pre-suspend timing segment for '%s'", track_name);
        }
    }

    if (accumulated > MAX_SESSION_SECONDS) {
        accumulated = info.accumulated;
        counted = 0;
        log_message(log_warn, "Discarded suspend segment for '%s': complete session would exceed policy", track_name);
    }

    json_node *active = json_tree_get(root, "active");
    json_node *temperature = read_number("/sys/class/thermal/thermal_zone0/temp");

    set_int(active, "accumulated", accumulated);
    set_string(active, "state", "suspended");
    set_int(active, "start_time", 0);
    set_int(active, "start_uptime", 0);
    set_string(active, "start_boot_id", "");
    add_int(active, "suspends", 1);
    add_temperature(active, temperature);

    const int saved = save_record(root, record);
    if (saved) daily_add(counted, 0, json_tree_string_or(json_tree_get(active, "runtime"), ""));
    json_tree_free(temperature);
    json_tree_free(root);
    free(record);
    return saved;
}

static int resume_tracking(void) {
    char *record = record_path(track_file);
    active_info info;
    json_node *root = load_active_record(record, "resume", &info);
    if (!root) {
        free(record);
        return 1;
    }

    if (strcmp(info.state, "suspended") != 0) {
        log_message(
            log_warn, "Resume without a matching suspend for '%s'; discarded the ambiguous elapsed interval", track_name
        );
    }

    json_node *active = json_tree_get(root, "active");
    json_node *temperature = read_number("/sys/class/thermal/thermal_zone0/temp");

    set_int(active, "start_time", now_valid ? now : 0);
    set_int(active, "start_uptime", uptime_now);
    set_string(active, "start_boot_id", boot_id);
    set_string(active, "state", "active");
    add_temperature(active, temperature);

    const int saved = save_record(root, record);
    json_tree_free(temperature);
    json_tree_free(root);
    free(record);
    return saved;
}

static int checkpoint_tracking(void) {
    char *record = record_path(track_file);
    json_node *root = record ? load_record(record) : NULL;
    active_info info;

    long long counted = 0;
    const int found = root && read_active(root, &info) && strcmp(info.state, "active") == 0;
    const int safe = found && segment(&info, &counted);
    if (!found || (safe && counted < CHECKPOINT_MIN_SECONDS)) {
        json_tree_free(root);
        free(record);
        return 1;
    }

    if (!safe) {
        log_message(log_warn, "Discarded unsafe timing segment at checkpoint for '%s'", track_name);
    } else if (info.accumulated + counted > MAX_SESSION_SECONDS) {
        log_message(
            log_warn, "Discarded checkpoint segment for '%s': complete session would exceed policy", track_name
        );
        counted = 0;
    }

    json_node *active = json_tree_get(root, "active");
    json_node *temperature = read_number("/sys/class/thermal/thermal_zone0/temp");

    set_int(active, "accumulated", info.accumulated + counted);
    set_int(active, "start_time", now_valid ? now : 0);
    set_int(active, "start_uptime", uptime_now);
    set_string(active, "start_boot_id", boot_id);
    add_temperature(active, temperature);

    const int saved = save_record(root, record);
    if (saved) daily_add(counted, 0, json_tree_string_or(json_tree_get(active, "runtime"), ""));

    json_tree_free(temperature);
    json_tree_free(root);
    free(record);
    return saved;
}

static int stop_tracking(void) {
    char *record = record_path(track_file);
    active_info info;
    json_node *root = load_active_record(record, "stop", &info);

    if (root) {
        json_tree_free(root);

        const char *exit_code = getenv("TRACK_EXIT");
        const char *end = getenv("TRACK_END");
        if (!is_uint(exit_code)) exit_code = NULL;

        const char *ended;
        if (end && strcmp(end, "poweroff") == 0) {
            ended = "poweroff";
        } else if (!exit_code || strcmp(exit_code, "0") == 0) {
            ended = "normal";
        } else if (strcmp(exit_code, "64") == 0) {
            ended = "switch";
        } else {
            ended = "error";
        }

        finalise(record, ended, exit_code);
    }

    unlink(CURRENT_FILE);
    free(record);
    return 1;
}

static int seen_compare(const void *a, const void *b) {
    return strcmp(((const seen_time *) a)->path, ((const seen_time *) b)->path);
}

static void seen_add(seen_time **list, size_t *count, size_t *size, const char *path, const long long time) {
    if (!clock_in_range(time)) return;

    if (*count == *size) {
        const size_t grown_size = *size ? *size * 2 : 64;
        seen_time *grown = realloc(*list, grown_size * sizeof(**list));
        if (!grown) return;
        *list = grown;
        *size = grown_size;
    }

    char *copy = strdup(path);
    if (!copy) return;

    (*list)[*count].path = copy;
    (*list)[*count].time = time;
    (*count)++;
}

static size_t recovery_times(seen_time **list) {
    size_t count = 0;
    size_t size = 0;
    *list = NULL;

    DIR *dir = opendir(HISTORY_DIR);
    if (dir) {
        const struct dirent *entry;
        while ((entry = readdir(dir))) {
            const size_t length = strlen(entry->d_name);
            if (entry->d_name[0] == '.' || length < 5 || strcmp(entry->d_name + length - 4, ".cfg") != 0) continue;

            char *path = format("%s/%s", HISTORY_DIR, entry->d_name);
            struct stat info;
            char *content = path && stat(path, &info) == 0 && S_ISREG(info.st_mode) ? read_line(path, 1, 0) : NULL;
            if (content) seen_add(list, &count, &size, content, (long long) info.st_mtime);
            free(content);
            free(path);
        }
        closedir(dir);
    }

    FILE *file = fopen(WASABI_HISTORY, "r");
    if (file) {
        char *line = NULL;
        size_t line_size = 0;
        ssize_t length;
        while ((length = getline(&line, &line_size, file)) >= 0) {
            if (length > 0 && line[length - 1] == '\n') line[length - 1] = '\0';

            char *fields[5] = {NULL, NULL, NULL, NULL, NULL};
            char *cursor = line;
            for (int i = 0; i < 5 && cursor; i++) {
                fields[i] = cursor;
                cursor = strchr(cursor, '\t');
                if (cursor) *cursor++ = '\0';
            }
            if (cursor) {
                char *tab = strchr(fields[4], '\t');
                if (tab) *tab = '\0';
            }

            if (is_uint(fields[4])) seen_add(list, &count, &size, fields[0], atoll(fields[4]));
        }
        free(line);
        fclose(file);
    }

    if (count) qsort(*list, count, sizeof(**list), seen_compare);
    return count;
}

static long long recovery_lookup(const seen_time *list, const size_t count, const char *path) {
    const seen_time probe = {(char *) path, 0};
    const seen_time *found = count ? bsearch(&probe, list, count, sizeof(*list), seen_compare) : NULL;
    if (!found) return 0;

    while (found > list && strcmp((found - 1)->path, path) == 0)
        found--;

    long long latest = 0;
    for (; found < list + count && strcmp(found->path, path) == 0; found++) {
        if (found->time > latest) latest = found->time;
    }
    return latest;
}

static long long stamp(const json_node *node) {
    if (!node || node->type != JSON_NUMBER) return 0;
    const double value = strtod(node->text, NULL);
    return value > 0 ? (long long) floor(value) : 0;
}

static json_node *launch_counts(const json_node *source) {
    json_node *out = json_tree_object();
    if (!source || source->type != JSON_OBJECT) return out;

    for (const json_node *child = source->child; child; child = child->next) {
        json_node *entry = json_tree_object();
        json_tree_set(entry, "launches", json_tree_value_or(child, json_tree_int(0)));
        set_int(entry, "time", 0);
        json_tree_set(out, child->key, entry);
    }

    return out;
}

static void merge_counts(json_node *target, const json_node *source) {
    if (!source || source->type != JSON_OBJECT) return;

    for (const json_node *child = source->child; child; child = child->next) {
        json_node *entry = json_tree_member_object(target, child->key);
        add_int(entry, "launches", json_tree_int_or(json_tree_get(child, "launches"), 0));
        add_int(entry, "time", json_tree_int_or(json_tree_get(child, "time"), 0));
    }
}

static void merge_count_maps(json_node *target, const json_node *source) {
    static const char *maps[] = {"cores", "devices", "modes"};
    for (size_t i = 0; i < sizeof(maps) / sizeof(*maps); i++) {
        merge_counts(json_tree_member_object(target, maps[i]), json_tree_get(source, maps[i]));
    }
}

static long long earliest_positive(const long long a, const long long b) {
    if (a <= 0) return b > 0 ? b : 0;
    if (b <= 0) return a;
    return a < b ? a : b;
}

static json_node *legacy_record(
    const char *path, const json_node *value, const seen_time *seen, const size_t seen_count, long long *last_played
) {
    const long long times[] = {
        stamp(json_tree_get(value, "last_played")), stamp(json_tree_get(value, "start_time")),
        recovery_lookup(seen, seen_count, path)
    };

    long long first = 0;
    long long last = 0;
    for (size_t i = 0; i < 3; i++) {
        first = earliest_positive(first, times[i]);
        if (times[i] > last) last = times[i];
    }

    char *key = content_key(path);
    const char *slash = strrchr(path, '/');
    json_node *record = json_tree_object();

    set_int(record, "version", RECORD_VERSION);
    set_string(record, "key", key ? key : "");
    set_string(record, "path", path);
    json_tree_set(
        record, "name", json_tree_value_or(json_tree_get(value, "name"), json_tree_string(slash ? slash + 1 : path))
    );
    set_string(record, "system", "");
    json_tree_set(record, "launches", json_tree_value_or(json_tree_get(value, "launches"), json_tree_int(0)));
    json_tree_set(record, "total_time", json_tree_value_or(json_tree_get(value, "total_time"), json_tree_int(0)));
    set_int(record, "first_played", first);
    set_int(record, "last_played", last);
    json_tree_set(record, "last_session", json_tree_value_or(json_tree_get(value, "last_session"), json_tree_int(0)));
    json_tree_set(
        record, "longest_session", json_tree_value_or(json_tree_get(value, "last_session"), json_tree_int(0))
    );
    json_tree_set(record, "last_core", json_tree_value_or(json_tree_get(value, "last_core"), json_tree_string("")));
    json_tree_set(record, "last_device", json_tree_value_or(json_tree_get(value, "last_device"), json_tree_string("")));
    json_tree_set(record, "last_mode", json_tree_value_or(json_tree_get(value, "last_mode"), json_tree_string("")));
    json_tree_set(record, "cores", launch_counts(json_tree_get(value, "core_launches")));
    json_tree_set(record, "devices", launch_counts(json_tree_get(value, "device_launches")));
    json_tree_set(record, "modes", launch_counts(json_tree_get(value, "mode_launches")));
    json_tree_set(record, "endings", json_tree_object());
    json_tree_set(record, "sessions", json_tree_array());
    json_tree_set(record, "active", json_tree_null());
    json_tree_set(record, "migrated", json_tree_bool(1));

    free(key);
    *last_played = last;
    return record;
}

static int legacy_by_hash(const void *a, const void *b) {
    const legacy_entry *x = a;
    const legacy_entry *y = b;
    const int order = strcmp(x->hash, y->hash);
    if (order) return order;
    return x->order < y->order ? -1 : x->order > y->order;
}

static int legacy_by_played(const void *a, const void *b) {
    const legacy_entry *x = a;
    const legacy_entry *y = b;
    if (x->last_played != y->last_played) return x->last_played < y->last_played ? -1 : 1;
    return x->order < y->order ? -1 : x->order > y->order;
}

static json_node *legacy_group(legacy_entry *group, const size_t count) {
    qsort(group, count, sizeof(*group), legacy_by_played);

    json_node *merged = json_tree_clone(group[count - 1].record);
    set_int(merged, "launches", 0);
    set_int(merged, "total_time", 0);
    json_tree_set(merged, "cores", json_tree_object());
    json_tree_set(merged, "devices", json_tree_object());
    json_tree_set(merged, "modes", json_tree_object());

    long long first = 0;
    for (size_t i = 0; i < count; i++) {
        const json_node *item = group[i].record;
        const long long longest = json_tree_int_or(json_tree_get(merged, "longest_session"), 0);
        const long long item_longest = json_tree_int_or(json_tree_get(item, "longest_session"), 0);

        add_int(merged, "launches", json_tree_int_or(json_tree_get(item, "launches"), 0));
        add_int(merged, "total_time", json_tree_int_or(json_tree_get(item, "total_time"), 0));
        set_int(merged, "longest_session", longest > item_longest ? longest : item_longest);
        merge_count_maps(merged, item);
        first = earliest_positive(first, json_tree_int_or(json_tree_get(item, "first_played"), 0));
    }

    set_int(merged, "first_played", first);
    return merged;
}

static void merge_existing(json_node *existing, const json_node *old) {
    const long long longest = json_tree_int_or(json_tree_get(existing, "longest_session"), 0);
    const long long old_longest = json_tree_int_or(json_tree_get(old, "longest_session"), 0);
    const long long last = json_tree_int_or(json_tree_get(existing, "last_played"), 0);
    const long long old_last = json_tree_int_or(json_tree_get(old, "last_played"), 0);

    add_int(existing, "launches", json_tree_int_or(json_tree_get(old, "launches"), 0));
    add_int(existing, "total_time", json_tree_int_or(json_tree_get(old, "total_time"), 0));
    set_int(existing, "longest_session", longest > old_longest ? longest : old_longest);
    set_int(
        existing, "first_played",
        earliest_positive(
            json_tree_int_or(json_tree_get(existing, "first_played"), 0),
            json_tree_int_or(json_tree_get(old, "first_played"), 0)
        )
    );
    set_int(existing, "last_played", last > old_last ? last : old_last);
    merge_count_maps(existing, old);
    json_tree_set(existing, "migrated", json_tree_bool(1));
}

static int write_legacy_group(json_node *merged, const char *hash) {
    char *record = format("%s/%s.json", TRACK_DIR, hash);
    if (!record) return 0;

    int written;
    if (file_has_data(record)) {
        json_node *existing = json_tree_load(record);
        if (!existing || existing->type != JSON_OBJECT) {
            json_tree_free(existing);
            free(record);
            return 0;
        }

        merge_existing(existing, merged);
        written = json_tree_write(existing, record);
        json_tree_free(existing);
    } else {
        written = json_tree_write(merged, record);
    }

    free(record);
    return written;
}

static int is_blank_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;

    int c;
    while ((c = fgetc(file)) != EOF) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
    }

    fclose(file);
    return c == EOF;
}

static int migrate_legacy(void) {
    if (access(LEGACY_JSON, F_OK) != 0) return 1;

    const int blank = is_blank_file(LEGACY_JSON);
    json_node *legacy = blank ? NULL : json_tree_load(LEGACY_JSON);

    if (!blank && !legacy) {
        file_rename(LEGACY_JSON, LEGACY_JSON ".corrupt");
        log_message(log_error, "Invalid legacy playtime data preserved as '%s.corrupt' and not migrated", LEGACY_JSON);
        return 0;
    }

    if (legacy && legacy->type != JSON_OBJECT) {
        json_tree_free(legacy);
        log_message(log_error, "Legacy playtime migration failed; playtime_data.json was left in place");
        return 0;
    }

    seen_time *seen = NULL;
    const size_t seen_count = legacy ? recovery_times(&seen) : 0;

    size_t total = 0;
    for (const json_node *child = legacy ? legacy->child : NULL; child; child = child->next) {
        if (child->type == JSON_OBJECT) total++;
    }

    legacy_entry *entries = total ? calloc(total, sizeof(*entries)) : NULL;
    size_t count = 0;
    for (const json_node *child = legacy ? legacy->child : NULL; entries && child; child = child->next) {
        if (child->type != JSON_OBJECT) continue;

        legacy_entry *entry = &entries[count];
        snprintf(entry->hash, sizeof(entry->hash), "%08X", activity_key_hash(child->key));
        entry->order = count;
        entry->record = legacy_record(child->key, child, seen, seen_count, &entry->last_played);
        count++;
    }

    if (count) qsort(entries, count, sizeof(*entries), legacy_by_hash);

    int migrated = 0;
    for (size_t start = 0; start < count;) {
        size_t end = start + 1;
        while (end < count && strcmp(entries[end].hash, entries[start].hash) == 0)
            end++;

        json_node *merged = legacy_group(entries + start, end - start);
        if (merged && write_legacy_group(merged, entries[start].hash)) migrated++;
        json_tree_free(merged);
        start = end;
    }

    for (size_t i = 0; i < count; i++)
        json_tree_free(entries[i].record);
    for (size_t i = 0; i < seen_count; i++)
        free(seen[i].path);
    free(entries);
    free(seen);
    json_tree_free(legacy);

    if (total && !entries) {
        log_message(log_error, "Legacy playtime migration failed; playtime_data.json was left in place");
        return 0;
    }

    if (file_rename(LEGACY_JSON, LEGACY_JSON ".migrated") != 0) return 0;

    unlink(UNION_MARKER);
    log_message(log_info, "Migrated legacy playtime data into %d tracker records", migrated);
    return 1;
}

static int is_empty_text(const json_node *node) {
    return !json_tree_truthy(node) || (node->type == JSON_STRING && !node->text[0]);
}

static const char *assigned_runtime(const char *path, const char *core) {
    const char *rom = strstr(path, "/ROMS/");
    char *launch = NULL;

    if (rom && strchr(rom + 6, '/')) {
        rom += 6;
        const char *slash = strrchr(rom, '/');
        char *stem = strdup(slash + 1);
        char *dot = stem ? strrchr(stem, '.') : NULL;
        if (dot) *dot = '\0';

        char *config = stem ? format("%s/%.*s/%s.cfg", CONTENT_INFO_DIR, (int) (slash - rom), rom, stem) : NULL;
        launch = config ? read_line(config, 6, 0) : NULL;
        free(config);

        if (!launch || !*launch) {
            free(launch);
            config = format("%s/%.*s/core.cfg", CONTENT_INFO_DIR, (int) (slash - rom), rom);
            launch = config ? read_line(config, 5, 0) : NULL;
            free(config);
        }

        free(stem);
    }

    const char *runtime;
    if (strcmp(core, "ext-video") == 0) {
        runtime = "wasabi";
    } else if (launch && strncmp(launch, "mu-", 3) == 0) {
        runtime = "pickles";
    } else if (strncmp(core, "ext-", 4) == 0 || (launch && strncmp(launch, "ext-", 4) == 0)) {
        runtime = "external";
    } else {
        runtime = "retroarch";
    }

    free(launch);
    return runtime;
}

static int backfill_record(const char *record) {
    json_node *root = load_record(record);
    if (!root || !is_empty_text(json_tree_get(root, "last_runtime"))) {
        json_tree_free(root);
        return 0;
    }

    const char *runtime = assigned_runtime(
        json_tree_string_or(json_tree_get(root, "path"), ""), json_tree_string_or(json_tree_get(root, "last_core"), "")
    );

    set_string(root, "last_runtime", runtime);

    const json_node *runtimes = json_tree_get(root, "runtimes");
    if (!json_tree_truthy(runtimes) || (runtimes->type == JSON_OBJECT && !runtimes->child)) {
        json_node *map = json_tree_object();
        json_node *entry = json_tree_set(map, runtime, json_tree_object());
        json_tree_set(entry, "launches", json_tree_value_or(json_tree_get(root, "launches"), json_tree_int(0)));
        json_tree_set(entry, "time", json_tree_value_or(json_tree_get(root, "total_time"), json_tree_int(0)));
        json_tree_set(root, "runtimes", map);
    }

    json_node *sessions = json_tree_get(root, "sessions");
    if (!json_tree_truthy(sessions)) sessions = json_tree_set(root, "sessions", json_tree_array());
    if (sessions->type == JSON_ARRAY) {
        for (json_node *session = sessions->child; session; session = session->next) {
            if (session->type == JSON_OBJECT && is_empty_text(json_tree_get(session, "runtime"))) {
                set_string(session, "runtime", runtime);
            }
        }
    }

    json_node *active = json_tree_get(root, "active");
    if (json_tree_truthy(active) && is_empty_text(json_tree_get(active, "runtime"))) {
        set_string(active, "runtime", runtime);
    }

    const int saved = save_record(root, record);
    json_tree_free(root);
    return saved;
}

static void backfill_runtime(void) {
    if (access(RUNTIME_MARKER, F_OK) == 0) return;

    int count = 0;
    DIR *dir = opendir(TRACK_DIR);
    if (dir) {
        const struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (!is_record_name(entry->d_name)) continue;

            char *record = format("%s/%s", TRACK_DIR, entry->d_name);
            if (record && backfill_record(record)) count++;
            free(record);
        }
        closedir(dir);
    }

    FILE *marker = fopen(RUNTIME_MARKER, "w");
    if (marker) fclose(marker);
    log_message(log_info, "Recorded the runtime for %d earlier tracker records", count);
}

static const char *choose_runtime(void) {
    const char *assigned = getenv("TRACK_RUNTIME");

    if (strcmp(track_core, "ext-video") == 0) return "wasabi";
    if (assigned
        && (strcmp(assigned, "pickles") == 0 || strcmp(assigned, "retroarch") == 0 || strcmp(assigned, "external") == 0
            || strcmp(assigned, "app") == 0)) {
        return assigned;
    }
    if (strncmp(track_core, "ext-", 4) == 0) return "external";
    return "retroarch";
}

static int current_action(const char *action) {
    return strcmp(action, "suspend") == 0 || strcmp(action, "resume") == 0 || strcmp(action, "stop") == 0
           || strcmp(action, "checkpoint") == 0;
}

static int load_current_session(void) {
    char *name = read_line(CURRENT_FILE, 1, 0);
    char *core = read_line(CURRENT_FILE, 2, 0);
    char *file = read_line(CURRENT_FILE, 3, 0);

    if (!file || !*file) {
        free(name);
        free(core);
        free(file);
        return 0;
    }

    track_name = name ? name : "";
    track_core = core ? core : "";
    track_file = file;
    return 1;
}

static int valid_action(const char *action) {
    static const char *actions[] = {"start", "suspend", "resume", "stop", "checkpoint", "migrate"};
    for (size_t i = 0; i < sizeof(actions) / sizeof(*actions); i++) {
        if (strcmp(action, actions[i]) == 0) return 1;
    }
    return 0;
}

int main(const int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "migrate") == 0) {
        track_action = "migrate";
    } else if (argc == 2 && current_action(argv[1])) {
        track_action = argv[1];
        if (!load_current_session()) return 0;
    } else {
        if (argc > 1) track_name = argv[1];
        if (argc > 2) track_core = argv[2];
        if (argc > 3) track_file = argv[3];
        if (argc > 4) track_action = argv[4];
    }

    const int migrate = strcmp(track_action, "migrate") == 0;
    if (!migrate) {
        char *enabled = get_var(CONF_CONFIG_PATH, "settings/advanced/activity");
        const int off = strcmp(enabled, "0") == 0;
        free(enabled);
        if (off) return 0;
    }

#ifdef TRACK_LOG_FILE
    track_log = strdup(TRACK_LOG_FILE);
#else
    char *rom_mount = get_var(CONF_DEVICE_PATH, "storage/rom/mount");
    track_log = format("%s/MUOS/log/playtime_error.log", rom_mount);
    free(rom_mount);
#endif
    if (!track_log) return 1;

    if (!valid_action(track_action)) {
        fprintf(
            stderr,
            "Usage: %s <name> <core> <file> <start|suspend|resume|stop|checkpoint>\n"
            "       %s <suspend|resume|stop|checkpoint>\n"
            "       %s migrate\n",
            argv[0], argv[0], argv[0]
        );
        return 1;
    }

    if (!migrate && !*track_file) {
        fprintf(stderr, "Error: file path must not be empty\n");
        return 1;
    }

    char *device_mode = get_var(CONF_CONFIG_PATH, "boot/device_mode");
    track_mode = strcmp(device_mode, "1") == 0 ? "console" : "handheld";
    free(device_mode);

    track_runtime = choose_runtime();
    track_board = get_var(CONF_DEVICE_PATH, "board/name");

    const time_t clock = time(NULL);
    now = clock > 0 ? (long long) clock : 0;
    uptime_now = read_uptime();

    boot_id = read_line("/proc/sys/kernel/random/boot_id", 1, 0);
    if (!boot_id || !*boot_id) {
        free(boot_id);
        boot_id = strdup("unknown");
    }
    session_id = format("%s-%ld-%lld", boot_id, (long) getpid(), uptime_now);
    if (!track_board || !boot_id || !session_id) return 1;

    now_valid = clock_is_sane();
    if (!now_valid) log_message(log_warn, "System clock is unsafe (epoch %lld); this call will not record dates", now);

    if (!mkdir_parents(TRACK_DIR)) {
        log_message(log_error, "Unable to create activity tracker directory");
        return 1;
    }

    if (!acquire_lock()) return 1;
    if (!migrate_legacy()) log_message(log_warn, "Legacy playtime migration was incomplete");
    backfill_runtime();
    daily_backfill();

    if (migrate) {
        if (now_valid) update_highwater();
        return 0;
    }

    if (strcmp(track_action, "checkpoint") == 0) return checkpoint_tracking() ? 0 : 1;

    if (now_valid) update_highwater();
    log_message(log_info, "Activity tracker '%s' for '%s' (core: %s)", track_action, track_name, track_core);

    int done = 1;
    if (strcmp(track_action, "start") == 0) {
        done = start_tracking();
    } else if (strcmp(track_action, "suspend") == 0) {
        done = suspend_tracking();
    } else if (strcmp(track_action, "resume") == 0) {
        done = resume_tracking();
    } else {
        done = stop_tracking();
    }

    return done ? 0 : 1;
}
