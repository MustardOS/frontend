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
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include "core.h"
#include "muxretro.h"
#include "../input/hotkeys.h"
#include "../netplay/netplay.h"
#include "../settings/settings.h"
#include "../state/gamestate.h"

#define REMOTE_DIR        "/run/muos/pickles"
#define REMOTE_FIFO       REMOTE_DIR "/control"
#define REMOTE_STATE      REMOTE_DIR "/now.json"
#define REMOTE_STATE_TEMP REMOTE_DIR "/now.json.tmp"
#define REMOTE_PERIOD_MS  1000
#define REMOTE_LINE       64

static int remote_reader = -1;
static int remote_keeper = -1;
static char remote_buffer[REMOTE_LINE * 4];
static size_t remote_length;
static uint32_t remote_published;
static uint32_t remote_last_tick;
static uint64_t remote_played_ms;
static double remote_fps;
static int remote_last_menu = -1;
static int remote_last_paused = -1;
static int remote_last_speed = -1;

void pickles_remote_open(void) {
    if (remote_reader >= 0) return;
    mkdir(REMOTE_DIR, 0755);
    unlink(REMOTE_FIFO);
    if (mkfifo(REMOTE_FIFO, 0600) != 0) return;
    remote_reader = open(REMOTE_FIFO, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (remote_reader < 0) return;
    remote_keeper = open(REMOTE_FIFO, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    remote_length = 0;
    remote_published = 0;
    remote_last_tick = SDL_GetTicks();
    remote_played_ms = 0;
}

void pickles_remote_close(void) {
    if (remote_reader >= 0) close(remote_reader);
    if (remote_keeper >= 0) close(remote_keeper);
    remote_reader = remote_keeper = -1;
    remote_length = 0;
    unlink(REMOTE_FIFO);
    unlink(REMOTE_STATE);
}

void pickles_remote_set_fps(const double fps) {
    remote_fps = fps;
}

static void run_line(char *line, const int menu_open) {
    char *argument = strchr(line, ' ');
    if (argument) *argument++ = '\0';
    const int value = argument ? atoi(argument) : 1;

    if (menu_open && strcmp(line, "quit") != 0) {
        LOG_INFO(mux_module, "Remote command '%s' ignored while the menu is open", line);
        return;
    }
    if (!hotkeys_remote_command(line, value)) LOG_INFO(mux_module, "Remote command '%s' was not accepted", line);
}

static void read_commands(const int menu_open) {
    for (;;) {
        char *newline = memchr(remote_buffer, '\n', remote_length);
        if (newline) {
            *newline = '\0';
            const size_t used = (size_t) (newline - remote_buffer) + 1;
            char line[REMOTE_LINE];
            snprintf(line, sizeof(line), "%s", remote_buffer);
            memmove(remote_buffer, remote_buffer + used, remote_length - used);
            remote_length -= used;
            run_line(line, menu_open);
            continue;
        }
        if (remote_length >= sizeof(remote_buffer)) remote_length = 0;
        const ssize_t got = read(remote_reader, remote_buffer + remote_length, sizeof(remote_buffer) - remote_length);
        if (got <= 0) return;
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

static void content_name(char *out, const size_t size) {
    const char *base = strrchr(core_content_path, '/');
    snprintf(out, size, "%s", base ? base + 1 : core_content_path);
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = '\0';
}

static void content_folder(char *out, const size_t size) {
    snprintf(out, size, "%s", core_content_path);
    char *slash = strrchr(out, '/');
    if (!slash) {
        out[0] = '\0';
        return;
    }
    *slash = '\0';
    const char *parent = strrchr(out, '/');
    if (parent) memmove(out, parent + 1, strlen(parent + 1) + 1);
}

static void publish(const int menu_open, const int force) {
    const uint32_t now = SDL_GetTicks();
    if (!force && remote_published && now - remote_published < REMOTE_PERIOD_MS) return;
    remote_published = now ? now : 1;

    char name[PATH_MAX];
    char folder[PATH_MAX];
    char core[128];
    content_name(name, sizeof(name));
    content_folder(folder, sizeof(folder));
    if (!core_get_name(core_file_path, core, sizeof(core))) core[0] = '\0';

    const char *speed = hotkeys_is_fast_forward_active()   ? "fast_forward"
                        : hotkeys_is_slow_motion_active() ? "slow_motion"
                                                          : "normal";

    FILE *file = fopen(REMOTE_STATE_TEMP, "w");
    if (!file) return;
    fputc('{', file);
    json_text(file, "name", name);
    json_text(file, "folder", folder);
    json_text(file, "core", core);
    json_text(file, "library", core_library_name());
    json_text(file, "version", core_library_version());
    json_text(file, "speed", speed);
    fprintf(
        file,
        "\"menu\":%d,\"paused\":%d,\"netplay\":%d,\"saves\":%d,\"quicksave\":%d,\"fps\":%.2f,"
        "\"played\":%llu,\"updated\":%lld}\n",
        menu_open, hotkeys_is_content_paused(), netplay_is_active(), state_saves_allowed() && state_saves_supported(),
        gamestate_quicksave_exists, remote_fps, (unsigned long long) (remote_played_ms / 1000), (long long) time(NULL)
    );
    if (fclose(file) == 0) rename(REMOTE_STATE_TEMP, REMOTE_STATE);
}

void pickles_remote_task(const int menu_open) {
    if (remote_reader < 0) return;

    const uint32_t now = SDL_GetTicks();
    if (!menu_open && !hotkeys_is_content_paused()) remote_played_ms += now - remote_last_tick;
    remote_last_tick = now;

    read_commands(menu_open);

    const int paused = hotkeys_is_content_paused();
    const int speed = hotkeys_is_fast_forward_active() ? 1 : hotkeys_is_slow_motion_active() ? 2 : 0;
    const int changed = menu_open != remote_last_menu || paused != remote_last_paused || speed != remote_last_speed;
    remote_last_menu = menu_open;
    remote_last_paused = paused;
    remote_last_speed = speed;

    publish(menu_open, changed);
}
