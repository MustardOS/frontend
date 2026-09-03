#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "../../common/fileio.h"
#include "../../common/options.h"
#include "spool.h"

#define CHEEVO_SPOOL_DIR        STORAGE_NETWORK "/cheevo/spool"
#define CHEEVO_SPOOL_CAP        512
#define CHEEVO_SPOOL_AGE        (90 * 24 * 60 * 60)
#define CHEEVO_SPOOL_SIZE       512
#define CHEEVO_SPOOL_TRIM_EVERY 16

typedef struct {
    int leaderboard;
    uint32_t id;
    int32_t score;
    char hash[33];
    long earned;
} spool_record;

static int spool_name_valid(const char *name) {
    if (!name || strncmp(name, "send-", 5) != 0) return 0;
    const size_t length = strlen(name);
    if (length != 5 + 16 + 4 || strcmp(name + length - 4, ".ini") != 0) return 0;
    for (size_t index = 5; index < length - 4; index++)
        if (!((name[index] >= '0' && name[index] <= '9') || (name[index] >= 'a' && name[index] <= 'f'))) return 0;
    return 1;
}

static int directory_open(const int create) {
    if (create) create_directories(CHEEVO_SPOOL_DIR, 0);
    const int directory = open(CHEEVO_SPOOL_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (directory < 0) return -1;

    struct stat directory_stat;
    if (fstat(directory, &directory_stat) != 0 || !S_ISDIR(directory_stat.st_mode)
        || directory_stat.st_uid != geteuid()) {
        close(directory);
        return -1;
    }
    if ((directory_stat.st_mode & 0777) != 0700) fchmod(directory, 0700);
    return directory;
}

static int form_value(const char *form, const char *key, char *value, const size_t value_size) {
    const size_t key_length = strlen(key);
    const char *field = form;

    while (*field) {
        const char *end = strchr(field, '&');
        if (!end) end = field + strlen(field);

        if ((size_t) (end - field) > key_length && memcmp(field, key, key_length) == 0 && field[key_length] == '=') {
            const size_t length = (size_t) (end - field) - key_length - 1;
            if (!length || length >= value_size) return 0;
            memcpy(value, field + key_length + 1, length);
            value[length] = '\0';
            return 1;
        }

        field = *end ? end + 1 : end;
    }
    return 0;
}

static int hex_value(const char *value) {
    for (size_t index = 0; value[index]; index++)
        if (!((value[index] >= 'a' && value[index] <= 'f') || (value[index] >= 'A' && value[index] <= 'F')
              || (value[index] >= '0' && value[index] <= '9')))
            return 0;
    return 1;
}

static int sign_submission(const char *parts[], const unsigned count, char out[33]) {
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context) return 0;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    int ok = EVP_DigestInit_ex(context, EVP_md5(), NULL) == 1;

    for (unsigned index = 0; ok && index < count; index++)
        ok = EVP_DigestUpdate(context, parts[index], strlen(parts[index])) == 1;

    ok = ok && EVP_DigestFinal_ex(context, digest, &digest_size) == 1 && digest_size == 16;
    EVP_MD_CTX_free(context);
    if (!ok) return 0;

    for (unsigned index = 0; index < 16; index++)
        snprintf(out + index * 2, 3, "%02x", digest[index]);

    return 1;
}

static void record_name(const spool_record *record, char *name, const size_t name_size) {
    char identity[64];
    snprintf(
        identity, sizeof(identity), "%s|%u|%d", record->leaderboard ? "lb" : "ac", record->id,
        record->leaderboard ? record->score : 0
    );

    uint64_t hash = 1469598103934665603ULL;
    for (const char *cursor = identity; *cursor; cursor++) {
        hash ^= (unsigned char) *cursor;
        hash *= 1099511628211ULL;
    }

    snprintf(name, name_size, "send-%016llx.ini", (unsigned long long) hash);
}

static int record_write(const int directory, const char *name, const spool_record *record) {
    char temporary[64];
    int descriptor = -1;

    for (int attempt = 0; attempt < 8 && descriptor < 0; attempt++) {
        uint32_t nonce;
        if (RAND_bytes((unsigned char *) &nonce, sizeof(nonce)) != 1) return -1;

        snprintf(temporary, sizeof(temporary), ".send-%08x.tmp", nonce);
        descriptor = openat(directory, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);

        if (descriptor < 0 && errno != EEXIST) return -1;
    }
    if (descriptor < 0) return -1;

    FILE *file = fdopen(descriptor, "w");
    if (!file) {
        close(descriptor);
        unlinkat(directory, temporary, 0);
        return -1;
    }

    int okay = fprintf(
                   file, "kind=%s\nid=%u\nscore=%d\nhash=%s\nearned=%ld\n", record->leaderboard ? "lbentry" : "award",
                   record->id, record->score, record->hash, record->earned
               ) > 0
               && fflush(file) == 0 && fsync(descriptor) == 0;

    if (fclose(file) != 0) okay = 0;

    if (!okay || renameat(directory, temporary, directory, name) != 0) {
        unlinkat(directory, temporary, 0);
        return -1;
    }

    fsync(directory);
    return 0;
}

