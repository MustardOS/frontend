#pragma once

#include <stddef.h>

typedef void (*exec_callback)(int exit_code);

const char **build_term_exec(const char **term_cmd, size_t *term_cnt);

void extract_archive(const char *filename, const char *screen);

void update_bootlogo(const char *next_screen);

void load_assign(const char *loader, const char *rom, const char *dir, const char *sys, int forced, int app);

void load_mux(const char *value);

void run_exec(const char *args[], size_t size, int background, int turbo, const char *log_file, exec_callback cb);

void exec_watch_task(void);

int set_scaling_governor(const char *governor, int show_done);

void turbo_time(int toggle, int show_done);

void set_process_name(const char *module);

const char *get_process_name(void);

const char *module_from_func(const char *func);
