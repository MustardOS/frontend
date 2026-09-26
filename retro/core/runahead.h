#pragma once

#include <stddef.h>

void runahead_before_frame(int allow_replay);

void runahead_invalidate(void);

void runahead_settle_cheevo(void);

int runahead_cheevo_deferred(void);

void runahead_shutdown(void);

size_t runahead_state_size(void);

int runahead_session_failed(void);
