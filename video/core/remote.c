#include "remote.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <common/runtime/remote.h>

#define REMOTE_DIR        "/run/muos/wasabi"
#define REMOTE_FIFO       REMOTE_DIR "/control"
#define REMOTE_STATE      REMOTE_DIR "/now.json"
#define REMOTE_STATE_TEMP REMOTE_DIR "/now.json.tmp"
#define REMOTE_LIST       REMOTE_DIR "/list.json"
#define REMOTE_LIST_TEMP  REMOTE_DIR "/list.json.tmp"
#define REMOTE_PERIOD_MS  1000
#define REMOTE_LINE       128

static remote_control remote = REMOTE_CONTROL_INITIALIZER;
static uint32_t remote_published;
static int remote_list_wanted;

void wasabi_remote_open(void) {
    if (!remote_control_open(&remote, REMOTE_DIR, REMOTE_FIFO)) return;
    remote_published = 0;
    remote_list_wanted = 1;
}

int wasabi_remote_list_wanted(void) {
    const int wanted = remote_list_wanted;
    remote_list_wanted = 0;
    return wanted;
}

void wasabi_remote_close(void) {
    remote_control_close(&remote, REMOTE_FIFO);
    unlink(REMOTE_STATE);
    unlink(REMOTE_LIST);
}

static int parse_command(char *line, wasabi_remote_command *command) {
    char *argument = strchr(line, ' ');
    if (argument) *argument++ = '\0';
    const double value = argument ? strtod(argument, NULL) : 0.0;
    static const struct {
        const char *name;
        wasabi_remote_type type;
    } names[] = {
        {"toggle", wasabi_remote_toggle},
        {"play", wasabi_remote_play},
        {"pause", wasabi_remote_pause},
        {"seek", wasabi_remote_seek},
        {"skip", wasabi_remote_skip},
        {"next", wasabi_remote_next},
        {"previous", wasabi_remote_previous},
        {"volume", wasabi_remote_volume},
        {"stop", wasabi_remote_stop},
        {"jump", wasabi_remote_jump},
        {"setvolume", wasabi_remote_set_volume},
        {"shuffle", wasabi_remote_shuffle},
        {"repeat", wasabi_remote_repeat},
    };
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
        if (strcmp(line, names[index].name) != 0) continue;
        command->type = names[index].type;
        command->value = value;
        return 1;
    }
    return 0;
}

int wasabi_remote_poll(wasabi_remote_command *command) {
    if (!command) return 0;
    char line[REMOTE_LINE];
    while (remote_control_read_line(&remote, line, sizeof(line))) {
        if (parse_command(line, command)) return 1;
    }
    return 0;
}

void wasabi_remote_publish(const wasabi_remote_state *state, const int force) {
    if (remote.reader < 0 || !state) return;
    const uint32_t now = SDL_GetTicks();
    if (!force && remote_published && now - remote_published < REMOTE_PERIOD_MS) return;
    remote_published = now ? now : 1;

    FILE *file = fopen(REMOTE_STATE_TEMP, "w");
    if (!file) return;
    fputc('{', file);
    remote_json_text(file, "title", state->title);
    remote_json_text(file, "artist", state->artist);
    remote_json_text(file, "album", state->album);
    remote_json_text(file, "uri", state->uri);
    fprintf(
        file,
        "\"live\":%d,\"audio\":%d,\"paused\":%d,\"seek\":%d,\"index\":%d,\"count\":%d,\"channels\":%d,"
        "\"volume\":%d,\"shuffle\":%d,\"repeat\":%d,\"position\":%.2f,\"duration\":%.2f,\"updated\":%lld}\n",
        state->live, state->audio, state->paused, state->can_seek, state->index, state->count, state->channels,
        state->volume, state->shuffle, state->repeat, state->position, state->duration, (long long) time(NULL)
    );
    if (fclose(file) == 0) rename(REMOTE_STATE_TEMP, REMOTE_STATE);
}

void wasabi_remote_publish_list(const video_library_entry *entries, const size_t count) {
    if (remote.reader < 0) return;

    FILE *file = fopen(REMOTE_LIST_TEMP, "w");
    if (!file) return;
    fputs("{\"items\":[", file);
    for (size_t index = 0; entries && index < count; index++) {
        const char *uri = entries[index].uri ? entries[index].uri : "";
        const char *base = strrchr(uri, '/');
        const char *title = entries[index].title && entries[index].title[0] ? entries[index].title
                            : base                                          ? base + 1
                                                                            : uri;
        fputs(index ? ",{" : "{", file);
        remote_json_text(file, "title", title);
        fprintf(file, "\"index\":%zu}", index);
    }
    fputs("]}\n", file);
    if (fclose(file) == 0) rename(REMOTE_LIST_TEMP, REMOTE_LIST);
}