static int record_read(const int directory, const char *name, spool_record *record) {
    const int descriptor = openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) return -1;

    struct stat file_stat;
    if (fstat(descriptor, &file_stat) != 0 || !S_ISREG(file_stat.st_mode) || file_stat.st_uid != geteuid()
        || file_stat.st_nlink != 1 || file_stat.st_size <= 0 || file_stat.st_size > CHEEVO_SPOOL_SIZE) {
        close(descriptor);
        return -1;
    }

    FILE *file = fdopen(descriptor, "r");
    if (!file) {
        close(descriptor);
        return -1;
    }

    memset(record, 0, sizeof(*record));

    int has_kind = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        char *newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals++ = '\0';

        if (strcmp(line, "kind") == 0) {
            record->leaderboard = strcmp(equals, "lbentry") == 0;
            has_kind = record->leaderboard || strcmp(equals, "award") == 0;
        } else if (strcmp(line, "id") == 0) {
            record->id = (uint32_t) strtoul(equals, NULL, 10);
        } else if (strcmp(line, "score") == 0) {
            record->score = (int32_t) strtol(equals, NULL, 10);
        } else if (strcmp(line, "hash") == 0) {
            if (strlen(equals) == 32 && hex_value(equals)) snprintf(record->hash, sizeof(record->hash), "%s", equals);
        } else if (strcmp(line, "earned") == 0) {
            record->earned = strtol(equals, NULL, 10);
        }
    }

    fclose(file);
    return has_kind && record->id ? 0 : -1;
}

static void spool_trim(const int directory) {
    for (;;) {
        unsigned total = 0;
        time_t oldest_time = 0;
        char oldest_name[32] = "";
        const time_t now = time(NULL);

        const int scan_descriptor = dup(directory);
        if (scan_descriptor < 0) return;
        DIR *scan = fdopendir(scan_descriptor);
        if (!scan) {
            close(scan_descriptor);
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(scan))) {
            if (!spool_name_valid(entry->d_name)) continue;

            struct stat file_stat;
            if (fstatat(directory, entry->d_name, &file_stat, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(file_stat.st_mode))
                continue;

            if (now > 0 && file_stat.st_mtime > 0 && now - file_stat.st_mtime > CHEEVO_SPOOL_AGE) {
                unlinkat(directory, entry->d_name, 0);
                continue;
            }

            total++;
            if (!oldest_name[0] || file_stat.st_mtime < oldest_time) {
                oldest_time = file_stat.st_mtime;
                snprintf(oldest_name, sizeof(oldest_name), "%s", entry->d_name);
            }
        }
        closedir(scan);

        if (total <= CHEEVO_SPOOL_CAP || !oldest_name[0]) return;
        if (unlinkat(directory, oldest_name, 0) != 0) return;
    }
}

int cheevo_spool_record(const char *post, char *name, const size_t name_size) {
    char request_type[32];
    if (!post || !name || name_size < 32 || !form_value(post, "r", request_type, sizeof(request_type))) return 0;

    spool_record record = {0};
    char value[64];

    if (strcmp(request_type, "awardachievement") == 0) {
        if (!form_value(post, "a", value, sizeof(value))) return 0;
        record.id = (uint32_t) strtoul(value, NULL, 10);
    } else if (strcmp(request_type, "submitlbentry") == 0) {
        if (!form_value(post, "i", value, sizeof(value))) return 0;
        record.id = (uint32_t) strtoul(value, NULL, 10);
        if (!form_value(post, "s", value, sizeof(value))) return 0;
        record.score = (int32_t) strtol(value, NULL, 10);
        record.leaderboard = 1;
    } else {
        return 0;
    }

    if (!record.id) return 0;

    char content_hash[64];
    if (form_value(post, "m", content_hash, sizeof(content_hash)) && strlen(content_hash) == 32
        && hex_value(content_hash))
        snprintf(record.hash, sizeof(record.hash), "%s", content_hash);

    record.earned = (long) time(NULL);
    if (form_value(post, "o", value, sizeof(value))) record.earned -= strtol(value, NULL, 10);

    record_name(&record, name, name_size);

    const int directory = directory_open(1);
    if (directory < 0) return 0;

    const int existed = faccessat(directory, name, F_OK, AT_SYMLINK_NOFOLLOW) == 0;
    const int written = record_write(directory, name, &record) == 0;

    static unsigned since_trim;
    if (written && !existed && ++since_trim >= CHEEVO_SPOOL_TRIM_EVERY) {
        since_trim = 0;
        spool_trim(directory);
    }

    close(directory);

    return written ? (existed ? 2 : 1) : 0;
}

