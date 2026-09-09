#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "safe_ini.h"

mini_t *safe_ini_load(const char *path, const size_t maximum_size) {
    if (!path || !*path || maximum_size == 0) return NULL;

    struct stat status;
    if (lstat(path, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0
        || (unsigned long long) status.st_size > maximum_size)
        return NULL;

    return mini_load(path);
}

enum safe_ini_value_status safe_ini_get_int(mini_t *ini, const char *group, const char *key, long long *value) {
    if (!ini || !key || !value || mini_value_exists(ini, group, key) != MINI_OK) return safe_ini_value_missing;

    const char *text = mini_get_string(ini, group, key, NULL);
    if (!text) return safe_ini_value_invalid;
    while (*text && isspace((unsigned char) *text))
        text++;
    if (!*text) return safe_ini_value_invalid;

    errno = 0;
    char *end = NULL;
    const long long parsed = strtoll(text, &end, 10);
    if (errno == ERANGE || end == text) return safe_ini_value_invalid;
    while (*end && isspace((unsigned char) *end))
        end++;
    if (*end) return safe_ini_value_invalid;

    *value = parsed;
    return safe_ini_value_valid;
}

int safe_ini_get_int_range(
    mini_t *ini, const char *group, const char *key, const long long minimum, const long long maximum,
    const int fallback, int *value
) {
    if (!value) return 0;

    long long parsed = 0;
    const enum safe_ini_value_status status = safe_ini_get_int(ini, group, key, &parsed);
    if (status == safe_ini_value_missing) return 0;

    *value = status == safe_ini_value_valid && parsed >= minimum && parsed <= maximum ? (int) parsed : fallback;
    return 1;
}
