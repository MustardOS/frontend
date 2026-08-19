#pragma once

void governor_boost_begin(const char *reason);

void governor_boost_end(void);

void governor_boost_gameplay_update(double observed_fps, double target_fps, int force);

void governor_boost_gameplay_pressure(double core_ms, double frame_budget_ms);

void governor_boost_gameplay_idle(void);

void governor_boost_shutdown(void);