void cheevo_spool_clear(const char *name) {
    if (!spool_name_valid(name)) return;
    const int directory = directory_open(0);
    if (directory < 0) return;
    unlinkat(directory, name, 0);
    fsync(directory);
    close(directory);
}

unsigned cheevo_spool_count(void) {
    const int directory = directory_open(0);
    if (directory < 0) return 0;

    const int scan_descriptor = dup(directory);
    if (scan_descriptor < 0) {
        close(directory);
        return 0;
    }
    DIR *scan = fdopendir(scan_descriptor);
    if (!scan) {
        close(scan_descriptor);
        close(directory);
        return 0;
    }

    unsigned total = 0;
    struct dirent *entry;

    while ((entry = readdir(scan)))
        if (spool_name_valid(entry->d_name)) total++;

    closedir(scan);
    close(directory);
    return total;
}

int cheevo_spool_next(
    const char *username, const char *token, char *name, const size_t name_size, char **post, size_t *post_size
) {
    if (!username || !username[0] || !token || !token[0] || !name || name_size < 32 || !post || !post_size) return -1;

    for (const char *field = username;; field = token) {
        for (size_t index = 0; field[index]; index++) {
            const char value = field[index];
            if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')
                  || value == '_' || value == '-'))
                return -1;
        }
        if (field == token) break;
    }

    const int directory = directory_open(0);
    if (directory < 0) return -1;

    const int scan_descriptor = dup(directory);
    if (scan_descriptor < 0) {
        close(directory);
        return -1;
    }
    DIR *scan = fdopendir(scan_descriptor);
    if (!scan) {
        close(scan_descriptor);
        close(directory);
        return -1;
    }

    time_t oldest_time = 0;
    char oldest_name[32] = "";
    struct dirent *entry;
    while ((entry = readdir(scan))) {
        if (!spool_name_valid(entry->d_name)) continue;

        struct stat file_stat;
        if (fstatat(directory, entry->d_name, &file_stat, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(file_stat.st_mode))
            continue;

        if (!oldest_name[0] || file_stat.st_mtime < oldest_time) {
            oldest_time = file_stat.st_mtime;
            snprintf(oldest_name, sizeof(oldest_name), "%s", entry->d_name);
        }
    }
    closedir(scan);

    spool_record record;
    if (!oldest_name[0] || record_read(directory, oldest_name, &record) != 0) {
        if (oldest_name[0]) unlinkat(directory, oldest_name, 0);
        close(directory);
        return -1;
    }
    close(directory);

    const long now = (long) time(NULL);
    unsigned long elapsed = record.earned > 0 && now > record.earned ? (unsigned long) (now - record.earned) : 0;

    char identity[32];
    char score[32];
    char elapsed_text[32];
    char signature[33];

    snprintf(identity, sizeof(identity), "%u", record.id);
    snprintf(elapsed_text, sizeof(elapsed_text), "%lu", elapsed);

    const char *parts[6];
    unsigned part_count = 0;
    parts[part_count++] = identity;
    parts[part_count++] = username;

    if (record.leaderboard) {
        snprintf(score, sizeof(score), "%d", record.score);
        parts[part_count++] = score;
        if (elapsed) parts[part_count++] = elapsed_text;
    } else {
        parts[part_count++] = "0";
        if (elapsed) {
            parts[part_count++] = identity;
            parts[part_count++] = elapsed_text;
        }
    }

    if (!sign_submission(parts, part_count, signature)) return -1;

    char body[1024];
    int written;
    if (record.leaderboard) {
        written = snprintf(
            body, sizeof(body), "r=submitlbentry&u=%s&t=%s&i=%u&s=%d%s%s%s%s&v=%s", username, token, record.id,
            record.score, record.hash[0] ? "&m=" : "", record.hash[0] ? record.hash : "", elapsed ? "&o=" : "",
            elapsed ? elapsed_text : "", signature
        );
    } else {
        written = snprintf(
            body, sizeof(body), "r=awardachievement&u=%s&t=%s&a=%u&h=0%s%s%s%s&v=%s", username, token, record.id,
            record.hash[0] ? "&m=" : "", record.hash[0] ? record.hash : "", elapsed ? "&o=" : "",
            elapsed ? elapsed_text : "", signature
        );
    }
    if (written <= 0 || (size_t) written >= sizeof(body)) return -1;

    char *copy = strdup(body);
    if (!copy) return -1;

    snprintf(name, name_size, "%s", oldest_name);
    *post = copy;
    *post_size = (size_t) written;
    return 0;
}
