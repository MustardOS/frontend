#pragma once

#include <stdint.h>

#include "uinput.h"

#define MUINPUT_REMOTE_SOCKET "/run/muinput/remote.sock"
#define MUINPUT_REMOTE_KEYS   "/run/muinput/remote.keys"

struct remote_input;

struct remote_input *remote_input_open(struct gamepad *gp, int verbose);

void remote_input_poll(struct remote_input *remote, uint64_t now_ms);

void remote_input_close(struct remote_input *remote);
