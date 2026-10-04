#pragma once

typedef enum {
    wasabi_remote_none = 0,
    wasabi_remote_toggle,
    wasabi_remote_play,
    wasabi_remote_pause,
    wasabi_remote_seek,
    wasabi_remote_skip,
    wasabi_remote_next,
    wasabi_remote_previous,
    wasabi_remote_volume,
    wasabi_remote_stop
} wasabi_remote_type;

typedef struct {
    wasabi_remote_type type;
    double value;
} wasabi_remote_command;

typedef struct {
    const char *title;
    const char *artist;
    const char *album;
    int live;
    int audio;
    int paused;
    int can_seek;
    int index;
    int count;
    int channels;
    int volume;
    double position;
    double duration;
} wasabi_remote_state;

void wasabi_remote_open(void);
void wasabi_remote_close(void);
int wasabi_remote_poll(wasabi_remote_command *command);
void wasabi_remote_publish(const wasabi_remote_state *state, int force);
