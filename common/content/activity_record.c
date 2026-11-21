#include <ctype.h>
#include <string.h>
#include <common/content/activity_record.h>

static const char *activity_key_start(const char *content_path) {
    const char *key = strstr(content_path, "/ROMS/");
    return key ? key + 6 : content_path;
}

void activity_content_key(const char *content_path, char *key, const size_t key_size) {
    if (!key_size) return;

    size_t length = 0;
    for (const char *p = activity_key_start(content_path); *p && length + 1 < key_size; p++) {
        key[length++] = (char) tolower((unsigned char) *p);
    }

    key[length] = '\0';
}

uint32_t activity_key_hash(const char *content_path) {
    uint32_t hash = 2166136261U;

    for (const char *p = activity_key_start(content_path); *p; p++) {
        hash ^= (uint8_t) tolower((unsigned char) *p);
        hash *= 16777619;
    }

    return hash;
}
