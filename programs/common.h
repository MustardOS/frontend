#pragma once

#include <alsa/asoundlib.h>
#include <sys/epoll.h>
#include <unistd.h>

int epoll_init(int fd);

int epoll_wait_events(int epoll_fd, struct epoll_event *events, int max_events);

snd_mixer_t *get_mixer_elem(const char *element, snd_mixer_elem_t **elem);

int get_volume(const char *element);

void set_volume(const char *element, int volume);

void set_speaker_power(const char *element, int state);

void save_volume(int volume);

void set_bl_power(int power);

void set_rumble_level(int level);

void set_governor(char *governor);

void set_cpu_scale(int speed);

void kill_process_by_name(char *process_name);

void setup_background_process();

int file_exist(char *filename);

char *read_text_from_file(char *filename);
