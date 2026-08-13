#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "options.h"

#define LOG_DIRECTORY    "/opt/muos/log"
#define LOG_MODULE_SIZE  20U
#define LOG_DATE_SIZE    16U
#define LOG_TIME_SIZE    20U
#define LOG_MESSAGE_SIZE 768U
#define LOG_LINE_SIZE    1024U
#define LOG_PATH_SIZE    256U

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static int log_file = -1;
static char log_file_path[LOG_PATH_SIZE];

static time_t formatted_second = (time_t) -1;
static char formatted_time[LOG_TIME_SIZE];
static char formatted_date[LOG_DATE_SIZE];

static const char *level_colour_symbol(const log_level level) {
    switch (level) {
        case log_level_warn:
            return WARN_SYMBOL;
        case log_level_error:
            return ERROR_SYMBOL;
        case log_level_success:
            return SUCCESS_SYMBOL;
        case log_level_debug:
            return DEBUG_SYMBOL;
        case log_level_info:
        default:
            return INFO_SYMBOL;
    }
}

static double uptime_seconds(void) {
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    return (double) now.tv_sec + (double) now.tv_nsec / 1000000000.0;
}

static void refresh_wall_time(void) {
    const time_t now = time(NULL);
    struct tm local;

    if (now == formatted_second) return;
    formatted_second = now;

    if (localtime_r(&now, &local)) {
        if (strftime(formatted_time, LOG_TIME_SIZE, "%Y-%m-%d %H:%M:%S", &local) == 0U)
            (void) snprintf(formatted_time, LOG_TIME_SIZE, "0000-00-00 00:00:00");
        if (strftime(formatted_date, LOG_DATE_SIZE, "%Y_%m_%d", &local) == 0U)
            (void) snprintf(formatted_date, LOG_DATE_SIZE, "0000_00_00");
        return;
    }

    (void) snprintf(formatted_time, LOG_TIME_SIZE, "0000-00-00 00:00:00");
    (void) snprintf(formatted_date, LOG_DATE_SIZE, "0000_00_00");
}

static void copy_module(char output[LOG_MODULE_SIZE], const char *module) {
    const char *source = module;

    if (!source || !*source) source = "unknown";
    (void) snprintf(output, LOG_MODULE_SIZE, "%.19s", source);
}

static size_t bounded_length(const int result) {
    if (result <= 0) return 0U;
    if ((size_t) result >= MAX_BUFFER_SIZE) return MAX_BUFFER_SIZE - 1U;
    return (size_t) result;
}

static void write_all(const int descriptor, const char *buffer, const size_t length) {
    size_t offset = 0U;

    while (offset < length) {
        const ssize_t written = write(descriptor, buffer + offset, length - offset);

        if (written > 0) {
            offset += (size_t) written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        break;
    }
}

static void
write_log_file(const char *date, const char *time_text, const double elapsed, const char *module, const char *message) {
    char path[LOG_PATH_SIZE];
    char line[LOG_LINE_SIZE];

    int result = snprintf(path, sizeof(path), "%s/%s_%s.log", LOG_DIRECTORY, date, module);
    if (result < 0 || (size_t) result >= sizeof(path)) return;

    if (log_file < 0 || strcmp(path, log_file_path) != 0) {
        if (log_file >= 0) (void) close(log_file);

        log_file = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, (mode_t) 0644);
        if (log_file < 0) {
            log_file_path[0] = '\0';
            return;
        }

        (void) snprintf(log_file_path, sizeof(log_file_path), "%s", path);
    }

    result = snprintf(line, sizeof(line), "[%.2f]\t[%s] [%s]\t%s\n", elapsed, time_text, module, message);
    const size_t length = bounded_length(result);

    if (length > 0U) write_all(log_file, line, length);
}

static void
log_write_va(const int debug_mode, const log_level level, const char *module, const char *format, va_list arguments) {
    char module_text[LOG_MODULE_SIZE];
    char message[LOG_MESSAGE_SIZE];
    char term_line[LOG_LINE_SIZE];

    if (debug_mode < 1 || !format) return;
    if (level == log_level_debug && debug_mode < 2) return;

    int result = vsnprintf(message, sizeof(message), format, arguments);
    if (result < 0) return;

    copy_module(module_text, module);
    const char *symbol = level_colour_symbol(level);
    const double elapsed = uptime_seconds();

    (void) pthread_mutex_lock(&log_lock);

    refresh_wall_time();

    result = snprintf(
        term_line, sizeof(term_line), "[%.2f]\t[%s] [%s] [%s]\t%s\n", elapsed, formatted_time, symbol, module_text,
        message
    );
    const size_t term_length = bounded_length(result);
    if (term_length > 0U) write_all(STDERR_FILENO, term_line, term_length);

    write_log_file(formatted_date, formatted_time, elapsed, module_text, message);

    (void) pthread_mutex_unlock(&log_lock);
}

void log_write_enabled(const int debug_mode, const log_level level, const char *module, const char *format, ...) {
    va_list arguments;

    va_start(arguments, format);
    log_write_va(debug_mode, level, module, format, arguments);
    va_end(arguments);
}

void log_write(const log_level level, const char *module, const char *format, ...) {
    const int debug_mode = is_debug_mode();
    va_list arguments;

    if (debug_mode < 1 || !format) return;
    if (level == log_level_debug && debug_mode < 2) return;

    va_start(arguments, format);
    log_write_va(debug_mode, level, module, format, arguments);
    va_end(arguments);
}
