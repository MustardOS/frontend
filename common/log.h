#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#define RED    "\x1b[38;5;196m"
#define GREEN  "\x1b[38;5;46m"
#define BLUE   "\x1b[38;5;33m"
#define YELLOW "\x1b[38;5;226m"
#define ORANGE "\x1b[38;5;202m"
#define RESET  "\x1b[0m"

#define INFO_SYMBOL    BLUE   "*" RESET
#define WARN_SYMBOL    YELLOW "!" RESET
#define ERROR_SYMBOL   RED    "-" RESET
#define SUCCESS_SYMBOL GREEN  "+" RESET
#define DEBUG_SYMBOL   ORANGE "?" RESET

#define LOG(level, symbol, color, mux_prog, msg, ...) \
    fprintf(stderr, "[" color symbol RESET "] [%s] " msg, mux_prog, ##__VA_ARGS__)

#define LOG_INFO(mux_prog, msg, ...) \
    LOG(INFO, INFO_SYMBOL, BLUE, mux_prog, msg, ##__VA_ARGS__)

#define LOG_WARN(mux_prog, msg, ...) \
    LOG(WARN, WARN_SYMBOL, YELLOW, mux_prog, msg, ##__VA_ARGS__)

#define LOG_ERROR(mux_prog, msg, ...) \
    LOG(ERROR, ERROR_SYMBOL, RED, mux_prog, msg, ##__VA_ARGS__)

#define LOG_SUCCESS(mux_prog, msg, ...) \
    LOG(SUCCESS, SUCCESS_SYMBOL, GREEN, mux_prog, msg, ##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(mux_prog, msg, ...) \
    LOG(DEBUG, DEBUG_SYMBOL, ORANGE, mux_prog, msg, ##__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif
