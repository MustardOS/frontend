#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "strutil.h"

int str_copy_checked(char *destination, const size_t capacity, const char *source) {
    if (!destination || capacity == 0 || !source) return 0;
    const size_t length = strlen(source);
    if (length >= capacity) {
        destination[0] = '\0';
        return 0;
    }
    memcpy(destination, source, length + 1);
    return 1;
}

int str_format_checked(char *destination, const size_t capacity, const char *format, ...) {
    if (!destination || capacity == 0 || !format) return 0;
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(destination, capacity, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t) length >= capacity) {
        destination[0] = '\0';
        return 0;
    }
    return 1;
}

int path_join_checked(char *destination, const size_t capacity, const char *const *parts, const size_t count) {
    if (!destination || capacity == 0 || !parts || count == 0) return 0;
    destination[0] = '\0';
    size_t used = 0;

    for (size_t index = 0; index < count; index++) {
        const char *part = parts[index];
        if (!part || !*part) continue;

        while (used > 0 && destination[used - 1] == '/' && *part == '/')
            part++;

        const int separator = used > 0 && destination[used - 1] != '/' && *part != '/';
        const size_t length = strlen(part);
        if ((size_t) separator > capacity - used - 1 || length > capacity - used - 1 - (size_t) separator) {
            destination[0] = '\0';
            return 0;
        }

        if (separator) destination[used++] = '/';
        memcpy(destination + used, part, length);
        used += length;
        destination[used] = '\0';
    }

    return used > 0;
}

int str_startswith(const char *a, const char *b) {
    if (strncmp(a, b, strlen(b)) == 0) return 1;

    return 0;
}
