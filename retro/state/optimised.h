#pragma once

enum optimised_state { optimised_state_none = 0, optimised_state_applied, optimised_state_invalid };

int optimised_init(const char *core_name, const char *content_path);

int optimised_active(void);

const char *optimised_name(void);

const char *optimised_option(const char *key);

int optimised_setting(const char *key, int *value);

enum optimised_state optimised_status(void);

void optimised_option_result(const char *key, int applied);

void optimised_setting_result(const char *key, int applied);
