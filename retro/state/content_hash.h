#pragma once

void content_hash_request(const char *content_path);

int content_hash_is_ready(void);

const char *content_hash_get(void);
