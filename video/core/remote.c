#include "remote.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define REMOTE_DIR        "/run/muos/wasabi"
#define REMOTE_FIFO       REMOTE_DIR "/control"
#define REMOTE_STATE      REMOTE_DIR "/now.json"
#define REMOTE_STATE_TEMP REMOTE_DIR "/now.json.tmp"
#define REMOTE_PERIOD_MS  1000
#define REMOTE_LINE       128

static int remote_reader = -1;
static int remote_keeper = -1;
static char remote_buffer[REMOTE_LINE * 4];
static size_t remote_length;
static uint32_t remote_published;

void wasabi_remote_open(void) {
    if (remote_reader >= 0) return;
    mkdir(REMOTE_DIR, 0755);
    unlink(REMOTE_FIFO);
    if (mkfifo(REMOTE_FIFO, 0600) != 0) return;
    remote_reader = open(REMOTE_FIFO, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (remote_reader < 0) return;
    remote_keeper = open(REMOTE_FIFO, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    remote_length = 0;
    remote_published = 0;
}

void wasabi_remote_close(void) {
    if (remote_reader >= 0) close(remote_reader);
    if (remote_keeper >= 0) close(remote_keeper);
    remote_reader = remote_keeper = -1;
    remote_length = 0;
    unlink(REMOTE_FIFO);
    unlink(REMOTE_STATE);
}

static int parse_command(char *line, wasabi_remote_command *command) {
    char *argument = strchr(line, ' ');
    if (argument) *argument++ = '\0';
    const double value = argument ? strtod(argument, NULL) : 0.0;
    static const struct {
        const char *name;
        wasabi_remote_type type;
    } names[] = {
        {"toggle", wasabi_remote_toggle},     {"play", wasabi_remote_play},     {"pause", wasabi_remote_pause},
        {"seek", wasabi_remote_seek},         {"skip", wasabi_remote_skip},     {"next", wasabi_remote_next},
        {"previous", wasabi_remote_previous}, {"volume", wasabi_remote_volume}, {"stop", wasabi_remote_stop},
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
    if (remote_reader < 0 || !command) return 0;
    for (;;) {
        char *newline = memchr(remote_buffer, '\n', remote_length);
        if (newline) {
            *newline = '\0';
            const size_t used = (size_t) (newline - remote_buffer) + 1;
            char line[REMOTE_LINE];
            snprintf(line, sizeof(line), "%s", remote_buffer);
            memmove(remote_buffer, remote_buffer + used, remote_length - used);
            remote_length -= used;
            if (parse_command(line, command)) return 1;
            continue;
        }
        if (remote_length >= sizeof(remote_buffer)) remote_length = 0;
        const ssize_t got = read(remote_reader, remote_buffer + remote_length, sizeof(remote_buffer) - remote_length);
        if (got <= 0) return 0;
        remote_length += (size_t) got;
    }
}

static void json_text(FILE *file, const char *key, const char *value) {
    fprintf(file, "\"%s\":\"", key);
    for (const unsigned char *cursor = (const unsigned char *) (value ? value : ""); *cursor; cursor++) {
        if (*cursor == '"' || *cursor == '\\')
            fprintf(file, "\\%c", *cursor);
        else if (*cursor < 0x20)
            fprintf(file, "\\u%04x", *cursor);
        else
            fputc(*cursor, file);
    }
    fputs("\",", file);
}

void wasabi_remote_publish(const wasabi_remote_state *state, const int force) {
    if (remote_reader < 0 || !state) return;
    const uint32_t now = SDL_GetTicks();
    if (!force && remote_published && now - remote_published < REMOTE_PERIOD_MS) return;
    remote_published = now ? now : 1;

    FILE *file = fopen(REMOTE_STATE_TEMP, "w");
    if (!file) return;
    fputc('{', file);
    json_text(file, "title", state->title);
    json_text(file, "artist", state->artist);
    json_text(file, "album", state->album);
    fprintf(
        file,
        "\"live\":%d,\"audio\":%d,\"paused\":%d,\"seek\":%d,\"index\":%d,\"count\":%d,\"channels\":%d,"
        "\"volume\":%d,\"position\":%.2f,\"duration\":%.2f,\"updated\":%lld}\n",
        state->live, state->audio, state->paused, state->can_seek, state->index, state->count, state->channels,
        state->volume, state->position, state->duration, (long long) time(NULL)
    );
    if (fclose(file) == 0) rename(REMOTE_STATE_TEMP, REMOTE_STATE);
}
