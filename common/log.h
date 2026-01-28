#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define COL_RED    "\x1b[38;5;196m"
#define COL_GREEN  "\x1b[38;5;46m"
#define COL_BLUE   "\x1b[38;5;33m"
#define COL_YELLOW "\x1b[38;5;226m"
#define COL_ORANGE "\x1b[38;5;202m"
#define COL_RESET  "\x1b[0m"

#define INFO_SYMBOL    COL_BLUE   "*" COL_RESET
#define WARN_SYMBOL    COL_YELLOW "!" COL_RESET
#define ERROR_SYMBOL   COL_RED    "-" COL_RESET
#define SUCCESS_SYMBOL COL_GREEN  "+" COL_RESET
#define DEBUG_SYMBOL   COL_ORANGE "?" COL_RESET

#define LOG(level, symbol, mux_module, msg, ...) {         \
    struct timespec ts;                                    \
    clock_gettime(CLOCK_MONOTONIC, &ts);                   \
    double uptime = ts.tv_sec + ts.tv_nsec / 1000000000.0; \
                                                           \
    time_t now = time(NULL);                               \
    struct tm *timeinfo = localtime(&now);                 \
    char time_buffer[20];                                  \
    strftime(time_buffer, sizeof(time_buffer),             \
             "%Y-%m-%d %H:%M:%S", timeinfo);               \
                                                           \
    char truncated_module[20];                             \
    snprintf(truncated_module, sizeof(truncated_module),   \
             "%.19s", mux_module);                         \
                                                           \
    fprintf(stderr, "[%.2f]\t[%s] [%s] [%s]\t" msg "\n",   \
        uptime, time_buffer, symbol, truncated_module,     \
        ##__VA_ARGS__);                                    \
}

#define LOG_INFO(mux_module, msg, ...)    do { LOG(INFO,    INFO_SYMBOL,    mux_module, msg, ##__VA_ARGS__); } while (0)
#define LOG_WARN(mux_module, msg, ...)    do { LOG(WARN,    WARN_SYMBOL,    mux_module, msg, ##__VA_ARGS__); } while (0)
#define LOG_ERROR(mux_module, msg, ...)   do { LOG(ERROR,   ERROR_SYMBOL,   mux_module, msg, ##__VA_ARGS__); } while (0)
#define LOG_SUCCESS(mux_module, msg, ...) do { LOG(SUCCESS, SUCCESS_SYMBOL, mux_module, msg, ##__VA_ARGS__); } while (0)
#define LOG_DEBUG(mux_module, msg, ...)   do { LOG(DEBUG,   DEBUG_SYMBOL,   mux_module, msg, ##__VA_ARGS__); } while (0)
