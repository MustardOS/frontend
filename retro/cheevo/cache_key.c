#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cache_key.h"

static int form_value(const char *form, const char *key, char *value, const size_t value_size) {
    if (!form) return 0;
    const size_t key_length = strlen(key);
    const char *field = form;
    while (*field) {
        const char *end = strchr(field, '&');
        if (!end) end = field + strlen(field);
        if ((size_t) (end - field) > key_length && memcmp(field, key, key_length) == 0 && field[key_length] == '=') {
            const size_t length = (size_t) (end - field) - key_length - 1;
            if (!length || length >= value_size) return 0;
            memcpy(value, field + key_length + 1, length);
            value[length] = '\0';
            return 1;
        }
        field = *end ? end + 1 : end;
    }
    return 0;
}

static int decimal_value(const char *value) {
    if (!value[0]) return 0;
    for (size_t index = 0; value[index]; index++)
        if (value[index] < '0' || value[index] > '9') return 0;
    return 1;
}

int cheevo_cache_name_valid(const char *name) {
    if (!name || strncmp(name, "data-", 5) != 0) return 0;
    const size_t length = strlen(name);
    if (length < 11 || length >= 96 || strcmp(name + length - 5, ".json") != 0) return 0;
    for (size_t index = 5; index < length - 5; index++) {
        const char value = name[index];
        if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')
              || value == '-'))
            return 0;
    }
    return 1;
}

static int account_tag(const char *post, char *tag, const size_t tag_size) {
    char account[128];
    if (!form_value(post, "u", account, sizeof(account))) return 0;

    uint64_t hash = 1469598103934665603ULL;
    for (const char *cursor = account; *cursor; cursor++) {
        hash ^= (unsigned char) *cursor;
        hash *= 1099511628211ULL;
    }

    const int written = snprintf(tag, tag_size, "%016llx", (unsigned long long) hash);
    return written > 0 && (size_t) written < tag_size;
}

int cheevo_cache_request_name(const char *post, char *name, const size_t name_size) {
    char request_type[32];
    if (!form_value(post, "r", request_type, sizeof(request_type))) return 0;

    if (strcmp(request_type, "login2") == 0) {
        char tag[24];
        if (!account_tag(post, tag, sizeof(tag))) return 0;
        const int written = snprintf(name, name_size, "data-signin-%s.json", tag);
        return written > 0 && (size_t) written < name_size && cheevo_cache_name_valid(name) ? 2 : 0;
    }

    if (strcmp(request_type, "startsession") == 0) {
        char tag[24];
        char game[16];
        if (!account_tag(post, tag, sizeof(tag)) || !form_value(post, "g", game, sizeof(game)) || !decimal_value(game))
            return 0;
        const int written = snprintf(name, name_size, "data-session-%s-%s.json", tag, game);
        return written > 0 && (size_t) written < name_size && cheevo_cache_name_valid(name) ? 2 : 0;
    }

    if (strcmp(request_type, "lbinfo") == 0) {
        char identity[16];
        char count[8];
        char offset[16] = "0";
        char ignored[256];
        if (!form_value(post, "i", identity, sizeof(identity)) || !decimal_value(identity)
            || !form_value(post, "c", count, sizeof(count)) || !decimal_value(count)
            || (form_value(post, "o", offset, sizeof(offset)) && !decimal_value(offset))
            || form_value(post, "u", ignored, sizeof(ignored)))
            return 0;
        const int written = snprintf(name, name_size, "data-leaderboard-%s-%s-%s.json", identity, offset, count);
        return written > 0 && (size_t) written < name_size && cheevo_cache_name_valid(name) ? 2 : 0;
    }

    if (strcmp(request_type, "achievementsets") != 0) return 0;

    char identity[65];
    const char *prefix;
    if (form_value(post, "m", identity, sizeof(identity))) {
        prefix = "hash";
    } else if (form_value(post, "g", identity, sizeof(identity))) {
        prefix = "game";
    } else {
        return 0;
    }
    for (size_t index = 0; identity[index]; index++) {
        const char value = identity[index];
        if (!((value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F') || (value >= '0' && value <= '9')))
            return 0;
    }

    const int written = snprintf(name, name_size, "data-%s-%s.json", prefix, identity);
    return written > 0 && (size_t) written < name_size && cheevo_cache_name_valid(name) ? 2 : 0;
}
