#define _POSIX_C_SOURCE 200809L

#include "remote.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define REMOTE_HOLD_MS       1500u
#define REMOTE_MIN_HOLD_MS   60u
#define REMOTE_STICK_HOLD_MS 300u
#define REMOTE_MESSAGE_MAX   256

enum remote_kind {
    remote_key,
    remote_hat,
    remote_stick,
};

struct remote_button {
    const char *name;
    const char *sdl_name;
    enum remote_kind kind;
    unsigned short code;
    int direction;
};

static const struct remote_button remote_buttons[] = {
    {"up", NULL, remote_hat, ABS_HAT0Y, -1},
    {"down", NULL, remote_hat, ABS_HAT0Y, 1},
    {"left", NULL, remote_hat, ABS_HAT0X, -1},
    {"right", NULL, remote_hat, ABS_HAT0X, 1},
    {"a", "a", remote_key, BTN_EAST, 0},
    {"b", "b", remote_key, BTN_SOUTH, 0},
    {"x", "x", remote_key, BTN_NORTH, 0},
    {"y", "y", remote_key, BTN_WEST, 0},
    {"l1", "leftshoulder", remote_key, BTN_TL, 0},
    {"r1", "rightshoulder", remote_key, BTN_TR, 0},
    {"l2", "lefttrigger", remote_key, BTN_TL2, 0},
    {"r2", "righttrigger", remote_key, BTN_TR2, 0},
    {"l3", "leftstick", remote_key, BTN_THUMBL, 0},
    {"r3", "rightstick", remote_key, BTN_THUMBR, 0},
    {"select", "back", remote_key, BTN_SELECT, 0},
    {"start", "start", remote_key, BTN_START, 0},
    {"menu", "guide", remote_key, BTN_MODE, 0},
    {"lx", NULL, remote_stick, ABS_X, 0},
    {"ly", NULL, remote_stick, ABS_Y, 0},
    {"rx", NULL, remote_stick, ABS_RX, 0},
    {"ry", NULL, remote_stick, ABS_RY, 0},
};

#define REMOTE_BUTTON_COUNT (sizeof(remote_buttons) / sizeof(remote_buttons[0]))

struct remote_hold {
    int held;
    uint64_t pressed_at;
    uint64_t expires_at;
    uint64_t release_at;
};

struct remote_input {
    int fd;
    int verbose;
    struct gamepad *gp;
    unsigned short codes[REMOTE_BUTTON_COUNT];
    int available[REMOTE_BUTTON_COUNT];
    struct remote_hold holds[REMOTE_BUTTON_COUNT];
    int stick_values[REMOTE_BUTTON_COUNT];
};

static int button_available(struct gamepad *gp, const struct remote_button *button, const unsigned short code) {
    if (button->kind == remote_stick) return gamepad_has_stick_axis(gp, code);
    return button->kind == remote_hat ? gamepad_has_hat(gp) : gamepad_has_key(gp, code);
}

static int read_sdl_map(char *map, const size_t size) {
    FILE *file = fopen(MUINPUT_SDL_MAP, "r");
    if (!file) return 0;
    const int read = fgets(map, (int) size, file) != NULL;
    fclose(file);
    return read;
}

static int sdl_map_index(const char *map, const char *sdl_name, size_t *index) {
    const size_t length = strlen(sdl_name);
    for (const char *field = map; field && *field; field = strchr(field, ',')) {
        if (*field == ',') ++field;
        if (strncmp(field, sdl_name, length) != 0 || field[length] != ':' || field[length + 1] != 'b') continue;

        char *end = NULL;
        const unsigned long value = strtoul(field + length + 2, &end, 10);
        if (end == field + length + 2 || (*end != ',' && *end != '\0' && *end != '\n')) return 0;
        *index = (size_t) value;
        return 1;
    }
    return 0;
}

static unsigned short button_code(const struct gamepad *gp, const struct remote_button *button, const char *map) {
    if (button->kind != remote_key || !button->sdl_name || !map) return button->code;

    size_t index;
    unsigned short code;
    if (!sdl_map_index(map, button->sdl_name, &index) || !gamepad_key_at_index(gp, index, &code)) return button->code;
    return code;
}

static void write_key_list(const struct remote_input *remote) {
    const char *temporary = MUINPUT_REMOTE_KEYS ".tmp";
    FILE *file = fopen(temporary, "w");
    if (!file) return;

    int first = 1;
    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        if (!remote->available[i]) continue;
        fprintf(file, "%s%s", first ? "" : " ", remote_buttons[i].name);
        first = 0;
    }
    fputc('\n', file);

    if (fclose(file) != 0 || rename(temporary, MUINPUT_REMOTE_KEYS) != 0) unlink(temporary);
}

struct remote_input *remote_input_open(struct gamepad *gp, const int verbose) {
    struct remote_input *remote = calloc(1, sizeof(*remote));
    if (!remote) return NULL;

    remote->gp = gp;
    remote->verbose = verbose;

    char map[1024];
    const char *sdl_map = read_sdl_map(map, sizeof(map)) ? map : NULL;
    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        remote->codes[i] = button_code(gp, &remote_buttons[i], sdl_map);
        remote->available[i] = button_available(gp, &remote_buttons[i], remote->codes[i]);
    }

    remote->fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (remote->fd < 0) {
        perror("remote input socket");
        free(remote);
        return NULL;
    }

    struct sockaddr_un address = {.sun_family = AF_UNIX};
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", MUINPUT_REMOTE_SOCKET);
    unlink(MUINPUT_REMOTE_SOCKET);

    const mode_t mask = umask(0177);
    const int bound = bind(remote->fd, (const struct sockaddr *) &address, sizeof(address));
    umask(mask);
    if (bound < 0) {
        perror("remote input bind");
        close(remote->fd);
        free(remote);
        return NULL;
    }

    write_key_list(remote);
    return remote;
}

