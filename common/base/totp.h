#pragma once

#include <stddef.h>
#include <stdint.h>

#define TOTP_STEP        30
#define TOTP_SKEW        1
#define TOTP_DIGITS      6
#define TOTP_SECRET_SIZE 20

int totp_secret_load(const char *path, unsigned char *secret);

int totp_secret_read(const char *path, unsigned char *secret);

int64_t totp_window(int64_t when);

int totp_remaining(int64_t when);

void totp_code(const unsigned char *secret, int64_t window, char *out, size_t out_size);

int totp_matches(const unsigned char *secret, const char *candidate);
