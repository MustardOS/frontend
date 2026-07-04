#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "init.h"
#include "fileio.h"
#include "util.h"
#include "strutil.h"
#include "log.h"
#include "language.h"

int file_exist(const char *filename) {
    return access(filename, F_OK) == 0;
}

int dir_exist(const char *dirname) {
    struct stat stats;
    return stat(dirname, &stats) == 0 && S_ISDIR(stats.st_mode);
}

// Just so nobody is confused in the future...
// line < 0  returns entire output
// line == 0 returns first line
// line == 1 returns second line
char *get_execute_result(const char *command, const int line) {
    FILE *fp = popen(command, "r");
    if (!fp) {
        LOG_ERROR(mux_module, "Failed to run: %s", command);
        return NULL;
    }

    char buffer[MAX_BUFFER_SIZE];
    size_t total = 0;
    char *result = NULL;

    int current_line = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (line >= 0) {
            if (current_line == line) {
                char *nl = strchr(buffer, '\n');
                if (nl) *nl = '\0';
                pclose(fp);
                return strdup(buffer);
            }
            current_line++;
            continue;
        }

        const size_t len = strlen(buffer);
        char *tmp = realloc(result, total + len + 1);
        if (!tmp) {
            free(result);
            pclose(fp);
            return NULL;
        }

        result = tmp;
        memcpy(result + total, buffer, len);

        total += len;
        result[total] = '\0';
    }

    pclose(fp);

    if (line >= 0) return NULL;
    return result;
}

char *read_all_char_from(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) return strdup("");

    fseek(file, 0, SEEK_END);
    const long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        return strdup("");
    }

    char *text = malloc((size_t) file_size + 1);

    if (text != NULL) {
        const size_t bytes_read = fread(text, 1, (size_t) file_size, file);

        if (bytes_read > 0 && text[bytes_read - 1] == '\n') {
            text[bytes_read - 1] = '\0';
        } else {
            text[bytes_read] = '\0';
        }
    } else {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        fclose(file);
        return strdup("");
    }

    fclose(file);
    return text;
}

char *read_line_char_from(const char *filename, const size_t line_number) {
    if (!filename || line_number == 0) {
        LOG_ERROR(mux_module, "Invalid filename or line number...");
        return strdup("");
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_open, filename);
        return strdup("");
    }

    char *line = malloc(MAX_BUFFER_SIZE);
    if (!line) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        fclose(file);
        return strdup("");
    }

    size_t current_line = 0;
    while (fgets(line, MAX_BUFFER_SIZE, file) != NULL) {
        current_line++;
        if (current_line == line_number) {
            const size_t length = strlen(line);

            if (length > 0 && line[length - 1] == '\n') line[length - 1] = '\0';

            fclose(file);
            return line;
        }
    }

    free(line);
    fclose(file);

    return strdup("");
}

int read_all_int_from(const char *filename, size_t buffer) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    char line[buffer];
    size_t buf_size = sizeof(line);
    if (buf_size > INT_MAX) buf_size = INT_MAX;

    if (!fgets(line, (int) buf_size, file)) {
        fclose(file);
        return 0;
    }

    fclose(file);
    return safe_atoi(line, 0);
}

int read_line_int_from(const char *filename, const size_t line_number) {
    char line[MAX_BUFFER_SIZE];
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    for (size_t i = 1; i <= line_number && fgets(line, sizeof(line), file); i++) {
        if (i == line_number) {
            line[strcspn(line, "\n")] = '\0';
            fclose(file);
            return safe_atoi(line, 0);
        }
    }

    fclose(file);
    return 0;
}

unsigned long long read_all_long_from(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    char buf[64];
    if (!fgets(buf, sizeof(buf), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    char *end_ptr;
    errno = 0;

    const unsigned long long value = strtoull(buf, &end_ptr, 10);
    if (errno != 0 || end_ptr == buf || (*end_ptr && *end_ptr != '\n')) return 0;

    return value;
}

void cfg_write_def_int(const char *path, const int value) {
    if (file_exist(path)) return;

    create_directories(path, 1);
    write_text_to_file(path, "w", INT, value);
}

void cfg_write_def_char(const char *path, const char *value) {
    if (file_exist(path)) return;

    create_directories(path, 1);
    write_text_to_file(path, "w", CHAR, value);
}

void write_text_to_file(const char *filename, const char *mode, const int type, ...) {
    FILE *file = fopen(filename, mode);

    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, filename);
        return;
    }

    va_list args;
    va_start(args, type);

    if (type == CHAR) { // type is general text!
        fprintf(file, "%s", va_arg(args, const char *));
    } else if (type == INT) { // type is a number!
        fprintf(file, "%d", va_arg(args, int));
    }

    va_end(args);
    fclose(file);
}

