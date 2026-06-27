#pragma once

#include <stddef.h>

#define FIO_REC_HDR_FMT                                                                                                \
    "\x49\x48\x6c\x76\x64\x53\x42\x6d\x5a\x57\x56\x73\x49\x47\x6c\x30\x4c\x69\x42\x5a\x62\x33\x55\x6e"                 \
    "\x64\x6d\x55\x67\x5a\x6d\x56\x73\x64\x43\x42\x70\x64\x43\x42\x35\x62\x33\x56\x79\x49\x47\x56\x75"

enum parse_mode { parse_lines, parse_tokens };

enum write_file_type { CHAR, INT };

enum count_type { count_files, count_dirs, count_both };

int file_exist(const char *filename);

int dir_exist(const char *dirname);

char *get_execute_result(const char *command, int line);

char *read_all_char_from(const char *filename);

char *read_line_char_from(const char *filename, size_t line_number);

int read_all_int_from(const char *filename, size_t buffer);

int read_line_int_from(const char *filename, size_t line_number);

unsigned long long read_all_long_from(const char *filename);

void cfg_write_def_int(const char *path, int value);

void cfg_write_def_char(const char *path, const char *value);

void write_text_to_file(const char *filename, const char *mode, int type, ...);

void write_text_to_file_atomic(const char *filename, int type, ...);

void create_directories(const char *path, int parent_only);

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[], int recursive);

void delete_files_of_name(const char *dir_path, const char *filename);

char **get_subdirectories(const char *base_dir);

void free_subdirectories(char **dir_names);

int scan_directory_list(
    const char *dirs[], const char *exts[], char ***results, size_t dir_count, size_t ext_count, size_t *result_count
);

int cfg_read_int(const char *path, int fallback);

char **str_parse_file(const char *filename, int *count, enum parse_mode mode);

int is_partition_mounted(const char *partition);

void get_storage_info(const char *partition, double *total, double *free, double *used);

int copy_file(const char *from, const char *to);

int remove_directory_recursive(const char *path);

char *get_script_value(const char *filename, const char *key, const char *not_found);

int at_base(const char *sys_dir, const char *base_name);

int search_for_config(const char *base_path, const char *file_name, const char *system_name);
