#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "options.h"

int epoll_init(int fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("failed to create epoll instance");
        exit(1);
    }

    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("failed to add file descriptor to epoll");
        close(epoll_fd);
        exit(1);
    }

    return epoll_fd;
}

int epoll_wait_events(int epoll_fd, struct epoll_event *events, int max_events) {
    int num_events = epoll_wait(epoll_fd, events, max_events, -1);
    if (num_events == -1) {
        if (errno == EINTR) {
            return 0;
        } else {
            perror("failed to create epoll_wait for event");
            exit(1);
        }
    }

    return num_events;
}

snd_mixer_t *get_mixer_elem(const char *element, snd_mixer_elem_t **elem) {
    snd_mixer_t *mixer;
    snd_mixer_selem_id_t *sid;

    const char *card = "default";

    snd_mixer_open(&mixer, 0);
    snd_mixer_attach(mixer, card);
    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, element);

    *elem = snd_mixer_find_selem(mixer, sid);
    if (!*elem) {
        snd_mixer_close(mixer);
        return NULL;
    }

    return mixer;
}

int get_volume(const char *element) {
    snd_mixer_elem_t *elem;
    snd_mixer_t *mixer = get_mixer_elem(element, &elem);
    if (!mixer) {
        return -1;
    }

    long volume;
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &volume);

    long min, max;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

    long volume_range = max - min;
    int current_volume = (int) ((volume * VOL_MAX) / volume_range);

    snd_mixer_close(mixer);
    return current_volume;
}

void set_volume(const char *element, int volume) {
    snd_mixer_elem_t *elem;
    snd_mixer_t *mixer = get_mixer_elem(element, &elem);
    if (!mixer) {
        return;
    }

    long min, max;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

    long volume_range = max - min;
    long set_volume = (volume_range * volume) / VOL_MAX;
    snd_mixer_selem_set_playback_volume_all(elem, set_volume);

    snd_mixer_close(mixer);
}

void set_speaker_power(const char *element, int state) {
    snd_mixer_elem_t *elem;
    snd_mixer_t *mixer = get_mixer_elem(element, &elem);
    if (!mixer) {
        return;
    }

    int power = (state != 0) ? 1 : 0;
    snd_mixer_selem_set_playback_switch_all(elem, power);

    snd_mixer_close(mixer);
}

void save_volume(int volume) {
    FILE *file = fopen(VOL_RST_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%d", volume);
        fclose(file);
    } else {
        perror("Failed to open volume restore file");
        exit(1);
    }
}

void set_bl_power(int power) {
    FILE *file = fopen(BL_POWER_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%d", power);
        fclose(file);
    } else {
        perror("Failed to open bl_power file");
    }
}

void set_rumble_level(int level) {
    char data[4];

    snprintf(data, sizeof(data), "%d", level);
    FILE *file = fopen(RUMBLE_DEVICE, "w");

    if (file) {
        fwrite(data, sizeof(char), strlen(data), file);
        fclose(file);
    }
}

void set_governor(char *governor) {
    FILE *file = fopen(GOVERNOR_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%s", governor);
        fclose(file);
    } else {
        perror("Failed to open scaling_governor file");
        exit(1);
    }
}

void set_cpu_scale(int speed) {
    FILE *file = fopen(SCALE_MX_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%d", speed);
        fclose(file);
    } else {
        perror("Failed to open scaling_max_freq file");
        exit(1);
    }
}

void kill_process_by_name(char *process_name) {
    char command[256];
    snprintf(command, sizeof(command), "/bin/busybox killall %s", process_name);
    if (system(command) == -1) {
        perror("Error executing killall");
    }
}

void setup_background_process() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Failed to fork");
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

int file_exist(char *filename) {
    return access(filename, F_OK) == 0;
}

char *read_text_from_file(char *filename) {
    char *text = NULL;
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        return "";
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    text = (char *) malloc(fileSize + 1);

    if (text != NULL) {
        size_t bytesRead = fread(text, 1, fileSize, file);

        if (bytesRead > 0 && text[bytesRead - 1] == '\n') {
            text[bytesRead - 1] = '\0';
        } else {
            text[bytesRead] = '\0';
        }
    } else {
        perror("Error allocating memory for text");
    }

    fclose(file);
    return text;
}