void write_text_to_file_atomic(const char *filename, const int type, ...) {
    char tmp[PATH_MAX];
    const int tmp_len = snprintf(tmp, sizeof(tmp), "%s.tmp", filename);

    if (tmp_len < 0 || (size_t) tmp_len >= sizeof(tmp)) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, filename);
        return;
    }

    FILE *f = fopen(tmp, "w");
    if (!f) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, filename);
        return;
    }

    va_list args;
    va_start(args, type);

    int ok = 1;
    if (type == CHAR) {
        if (fputs(va_arg(args, const char *), f) < 0) ok = 0;
    } else if (type == INT) {
        if (fprintf(f, "%d", va_arg(args, int)) < 0) ok = 0;
    }

    va_end(args);

    if (fflush(f) != 0) ok = 0;
    if (fsync(fileno(f)) != 0) ok = 0;
    fclose(f);

    if (!ok || rename(tmp, filename) != 0) {
        remove(tmp);
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_write, filename);
    }
}

void create_directories(const char *path, const int parent_only) {
    struct stat st;
    char tmp_path[MAX_BUFFER_SIZE];

    if (!path || !*path) return;

    snprintf(tmp_path, sizeof(tmp_path), "%s", path);
    if (parent_only) {
        char *slash = strrchr(tmp_path, '/');
        if (!slash) return;
        *slash = '\0';
    }

    if (stat(tmp_path, &st) == 0 && S_ISDIR(st.st_mode)) return;

    // recursive bullshit
    char *slash = strrchr(tmp_path, '/');
    if (slash) {
        *slash = '\0';
        create_directories(tmp_path, 0);
        *slash = '/';
    }

    mkdir(tmp_path, 0777);
}

void increment_counter_file(const char *path) {
    create_directories(path, 1);
    write_text_to_file(path, "w", INT, read_line_int_from(path, 1) + 1);
}

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[], const int recursive) {
    struct dirent *entry;
    DIR *dir = opendir(dir_path);

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                const size_t len = strlen(extension);
                const size_t name_len = strlen(entry->d_name);

                if (name_len > len && strcasecmp(entry->d_name + name_len - len, extension) == 0) {

                    char file_path[PATH_MAX];
                    snprintf(file_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

                    int is_exception = 0;
                    if (exception != NULL) {
                        for (int i = 0; exception[i] != NULL; ++i) {
                            if (strcmp(entry->d_name, exception[i]) == 0) {
                                is_exception = 1;
                                break;
                            }
                        }
                    }

                    if (!is_exception) {
                        if (remove(file_path) != 0) {
                            perror(lang.system.fail_delete_file);
                        }
                    }
                }
            } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                if (recursive) {
                    char sub_dir_path[PATH_MAX];
                    snprintf(sub_dir_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
                    delete_files_of_type(sub_dir_path, extension, exception, recursive);
                }
            }
        }

        closedir(dir);
    } else {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
    }
}

void delete_files_of_name(const char *dir_path, const char *filename) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_dir_open, dir_path);
        return;
    }

    const int dfd = dirfd(dir);
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.'
            && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
            struct stat st;
            if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode)) {
                delete_files_of_name(full_path, filename);
                continue;
            }
        }

        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) continue;

        char base[NAME_MAX + 1];
        snprintf(base, sizeof(base), "%s", entry->d_name);

        char *dot = strrchr(base, '.');
        if (!dot) continue;

        *dot = '\0';

        if (strcmp(base, filename) == 0) {
            if (remove(full_path) != 0) {
                perror(lang.system.fail_delete_file);
            }
        }
    }

    closedir(dir);
}

static void add_directory_to_list(char ***list, size_t *size, size_t *count, const char *dir) {
    if (*count >= *size) {
        const size_t new_size = *size + 10;

        char **new_list = realloc(*list, new_size * sizeof(char *));
        if (!new_list) return;

        *list = new_list;
        *size = new_size;
    }

    (*list)[*count] = strdup(dir);
    if (!(*list)[*count]) return;

    (*count)++;
}

static void
collect_subdirectories(const char *base_dir, char ***list, size_t *size, size_t *count, const size_t trim_start_count) {
    char subdir_path[PATH_MAX];
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return;
    }

    load_skip_patterns();

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            const char *name = entry->d_name;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            if (should_skip(name, 1)) continue;

            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", base_dir, name);
            const char *trimmed_path = subdir_path + trim_start_count;

            add_directory_to_list(list, size, count, trimmed_path);
            collect_subdirectories(subdir_path, list, size, count, trim_start_count);
        }
    }

    closedir(dir);
}

char **get_subdirectories(const char *base_dir) {
    const size_t trim_start_count = strlen(base_dir) + 1;
    size_t list_size = 10;
    size_t count = 0;

    char **subdir_list = malloc(list_size * sizeof(char *));
    if (!subdir_list) return NULL;

    collect_subdirectories(base_dir, &subdir_list, &list_size, &count, trim_start_count);
    subdir_list[count] = NULL;

    return subdir_list;
}

void free_subdirectories(char **dir_names) {
    if (dir_names == NULL) return;

    for (int i = 0; dir_names[i] != NULL; i++)
        free(dir_names[i]);

    free(dir_names);
}

int cfg_read_int(const char *path, const int fallback) {
    FILE *f = fopen(path, "r");
    if (!f) return fallback;

    char buf[32];
    const int ok = fgets(buf, sizeof(buf), f) != NULL;
    fclose(f);

    if (!ok) return fallback;
    buf[strcspn(buf, "\n")] = '\0';

    return safe_atoi(buf, fallback);
}

char **str_parse_file(const char *filename, int *count, const enum parse_mode mode) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_open, filename);
        return NULL;
    }

    char **list = malloc(MAX_BUFFER_SIZE * sizeof(char *));
    if (!list) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        fclose(file);
        return NULL;
    }

    *count = 0;
    char line[MAX_BUFFER_SIZE];
    int failed = 0;

    if (mode == parse_tokens) {
        if (fgets(line, sizeof(line), file)) {
            char *saveptr;
            const char *token = strtok_r(line, " \t\r\n", &saveptr);
            while (token && *count < MAX_BUFFER_SIZE) {
                list[*count] = strdup(token);

                if (!list[*count]) {
                    failed = 1;
                    break;
                }

                (*count)++;
                token = strtok_r(NULL, " \t\r\n", &saveptr);
            }
        }
    } else {
        while (fgets(line, sizeof(line), file) && *count < MAX_BUFFER_SIZE) {
            char *end = strpbrk(line, "\r\n");
            if (end) *end = '\0';
            if (*line == '\0') continue;

            list[*count] = strdup(line);
            if (!list[*count]) {
                failed = 1;
                break;
            }

            (*count)++;
        }
    }

    fclose(file);

    if (failed) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        for (int i = 0; i < *count; i++)
            free(list[i]);
        free(list);
        return NULL;
    }

    return list;
}

int is_partition_mounted(const char *partition) {
    if (strcmp(partition, "/") == 0) return 1; // this is rootfs so I mean it should always be mounted

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        perror("fopen /proc/mounts");
        return 0;
    }

    char line[MAX_BUFFER_SIZE];
    int mounted = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, partition)) {
            mounted = 1;
            break;
        }
    }

    fclose(fp);
    return mounted;
}

void get_storage_info(const char *partition, double *total, double *free, double *used) {
    struct statvfs stat;

    if (!is_partition_mounted(partition)) {
        *total = 0.0;
        *free = 0.0;
        *used = 0.0;
        return;
    }

    if (statvfs(partition, &stat) != 0) {
        perror("statvfs");
        *total = 0.0;
        *free = 0.0;
        *used = 0.0;
        return;
    }

    *total = (double) (stat.f_blocks * stat.f_frsize) / (1024 * 1024 * 1024);
    *free = (double) (stat.f_bavail * stat.f_frsize) / (1024 * 1024 * 1024);
    *used = *total - *free;
}

int copy_file(const char *from, const char *to) {
    int fd_to = -1;
    int fd_from = -1;
    int saved_errno = 0;

    struct stat st;

    fd_from = open(from, O_RDONLY | O_CLOEXEC);
    if (fd_from < 0) return -1;

    if (fstat(fd_from, &st) < 0) goto out_error;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, st.st_mode & 0777);
    if (fd_to < 0) goto out_error;

    char buf[65536];
    ssize_t f_read;
    while (1) {
        f_read = read(fd_from, buf, sizeof buf);

        if (f_read == 0) break;

        if (f_read < 0) {
            if (errno == EINTR) continue;
            goto out_error;
        }

        size_t off = 0;
        while (off < (size_t) f_read) {
            const ssize_t nw = write(fd_to, buf + off, (size_t) f_read - off);

            if (nw < 0) {
                if (errno == EINTR) continue;
                goto out_error;
            }

            off += (size_t) nw;
        }
    }

    if (fsync(fd_to) < 0) goto out_error;

    if (fchmod(fd_to, st.st_mode & 0777) < 0) goto out_error;

    if (close(fd_to) < 0) {
        fd_to = -1;
        goto out_error;
    }

    close(fd_from);
    return 0;

out_error:
    saved_errno = errno;

    if (fd_from >= 0) close(fd_from);

    if (fd_to >= 0) {
        close(fd_to);
        unlink(to);
    }

    errno = saved_errno;
    return -1;
}

int remove_directory_recursive(const char *path) {
    struct dirent *entry;
    DIR *dp = opendir(path);

    if (dp == NULL) {
        perror("opendir");
        return -1;
    }

    const int dfd = dirfd(dp);

    while ((entry = readdir(dp)) != NULL) {
        char fullpath[4096];
        struct stat statbuf;

        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        if (fstatat(dfd, entry->d_name, &statbuf, AT_SYMLINK_NOFOLLOW) == -1) {
            perror("fstatat");
            closedir(dp);
            return -1;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Recursively delete subdirectory
            if (remove_directory_recursive(fullpath) == -1) {
                closedir(dp);
                return -1;
            }
        } else {
            // Delete file or symlink
            if (unlink(fullpath) == -1) {
                perror("unlink");
                closedir(dp);
                return -1;
            }
        }
    }

    closedir(dp);

    // Directory should now be empty
    if (rmdir(path) == -1) {
        perror("rmdir");
        return -1;
    }

    return 0;
}

char *get_script_value(const char *filename, const char *key, const char *not_found) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file!");
        return strdup("");
    }

    char line[MAX_BUFFER_SIZE];
    char search_key[MAX_BUFFER_SIZE];
    snprintf(search_key, sizeof(search_key), "# %s: ", key);

    char *value = NULL;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, search_key, strlen(search_key)) == 0) {
            value = strdup(line + strlen(search_key));
            if (value) value[strcspn(value, "\n")] = 0;
            break;
        }
    }

    fclose(file);

    if (value == NULL || value[0] == '\0') value = strdup(not_found);
    return value;
}

int at_base(const char *sys_dir, const char *base_name) {
    const char *base_dir = strrchr(sys_dir, '/');
    return base_dir && strcasecmp(base_dir + 1, base_name) == 0 ? 1 : 0;
}

int search_for_config(const char *base_path, const char *file_name, const char *system_name) {
    struct dirent *entry;
    char full_path[PATH_MAX];
    DIR *dir = opendir(base_path);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.'
            && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        if (entry->d_type == DT_REG) {
            if (strstr(entry->d_name, file_name)) {
                const char *line = read_line_char_from(full_path, 2);
                if (line && strcmp(line, system_name) == 0) {
                    closedir(dir);
                    return 1;
                }
            }
        } else if (entry->d_type == DT_DIR) {
            if (search_for_config(full_path, file_name, system_name)) {
                closedir(dir);
                return 1;
            }
        }
    }

    closedir(dir);
    return 0;
}

int scan_directory_list(
    const char *dirs[], const char *exts[], char ***results, const size_t dir_count, const size_t ext_count,
    size_t *result_count
) {
    size_t count = 0;
    size_t capacity = 0;
    char **list = NULL;

    for (size_t d = 0; d < dir_count; ++d) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;

        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (ent->d_type != DT_REG) continue;

            const char *dot = strrchr(ent->d_name, '.');
            if (!dot) continue;

            int match = 0;
            for (size_t e = 0; e < ext_count; ++e) {
                if (strcasecmp(dot, exts[e]) == 0) {
                    match = 1;
                    break;
                }
            }

            if (!match) continue;

            if (count >= capacity) {
                const size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                char **tmp = realloc(list, new_capacity * sizeof(char *));

                if (!tmp) {
                    LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
                    free_array(list, count);
                    closedir(dir);
                    return -1;
                }

                memset(tmp + capacity, 0, (new_capacity - capacity) * sizeof(char *));

                list = tmp;
                capacity = new_capacity;
            }

            char full[MAX_BUFFER_SIZE];
            snprintf(full, sizeof(full), "%s/%s", dirs[d], ent->d_name);

            list[count] = strdup(full);
            if (!list[count]) {
                LOG_ERROR(mux_module, "%s", lang.system.fail_dup_string);
                free_array(list, count);
                closedir(dir);
                return -1;
            }

            count++;
        }

        closedir(dir);
    }

    if (count == 0) {
        free(list);
        list = NULL;
    }

    *results = list;
    *result_count = count;

    return 0;
}
