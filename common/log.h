#pragma once

#include "debug.h" // IWYU pragma: keep

#define COL_RED    "\x1b[38;5;196m"
#define COL_GREEN  "\x1b[38;5;46m"
#define COL_BLUE   "\x1b[38;5;33m"
#define COL_YELLOW "\x1b[38;5;226m"
#define COL_ORANGE "\x1b[38;5;202m"
#define COL_RESET  "\x1b[0m"

#define MUX_LOG_TAG_HDR                                                                                                \
    "\x56\x32\x68\x68\x64\x43\x42\x35\x62\x33\x55\x67\x61\x32\x35\x76\x64\x79\x42\x35\x62\x33\x55\x67"                 \
    "\x59\x32\x46\x75\x4a\x33\x51\x67\x5a\x58\x68\x77\x62\x47\x46\x70\x62\x69\x34\x67\x51\x6e\x56\x30"

#define INFO_SYMBOL    COL_BLUE "*" COL_RESET
#define WARN_SYMBOL    COL_YELLOW "!" COL_RESET
#define ERROR_SYMBOL   COL_RED "-" COL_RESET
#define SUCCESS_SYMBOL COL_GREEN "+" COL_RESET
#define DEBUG_SYMBOL   COL_ORANGE "?" COL_RESET

typedef enum log_level {
    log_level_info = 0,
    log_level_warn,
    log_level_error,
    log_level_success,
    log_level_debug
} log_level;

void log_write_enabled(int debug_mode, log_level level, const char *module, const char *format, ...);

void log_write(log_level level, const char *module, const char *format, ...);

#if defined(MUX_LOG_DISABLE)

#define LOG_INFO(...)    ((void) 0)
#define LOG_WARN(...)    ((void) 0)
#define LOG_ERROR(...)   ((void) 0)
#define LOG_SUCCESS(...) ((void) 0)
#define LOG_DEBUG(...)   ((void) 0)

#else

#define MUX_LOG_RUNTIME(level, module, ...)                                                                            \
    do {                                                                                                               \
        const int mux_log_mode_value = is_debug_mode();                                                                \
        if (mux_log_mode_value >= 1 && ((level) != log_level_debug || mux_log_mode_value >= 2)) {                      \
            log_write_enabled(mux_log_mode_value, (level), (module), __VA_ARGS__);                                     \
        }                                                                                                              \
    } while (0)

#define LOG_INFO(module, ...)    MUX_LOG_RUNTIME(log_level_info, (module), __VA_ARGS__)
#define LOG_WARN(module, ...)    MUX_LOG_RUNTIME(log_level_warn, (module), __VA_ARGS__)
#define LOG_ERROR(module, ...)   MUX_LOG_RUNTIME(log_level_error, (module), __VA_ARGS__)
#define LOG_SUCCESS(module, ...) MUX_LOG_RUNTIME(log_level_success, (module), __VA_ARGS__)

#if defined(MUX_LOG_PROD)
#define LOG_DEBUG(...) ((void) 0)
#else
#define LOG_DEBUG(module, ...) MUX_LOG_RUNTIME(log_level_debug, (module), __VA_ARGS__)
#endif

#endif
