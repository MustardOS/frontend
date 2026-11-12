#pragma once

#include <time.h>

void content_clock_register_core(void *handle);

void content_clock_apply(void);

time_t content_clock_now(void);
