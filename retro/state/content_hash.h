#pragma once

enum content_hash_kind { content_hash_archive, content_hash_content, content_hash_cheevo, content_hash_count };

void content_hash_request(const char *archive_path, const char *content_path);

int content_hash_is_ready(enum content_hash_kind kind);

const char *content_hash_get(enum content_hash_kind kind);

int content_hash_resolve(
    enum content_hash_kind kind, const char *archive_path, const char *path, long long compute_limit, char *out,
    size_t out_size
);

int content_hash_archive_cached(enum content_hash_kind kind, const char *archive_path, char *out, size_t out_size);
