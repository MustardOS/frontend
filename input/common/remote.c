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

#define REMOTE_HOLD_MS    1500u
#define REMOTE_MIN_HOLD_MS 60u

enum remote_kind {
    remote_key,
    remote_hat,
};

struct remote_button {
    const char *name;
    enum remote_kind kind;
    unsigned short code;
    int direction;
};

static const struct remote_button remote_buttons[] = {
    {"up", remote_hat, ABS_HAT0Y, -1},      {"down", remote_hat, ABS_HAT0Y, 1},
    {"left", remote_hat, ABS_HAT0X, -1},    {"right", remote_hat, ABS_HAT0X, 1},
    {"a", remote_key, BTN_EAST, 0},         {"b", remote_key, BTN_SOUTH, 0},
    {"x", remote_key, BTN_NORTH, 0},        {"y", remote_key, BTN_WEST, 0},
    {"l1", remote_key, BTN_TL, 0},          {"r1", remote_key, BTN_TR, 0},
    {"l2", remote_key, BTN_TL2, 0},         {"r2", remote_key, BTN_TR2, 0},
    {"l3", remote_key, BTN_THUMBL, 0},      {"r3", remote_key, BTN_THUMBR, 0},
    {"select", remote_key, BTN_SELECT, 0},  {"start", remote_key, BTN_START, 0},
    {"menu", remote_key, BTN_MODE, 0},
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
    int available[REMOTE_BUTTON_COUNT];
    struct remote_hold holds[REMOTE_BUTTON_COUNT];
};

static int button_available(struct gamepad *gp, const struct remote_button *button) {
    return button->kind == remote_hat ? gamepad_has_hat(gp) : gamepad_has_key(gp, button->code);
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
    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) remote->available[i] = button_available(gp, &remote_buttons[i]);

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
    return gamepad_remote_key(remote->gp, button->code, remote->holds[index].held);
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

static int release_all(struct remote_input *remote, const uint64_t now_ms) {
    int changed = 0;
    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) changed |= set_held(remote, i, 0, now_ms);
    return changed;
}

static int handle_message(struct remote_input *remote, char *message, const uint64_t now_ms) {
    char *end = message + strcspn(message, "\r\n");
    *end = '\0';

    if (strcmp(message, "clear") == 0) return release_all(remote, now_ms);

    char *space = strchr(message, ' ');
    if (!space || (strcmp(space + 1, "0") != 0 && strcmp(space + 1, "1") != 0)) return 0;
    *space = '\0';

    for (size_t i = 0; i < REMOTE_BUTTON_COUNT; ++i) {
        if (strcmp(remote_buttons[i].name, message) != 0 || !remote->available[i]) continue;
        if (remote->verbose) fprintf(stderr, "remote %s %s\n", message, space + 1);
        return set_held(remote, i, space[1] == '1', now_ms);
    }
    return 0;
}

void remote_input_poll(struct remote_input *remote, const uint64_t now_ms) {
    if (!remote) return;

    int changed = 0;
    for (int messages = 0; messages < 64; ++messages) {
        char message[64];
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
