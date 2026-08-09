#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/rand.h>
#include "../../common/fileio.h"
#include "../../common/options.h"
#include "account.h"

#define CHEEVO_ACCOUNT_DIR  STORAGE_NETWORK "/cheevo"
#define CHEEVO_ACCOUNT_FILE "account.ini"

static int text_safe(const char *value) {
    return value && !strchr(value, '\n') && !strchr(value, '\r');
}

static int directory_open(const int create) {
    if (create) create_directories(CHEEVO_ACCOUNT_DIR, 0);
    const int directory = open(CHEEVO_ACCOUNT_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
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

void cheevo_account_defaults(cheevo_account *account) {
    if (!account) return;
    memset(account, 0, sizeof(*account));
    account->notifications = cheevo_notifications_basic;
    account->achievement_sort = cheevo_sort_alphanumeric_ascending;
    account->achievement_view = cheevo_view_achievements;
}

int cheevo_account_load(cheevo_account *account) {
    if (!account) return -1;
    cheevo_account_defaults(account);

    const int directory = directory_open(0);
    if (directory < 0) return 1;
    const int account_fd = openat(directory, CHEEVO_ACCOUNT_FILE, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    close(directory);
    if (account_fd < 0) return 1;

    struct stat account_stat;
    if (fstat(account_fd, &account_stat) != 0 || !S_ISREG(account_stat.st_mode)
        || account_stat.st_uid != geteuid() || account_stat.st_nlink != 1 || account_stat.st_size > 4096) {
        close(account_fd);
        return -2;
    }
    if ((account_stat.st_mode & 0777) != 0600) fchmod(account_fd, 0600);
    FILE *file = fdopen(account_fd, "r");
    if (!file) {
        close(account_fd);
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char *newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals++ = '\0';

        if (strcmp(line, "enabled") == 0)
            account->enabled = atoi(equals) != 0;
        else if (strcmp(line, "hardcore") == 0)
            account->hardcore = atoi(equals) != 0;
        else if (strcmp(line, "unofficial") == 0)
            account->unofficial = atoi(equals) != 0;
        else if (strcmp(line, "notifications") == 0) {
            account->notifications = (cheevo_notification_mode) atoi(equals);
            if (account->notifications < cheevo_notifications_disabled
                || account->notifications > cheevo_notifications_detailed)
                account->notifications = cheevo_notifications_basic;
        } else if (strcmp(line, "achievement_sort") == 0) {
            account->achievement_sort = (cheevo_achievement_sort) atoi(equals);
            if (account->achievement_sort < cheevo_sort_alphanumeric_ascending
                || account->achievement_sort >= cheevo_sort_count)
                account->achievement_sort = cheevo_sort_alphanumeric_ascending;
        } else if (strcmp(line, "achievement_view") == 0) {
            account->achievement_view = (cheevo_achievement_view) atoi(equals);
            if (account->achievement_view < cheevo_view_achievements || account->achievement_view >= cheevo_view_count)
                account->achievement_view = cheevo_view_achievements;
        } else if (strcmp(line, "username") == 0)
            snprintf(account->username, sizeof(account->username), "%s", equals);
        else if (strcmp(line, "token") == 0)
            snprintf(account->token, sizeof(account->token), "%s", equals);
    }

    fclose(file);
    explicit_bzero(line, sizeof(line));
    return 0;
}

int cheevo_account_save(const cheevo_account *account) {
    if (!account || !text_safe(account->username) || !text_safe(account->token)) return -1;
    const int directory = directory_open(1);
    if (directory < 0) return -1;

    char temporary[64];
    int descriptor = -1;
    for (int attempt = 0; attempt < 8 && descriptor < 0; attempt++) {
        uint32_t nonce;
        if (RAND_bytes((unsigned char *) &nonce, sizeof(nonce)) != 1) break;
        snprintf(temporary, sizeof(temporary), ".account-%08x.tmp", nonce);
        descriptor = openat(directory, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (descriptor < 0 && errno != EEXIST) break;
    }
    if (descriptor < 0) {
        close(directory);
        return -1;
    }

    struct stat temporary_stat;
    if (fstat(descriptor, &temporary_stat) != 0 || !S_ISREG(temporary_stat.st_mode)
        || temporary_stat.st_uid != geteuid() || temporary_stat.st_nlink != 1) {
        close(descriptor);
        unlinkat(directory, temporary, 0);
        close(directory);
        return -1;
    }

    FILE *file = fdopen(descriptor, "w");
    if (!file) {
        close(descriptor);
        unlinkat(directory, temporary, 0);
        close(directory);
        return -1;
    }

    int okay =
        fprintf(
            file,
            "enabled=%d\nhardcore=%d\nunofficial=%d\nnotifications=%d\nachievement_sort=%d\nachievement_view=%"
            "d\nusername=%s\ntoken=%s\n",
            account->enabled, account->hardcore, account->unofficial, account->notifications,
            account->achievement_sort, account->achievement_view, account->username, account->token
        ) > 0
        && fflush(file) == 0 && fsync(descriptor) == 0;
    if (fclose(file) != 0) okay = 0;

    if (!okay || renameat(directory, temporary, directory, CHEEVO_ACCOUNT_FILE) != 0) {
        unlinkat(directory, temporary, 0);
        close(directory);
        return -1;
    }

    fsync(directory);
    close(directory);
    return 0;
}

void cheevo_account_delete(void) {
    const int directory = directory_open(0);
    if (directory < 0) return;
    unlinkat(directory, CHEEVO_ACCOUNT_FILE, 0);
    fsync(directory);
    close(directory);
}
