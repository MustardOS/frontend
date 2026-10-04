#pragma once

#include <stddef.h>

#define WASABI_EQ_BANDS 10
#define WASABI_EQ_GAINS (WASABI_EQ_BANDS + 1)
#define WASABI_EQ_LIMIT 12
#define WASABI_EQ_STEPS 4

void wasabi_eq_changed(void);
void wasabi_eq_process(float *samples, int frames, int channels, int rate);
int wasabi_eq_gain(int index);
int wasabi_eq_set_gain(int index, int value);
const char *wasabi_eq_band_label(int band);
void wasabi_eq_format(int steps, char *value, size_t size);

void wasabi_eq_profiles_refresh(void);
int wasabi_eq_profile_count(void);
const char *wasabi_eq_profile_name(int index);
int wasabi_eq_profile_current(void);
int wasabi_eq_profile_cycle(int direction);
int wasabi_eq_profile_save(const char *name);
int wasabi_eq_profile_deletable(void);
int wasabi_eq_profile_delete(void);
void wasabi_eq_profile_label(char *value, size_t size);
