#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fileio.h"
#include "init.h"
#include "log.h"
#include "options.h"

static int lite_atoi(const char *text, const int fallback) {
    if (!*text) return fallback;

    char *end = NULL;
    errno = 0;
    const long value = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX) return fallback;

    return (int) value;
}

int file_exist(const char *filename) {
    return access(filename, F_OK) == 0;
}

int dir_exist(const char *dirname) {
    struct stat stats;
    return stat(dirname, &stats) == 0 && S_ISDIR(stats.st_mode);
}

char *read_line_char_from(const char *filename, const size_t line_number) {
    if (!filename || line_number == 0) {
        LOG_ERROR(mux_module, "Invalid filename or line number...");
        return strdup("");
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG_ERROR(mux_module, "Failed to open file: %s", filename);
        return strdup("");
    }

    char *line = malloc(MAX_BUFFER_SIZE);
    if (!line) {
        LOG_ERROR(mux_module, "Failed to allocate memory");
        fclose(file);
        return strdup("");
    }

    size_t current_line = 0;
    while (fgets(line, MAX_BUFFER_SIZE, file) != NULL) {
        current_line++;
        if (current_line == line_number) {
            const size_t length = strlen(line);

            if (length > 0 && line[length - 1] == '\n') line[length - 1] = '\0';

            fclose(file);
            return line;
        }
    }

    free(line);
    fclose(file);

    return strdup("");
}

int read_line_int_from(const char *filename, const size_t line_number) {
    char line[MAX_BUFFER_SIZE];
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    for (size_t i = 1; i <= line_number && fgets(line, sizeof(line), file); i++) {
        if (i == line_number) {
            line[strcspn(line, "\n")] = '\0';
            fclose(file);
            return lite_atoi(line, 0);
        }
    }

    fclose(file);
    return 0;
}
