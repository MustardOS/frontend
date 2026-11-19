#include <stdlib.h>
#include <string.h>
#include <json/json.h>
#include <common/base/message.h>
#include <common/storage/fileio.h>

static char *message_copy(const struct json value) {
    if (json_type(value) != JSON_STRING) return NULL;

    const size_t size = json_string_length(value) + 1;
    char *text = malloc(size);
    if (text) json_string_copy(value, text, size);

    return text;
}

int message_load(const char *path, message_set *set) {
    memset(set, 0, sizeof(*set));

    char *data = file_exist(path) ? read_all_char_from(path) : NULL;
    if (!data || !json_valid(data)) {
        free(data);
        return 0;
    }

    const struct json root = json_parse(data);
    const struct json list = json_object_get(root, "messages");
    const size_t total = json_array_count(list);

    set->title = message_copy(json_object_get(root, "title"));
    set->lines = total ? calloc(total, sizeof(char *)) : NULL;

    for (size_t i = 0; set->lines && i < total; i++) {
        char *line = message_copy(json_array_get(list, i));
        if (line && *line) {
            set->lines[set->count++] = line;
        } else {
            free(line);
        }
    }

    free(data);

    if (set->count) return 1;

    message_free(set);
    return 0;
}

int message_index(const message_set *set, const int index, const char *exclude) {
    if (set->count <= 0) return -1;

    int pick = (index < 0 ? -index : index) % set->count;
    if (exclude && set->count > 1 && strcmp(set->lines[pick], exclude) == 0) pick = (pick + 1) % set->count;

    return pick;
}

void message_free(message_set *set) {
    for (int i = 0; i < set->count; i++)
        free(set->lines[i]);

    free(set->lines);
    free(set->title);
    memset(set, 0, sizeof(*set));
}
