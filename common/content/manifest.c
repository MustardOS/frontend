#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <common/content/manifest.h>

#define SHA256_TEXT_LENGTH 64

int manifest_json_string(const struct json object, const char *key, char *output, const size_t size) {
    if (!key || !output || !size) return 0;
    const struct json value = json_object_get(object, key);
    if (json_type(value) != JSON_STRING) return 0;
    const size_t length = json_string_copy(value, output, size);
    return length > 0 && length < size;
}

int manifest_https_url(const char *url) {
    return url && strncasecmp(url, "https://", 8) == 0 && url[8];
}

int manifest_label_valid(const char *text) {
    if (!text || !text[0] || isspace((unsigned char) text[0])) return 0;
    size_t length = 0;
    for (const unsigned char *cursor = (const unsigned char *) text; *cursor; cursor++, length++)
        if (*cursor < 0x20 || *cursor == 0x7f) return 0;
    return length && !isspace((unsigned char) text[length - 1]);
}

int manifest_safe_stem(const char *text, char *output, const size_t size) {
    if (!text || !output || !size) return 0;
    size_t used = 0;
    int separated = 0;
    for (const unsigned char *cursor = (const unsigned char *) text; *cursor && used + 1 < size; cursor++) {
        if (isalnum(*cursor) || *cursor == '-' || *cursor == '_') {
            output[used++] = (char) *cursor;
            separated = 0;
        } else if (isspace(*cursor) || *cursor >= 0x80) {
            if (used && !separated) output[used++] = ' ';
            separated = 1;
        }
    }
    while (used && output[used - 1] == ' ')
        used--;
    output[used] = '\0';
    return used > 0;
}

int manifest_url_stem(const char *url, char *output, const size_t size) {
    if (!url || !output || !size) return 0;
    const char *base = strrchr(url, '/');
    base = base ? base + 1 : url;
    char name[128];
    snprintf(name, sizeof(name), "%s", base);
    char *extension = strrchr(name, '.');
    if (extension) *extension = '\0';
    return manifest_safe_stem(name, output, size);
}

int manifest_sha256_valid(const char *text) {
    if (!text || strlen(text) != SHA256_TEXT_LENGTH) return 0;
    for (size_t index = 0; index < SHA256_TEXT_LENGTH; index++)
        if (!isxdigit((unsigned char) text[index])) return 0;
    return 1;
}
