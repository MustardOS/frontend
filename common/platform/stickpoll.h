#pragma once

enum stick_poll_state { stick_poll_unsupported = -1, stick_poll_off = 0, stick_poll_on = 1 };

int stick_poll_get(void);

int stick_poll_set(int on);
