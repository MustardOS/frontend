#pragma once

#include <stddef.h>
#include "options.h"

struct pattern {
    char **patterns;
    size_t count;
    size_t capacity;
};

extern struct pattern skip_pattern_list;
extern int skip_patterns_loaded;

void load_skip_patterns(void);

int should_skip(const char *name, int is_dir);

int str_compare(const void *a, const void *b);

int str_startswith(const char *a, const char *b);

char *str_nonew(char *text);

char *str_tolower(char *text);

char *str_toupper(char *text);

char *str_remchar(char *text, char c);

char *str_remchars(char *text, char *c);

void str_split(char *text, char sep, char *p1, char *p2);

char *str_trim(char *text);

char *str_replace(const char *orig, const char *rep, const char *with);

int str_replace_segment(const char *orig, const char *prefix, const char *suffix,
                        const char *with, char **replacement);

int str_extract(const char *orig, const char *prefix, const char *suffix, char **extraction);

char *str_capital(char *text);

char *str_capital_all(char *text);

char *str_rem_first_char(char *text, int count);

char *str_rem_last_char(char *text, int count);

char *get_last_subdir(char *text, char separator, int n);

void remove_double_slashes(char *str);

char *get_last_dir(char *text);

char *get_file_name(char *text);

char *get_content_path(char *path);

char *get_content_name(char *path);

char *strip_dir(char *text);

char *strip_ext(char *text);

char *grab_ext(char *text);

void adjust_visual_label(char *text, int method, int rep_dash);

char *generate_number_string(int min, int max, int increment, const char *prefix, const char *infix,
                             const char *suffix, int infix_position);

char **split_command(const char *cmd, size_t *argc_out);

void free_array(char **array, size_t count);