static int hat_value(const struct remote_input *remote, const unsigned short code) {
    int value = 0;
    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        if (remote_buttons[i].kind == remote_hat && remote_buttons[i].code == code && remote->holds[i].held) {
            value += remote_buttons[i].direction;
        }
    }
    return value;
}

static int apply(struct remote_input *remote, const size_t index) {
    const struct remote_button *button = &remote_buttons[index];
    if (button->kind == remote_hat) {
        return gamepad_remote_hat(remote->gp, button->code, hat_value(remote, button->code));
    }
    return gamepad_remote_key(remote->gp, remote->codes[index], remote->holds[index].held);
}

static int set_held(struct remote_input *remote, const size_t index, const int held, const uint64_t now_ms) {
    struct remote_hold *hold = &remote->holds[index];

    if (held) {
        hold->expires_at = now_ms + REMOTE_HOLD_MS;
        hold->release_at = 0;
        if (hold->held) return 0;
        hold->held = 1;
        hold->pressed_at = now_ms;
        return apply(remote, index);
    }

    if (!hold->held) return 0;
    if (now_ms < hold->pressed_at + REMOTE_MIN_HOLD_MS) {
        hold->release_at = hold->pressed_at + REMOTE_MIN_HOLD_MS;
        return 0;
    }

    hold->held = 0;
    hold->release_at = 0;
    return apply(remote, index);
}

static int set_stick(struct remote_input *remote, const size_t index, const int value, const uint64_t now_ms) {
    struct remote_hold *hold = &remote->holds[index];
    hold->held = value != 0;
    hold->expires_at = now_ms + REMOTE_STICK_HOLD_MS;
    if (remote->stick_values[index] == value) return 0;
    remote->stick_values[index] = value;
    return gamepad_remote_stick(remote->gp, remote->codes[index], value);
}

static int release_all(struct remote_input *remote, const uint64_t now_ms) {
    int changed = 0;
    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        if (remote_buttons[i].kind == remote_stick) {
            changed |= set_stick(remote, i, 0, now_ms);
            remote->holds[i].held = 0;
        } else {
            changed |= set_held(remote, i, 0, now_ms);
        }
    }
    return changed;
}

static int stick_value(const char *text, int *value) {
    char *end = NULL;
    const float level = strtof(text, &end);
    if (end == text || *end != '\0' || level != level) return 0;
    const float clamped = level < -1.0f ? -1.0f : level > 1.0f ? 1.0f : level;
    *value = (int) (clamped * 32767.0f);
    return 1;
}

static int handle_line(struct remote_input *remote, char *line, const uint64_t now_ms) {
    if (strcmp(line, "clear") == 0) return release_all(remote, now_ms);

    char *space = strchr(line, ' ');
    if (!space) return 0;
    *space = '\0';
    const char *argument = space + 1;

    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        if (strcmp(remote_buttons[i].name, line) != 0 || !remote->available[i]) continue;
        if (remote->verbose) fprintf(stderr, "remote %s %s\n", line, argument);

        if (remote_buttons[i].kind == remote_stick) {
            int value;
            return stick_value(argument, &value) ? set_stick(remote, i, value, now_ms) : 0;
        }
        if (strcmp(argument, "0") != 0 && strcmp(argument, "1") != 0) return 0;
        return set_held(remote, i, argument[0] == '1', now_ms);
    }
    return 0;
}

static int handle_message(struct remote_input *remote, char *message, const uint64_t now_ms) {
    int changed = 0;
    char *line = message;
    while (line && *line) {
        char *next = line + strcspn(line, "\r\n");
        if (*next)
            *next++ = '\0';
        else
            next = NULL;
        if (*line) changed |= handle_line(remote, line, now_ms);
        line = next;
    }
    return changed;
}

void remote_input_poll(struct remote_input *remote, const uint64_t now_ms) {
    if (!remote) return;

    int changed = 0;
    for (int messages = 0; messages < 64; ++messages) {
        char message[REMOTE_MESSAGE_MAX];
        const ssize_t length = recv(remote->fd, message, sizeof(message) - 1, 0);
        if (length < 0) {
            if (errno == EINTR) continue;
            break;
        }
        message[length] = '\0';
        changed |= handle_message(remote, message, now_ms);
    }

    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        struct remote_hold *hold = &remote->holds[i];
        if (!hold->held) continue;
        if (remote_buttons[i].kind == remote_stick) {
            if (now_ms >= hold->expires_at) {
                changed |= set_stick(remote, i, 0, now_ms);
                hold->held = 0;
            }
            continue;
        }
        if ((hold->release_at && now_ms >= hold->release_at) || now_ms >= hold->expires_at) {
            hold->held = 0;
            hold->release_at = 0;
            changed |= apply(remote, i);
        }
    }

    if (changed) gamepad_sync(remote->gp);
}

void remote_input_close(struct remote_input *remote) {
    if (!remote) return;

    close(remote->fd);
    unlink(MUINPUT_REMOTE_SOCKET);
    unlink(MUINPUT_REMOTE_KEYS);
    free(remote);
}
