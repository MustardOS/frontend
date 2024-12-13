#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "json/json.h"
#include "wireplumber.h"

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

static char *execute_pw_dump(void);
static int parse_json_to_sinks(const char *json_output, Sink **sinks, int *count);
static Sink create_sink_from_json(struct json item);

int get_sinks(Sink **sinks, int *count) {
    printf("Executing pw-dump command to retrieve sinks...\n");

    FILE *fp = popen("pw-dump", "r");
    if (!fp) {
        perror("Failed to run pw-dump");
        return -1;
    }

    char *json_output = NULL;
    size_t json_size = 0;
    char buffer[512];

    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t line_length = strlen(buffer);
        char *new_output = realloc(json_output, json_size + line_length + 1);
        if (!new_output) {
            perror("Failed to allocate memory for JSON buffer");
            free(json_output);
            pclose(fp);
            return -1;
        }
        json_output = new_output;
        memcpy(json_output + json_size, buffer, line_length);
        json_size += line_length;
        json_output[json_size] = '\0';
    }
    pclose(fp);

    if (!json_output) {
        fprintf(stderr, "Failed to retrieve JSON output from pw-dump\n");
        return -1;
    }

    int result = parse_json_to_sinks(json_output, sinks, count);
    free(json_output);

    return result;
}

static char *execute_pw_dump(void) {
    FILE *fp = popen("pw-dump", "r");
    if (!fp) {
        perror("Failed to run pw-dump");
        return NULL;
    }

    size_t buffer_size = 4096;
    char *json_output = malloc(buffer_size);
    if (!json_output) {
        perror("Failed to allocate memory for JSON buffer");
        pclose(fp);
        return NULL;
    }

    size_t json_size = 0;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        size_t line_length = strlen(line);
        if (json_size + line_length + 1 > buffer_size) {
            buffer_size *= 2;
            char *new_output = realloc(json_output, buffer_size);
            if (!new_output) {
                perror("Failed to reallocate memory for JSON buffer");
                free(json_output);
                pclose(fp);
                return NULL;
            }
            json_output = new_output;
        }
        memcpy(json_output + json_size, line, line_length);
        json_size += line_length;
        json_output[json_size] = '\0';
    }
    pclose(fp);

    DEBUG_PRINT("JSON output size: %zu bytes\n", json_size);
    return json_output;
}

static int parse_json_to_sinks(const char *json_output, Sink **sinks, int *count) {
    struct json root = json_parse(json_output);
    if (!json_exists(root) || json_type(root) != JSON_ARRAY) {
        fprintf(stderr, "Failed to parse JSON or JSON is not an array\n");
        return -1;
    }

    *sinks = NULL;
    *count = 0;

    size_t total_items = json_array_count(root);
    for (size_t i = 0; i < total_items; i++) {
        struct json item = json_array_get(root, i);
        if (json_type(item) != JSON_OBJECT) {
            continue;
        }

        struct json info = json_object_get(item, "info");
        if (!json_exists(info) || json_type(info) != JSON_OBJECT) {
            continue;
        }

        struct json props = json_object_get(info, "props");
        if (!json_exists(props) || json_type(props) != JSON_OBJECT) {
            continue;
        }

        struct json media_class = json_object_get(props, "media.class");
        if (!json_exists(media_class) || json_type(media_class) != JSON_STRING ||
            json_string_compare(media_class, "Audio/Sink") != 0) {
            continue;
        }

        struct json description = json_object_get(props, "node.description");
        struct json nick = json_object_get(props, "node.nick");
        struct json name = json_object_get(props, "node.name");
        struct json object_id = json_object_get(item, "id");

        if (!json_exists(description) || !json_exists(object_id) || json_type(object_id) != JSON_NUMBER) {
            continue;
        }

        Sink sink = {0};
        sink.id = json_int(object_id);
        json_string_copy(description, sink.description, sizeof(sink.description));
        if (json_exists(nick)) {
            json_string_copy(nick, sink.nick, sizeof(sink.nick));
        } else {
            strncpy(sink.nick, "Unknown", sizeof(sink.nick));
        }
        if (json_exists(name)) {
            json_string_copy(name, sink.name, sizeof(sink.name));
        } else {
            strncpy(sink.name, "Unknown", sizeof(sink.name));
        }

        // Allocate memory for the new sink
        Sink *new_sinks = realloc(*sinks, (*count + 1) * sizeof(Sink));
        if (!new_sinks) {
            perror("Failed to allocate memory for sinks array");
            free(*sinks);
            return -1;
        }

        *sinks = new_sinks;
        (*sinks)[*count] = sink;
        (*count)++;
    }

    return 0;
}

static Sink create_sink_from_json(struct json item) {
    Sink sink;
    sink.id = -1;
    struct json info = json_object_get(item, "info");
    if (!json_exists(info)) {
        return sink;
    }

    struct json props = json_object_get(info, "props");
    if (!json_exists(props)) {
        return sink;
    }

    struct json media_class = json_object_get(props, "media.class");
    struct json description = json_object_get(props, "node.description");
    struct json object_id = json_object_get(item, "id");

    if (!json_exists(media_class) || json_string_compare(media_class, "Audio/Sink") != 0) {
        return sink;
    }

    if (!json_exists(description) || !json_exists(object_id)) {
        return sink;
    }

    json_string_copy(description, sink.description, sizeof(sink.description));

    struct json nick = json_object_get(props, "node.nick");
    struct json name = json_object_get(props, "node.name");
    if (json_exists(nick)) {
        json_string_copy(nick, sink.nick, sizeof(sink.nick));
    } else {
        strcpy(sink.nick, "Unknown");
    }

    if (json_exists(name)) {
        json_string_copy(name, sink.name, sizeof(sink.name));
    } else {
        strcpy(sink.name, "Unknown");
    }

    sink.id = json_int(object_id);

    const char *raw_json = json_raw(item);
    size_t raw_length = json_raw_length(item);
    if (raw_json && raw_length > 0) {
        DEBUG_PRINT("Full JSON for sink: %.*s\n", (int)raw_length, raw_json);
    }

    DEBUG_PRINT("Found sink: ID=%d, Description=%s, Nick=%s, Name=%s\n", sink.id, sink.description, sink.nick, sink.name);
    return sink;
}

int get_default_sink_id(int *sink_id) {
    const char* command = "wpctl status | grep -A 5 Sinks | grep '\\*' | sed 's/ \\|.*\\*   //' | cut -f1 -d'.'";
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        return -1;
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        *sink_id = atoi(buffer);
    } else {
        perror("Failed to read sink ID");
        pclose(fp);
        return -1;
    }

    pclose(fp);
    return 0;
}

int set_default_sink(int sink_id) {
    char command[256];
    snprintf(command, sizeof(command), "wpctl set-default %d", sink_id);
    DEBUG_PRINT("Executing command: %s\n", command);
    int result = system(command);
    if (result != 0) {
        perror("Failed to execute command");
    }
    DEBUG_PRINT("Command result: %d\n", result);
    return result;
}