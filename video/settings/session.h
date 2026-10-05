#pragma once

int wasabi_session_begin(const char *uri);
void wasabi_session_end(void);
int wasabi_session_dirty(void);
void wasabi_session_discard(void);
void wasabi_session_reset(void);
int wasabi_session_save(int choice);
void wasabi_session_apply_idle_policy(void);
void wasabi_session_accept_volume(void);
