#pragma once

int hotkeys_task(void);

void hotkeys_volume_bright_task(void);

int hotkeys_is_fast_forward_active(void);

int hotkeys_is_slow_motion_active(void);

int hotkeys_is_quit_requested(void);

void hotkeys_request_quit(void);

int hotkeys_is_manual_requested(void);

void hotkeys_reset(void);
