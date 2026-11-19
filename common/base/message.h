#pragma once

#include <common/base/options.h>

#define MESSAGE_FILE OPT_SHARE_PATH "media/message.json"

typedef struct {
    char *title;
    char **lines;
    int count;
} message_set;

int message_load(const char *path, message_set *set);

int message_index(const message_set *set, int index, const char *exclude);

void message_free(message_set *set);
