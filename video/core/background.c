#include "background.h"
#include "audio.h"
#include "audio_source.h"
#include "equaliser.h"
#include "remote.h"
#include "state.h"
#include "../content/library.h"
#include "../settings/session.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>

#include <module/muxshare.h>
#include <common/platform/audio.h>
#include <common/platform/soundfont.h>
#include <common/storage/fileio.h>

#define BACKGROUND_RING_SECONDS  2
#define BACKGROUND_CONVERT       16384
#define BACKGROUND_HISTORY_MS    30000U
#define BACKGROUND_REMOTE_MS     1000U
#define BACKGROUND_IDLE_MS       10
#define BACKGROUND_TAKEOVER_MS   3000U
#define BACKGROUND_IDLE_CHECK_MS 500U
#define BACKGROUND_IDLE_STATE    RUN_PATH "idle_state"

typedef enum { track_finished = 0, track_next, track_previous, track_jump, track_stopped, track_failed } track_result;

typedef struct {
    float *ring;
    int capacity;
    int read;
    int write;
    int rate;
    int channels;
    double origin;
    double played;
    float filter_state[8];
    float filter_input[8];
} background_audio;

static volatile sig_atomic_t terminate_requested;
static SDL_atomic_t paused;
static background_audio output;
static char hook_owner;
static video_library_entry *entries;
static size_t entry_count;
static size_t current;
static size_t jump_target;
static double position;
static double duration;
static char current_uri[PATH_MAX];
static char current_title[PATH_MAX];
static char source_uri[PATH_MAX];
static wasabi_audio_info information;
static uint32_t remote_deadline;
static uint32_t history_deadline;
static uint32_t idle_deadline;
static int idle_paused;

static void handle_terminate(const int signal_number __attribute__((unused))) {
    terminate_requested = 1;
}

static void close_inherited_descriptors(void) {
    DIR *folder = opendir("/proc/self/fd");
    if (!folder) {
        for (int descriptor = 3; descriptor < 1024; descriptor++)
            close(descriptor);
        return;
    }
    const int own = dirfd(folder);
    int descriptors[1024];
    int count = 0;
    const struct dirent *entry;
    while ((entry = readdir(folder)) && count < 1024) {
        const int descriptor = atoi(entry->d_name);
        if (descriptor > 2 && descriptor != own) descriptors[count++] = descriptor;
    }
    closedir(folder);
    for (int index = 0; index < count; index++)
        close(descriptors[index]);
}

static int process_alive(const int pid) {
    return pid > 0 && (kill(pid, 0) == 0 || errno == EPERM);
}

int wasabi_background_launch(const char *source, const size_t index, const char *track, const double position_start) {
    if (!source || !source[0]) return 0;

    char executable[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length <= 0) return 0;
    executable[length] = '\0';

    char start[32];
    char selected[32];
    snprintf(start, sizeof(start), "%.3f", position_start > 0.0 ? position_start : 0.0);
    snprintf(selected, sizeof(selected), "%zu", index);

    const pid_t child = fork();
    if (child < 0) return 0;
    if (child > 0) {
        int status = 0;
        waitpid(child, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    setsid();
    const pid_t grandchild = fork();
    if (grandchild != 0) _exit(grandchild > 0 ? 0 : 1);

    close_inherited_descriptors();
    const int null = open("/dev/null", O_RDWR);
    if (null >= 0) {
        dup2(null, STDIN_FILENO);
        dup2(null, STDOUT_FILENO);
        if (null > STDERR_FILENO) close(null);
    }
    execl(
        executable, executable, "--background", "--start", start, "--index", selected, "--track",
        track && track[0] ? track : source, source, (char *) NULL
    );
    _exit(127);
}

int wasabi_background_takeover(
    char *track, const size_t track_size, double *position_out, char *source, const size_t source_size
) {
    if (track && track_size) track[0] = '\0';
    if (source && source_size) source[0] = '\0';
    if (position_out) *position_out = 0.0;

    const int pid = read_line_int_from(WASABI_BACKGROUND_PID, 1);
    if (!process_alive(pid) || pid == (int) getpid()) {
        remove(WASABI_BACKGROUND_PID);
        return 0;
    }

    remove(WASABI_BACKGROUND_STATE);
    kill(pid, SIGTERM);
    const uint32_t started = SDL_GetTicks();
    while (process_alive(pid) && SDL_GetTicks() - started < BACKGROUND_TAKEOVER_MS)
        usleep(20000);
    if (process_alive(pid)) kill(pid, SIGKILL);
    remove(WASABI_BACKGROUND_PID);

    char *line = read_line_char_from(WASABI_BACKGROUND_STATE, 1);
    if (line && source) snprintf(source, source_size, "%s", line);
    free(line);
    line = read_line_char_from(WASABI_BACKGROUND_STATE, 2);
    if (line && track) snprintf(track, track_size, "%s", line);
    free(line);
    line = read_line_char_from(WASABI_BACKGROUND_STATE, 3);
    if (line && position_out) *position_out = strtod(line, NULL);
    free(line);
    remove(WASABI_BACKGROUND_STATE);
    return 1;
}

static int ring_available(void) {
    if (output.capacity <= 0) return 0;
    return (output.write - output.read + output.capacity) % output.capacity;
}

static int ring_space(void) {
    return output.capacity > 0 ? output.capacity - 1 - ring_available() : 0;
}

static void ring_reset(const double origin) {
    SDL_LockAudio();
    output.read = output.write = 0;
    output.origin = origin;
    output.played = 0.0;
    SDL_UnlockAudio();
}

static void ring_write(const float *samples, int frames) {
    SDL_LockAudio();
    const int space = ring_space();
    if (frames > space) frames = space;
    if (frames > 0) {
        const size_t frame_bytes = (size_t) output.channels * sizeof(float);
        const int first = frames < output.capacity - output.write ? frames : output.capacity - output.write;
        memcpy(output.ring + (size_t) output.write * output.channels, samples, (size_t) first * frame_bytes);
        memcpy(output.ring, samples + (size_t) first * output.channels, (size_t) (frames - first) * frame_bytes);
        output.write = (output.write + frames) % output.capacity;
    }
    SDL_UnlockAudio();
}

static float clamp_sample(const float sample) {
    if (sample > 1.0f) return 1.0f;
    if (sample < -1.0f) return -1.0f;
    return sample;
}

static void apply_output_effects(float *samples, const int frames) {
    const float volume = (float) config.video.volume / 100.0f;
    const int channels = output.channels < 8 ? output.channels : 8;
    const int filter = config.video.audio_filter;
    for (int frame = 0; frame < frames; frame++) {
        for (int channel = 0; channel < channels; channel++) {
            float sample = clamp_sample(samples[(size_t) frame * output.channels + channel] * volume);
            if (filter == 1) {
                output.filter_state[channel] += 0.15f * (sample - output.filter_state[channel]);
                sample = output.filter_state[channel];
            } else if (filter == 2) {
                const float filtered = 0.90f * (output.filter_state[channel] + sample - output.filter_input[channel]);
                output.filter_input[channel] = sample;
                output.filter_state[channel] = filtered;
                sample = filtered;
            }
            samples[(size_t) frame * output.channels + channel] = sample;
        }
    }
    wasabi_eq_process(samples, frames, output.channels, output.rate);
}

static void background_callback(void *unused __attribute__((unused)), Uint8 *stream, const int length) {
    const int bytes_per_frame = output.channels * (int) sizeof(float);
    SDL_memset(stream, 0, (size_t) length);
    if (bytes_per_frame <= 0 || SDL_AtomicGet(&paused)) return;

    const int requested = length / bytes_per_frame;
    int available = ring_available();
    if (available > requested) available = requested;
    float *samples = (float *) stream;
    if (available > 0) {
        const int first = available < output.capacity - output.read ? available : output.capacity - output.read;
        memcpy(
            samples, output.ring + (size_t) output.read * output.channels, (size_t) first * (size_t) bytes_per_frame
        );
        memcpy(
            samples + (size_t) first * output.channels, output.ring,
            (size_t) (available - first) * (size_t) bytes_per_frame
        );
        output.read = (output.read + available) % output.capacity;
    }
    output.played += available;
    if (available > 0) apply_output_effects(samples, available);
}

static void sequenced_postmix(void *unused __attribute__((unused)), Uint8 *stream, const int length) {
    const int bytes_per_frame = output.channels * (int) sizeof(float);
    if (bytes_per_frame <= 0 || length < bytes_per_frame) return;
    const int frames = length / bytes_per_frame;
    if (config.video.volume > 100) {
        const float gain = (float) config.video.volume / 100.0f;
        float *samples = (float *) stream;
        for (size_t sample = 0; sample < (size_t) frames * (size_t) output.channels; sample++)
            samples[sample] = clamp_sample(samples[sample] * gain);
    }
    wasabi_eq_process((float *) stream, frames, output.channels, output.rate);
}

static double playback_position(void) {
    SDL_LockAudio();
    const double value = output.origin + (output.rate > 0 ? output.played / output.rate : 0.0);
    SDL_UnlockAudio();
    return value;
}

static void save_progress(const int complete) {
    if (!config.video.keep_history || !current_uri[0] || position < 1.0 || !audio_source_resumable(current_uri)) return;
    video_history_update(current_uri, current_title, position, duration, complete, "", NULL, 0);
}

static void announce_track(void) {
    static unsigned serial;
    char temporary[] = WASABI_BACKGROUND_TRACK ".tmp";
    FILE *file = fopen(temporary, "w");
    if (!file) return;
    fprintf(
        file, "%u\n%s\n%s\n", ++serial, information.title[0] ? information.title : current_title, information.artist
    );
    fclose(file);
    rename(temporary, WASABI_BACKGROUND_TRACK);
}

static void publish(const int force) {
    const uint32_t now = SDL_GetTicks();
    if (!force && remote_deadline && !SDL_TICKS_PASSED(now, remote_deadline)) return;
    remote_deadline = now + BACKGROUND_REMOTE_MS;
    const wasabi_remote_state state = {
        .title = information.title[0] ? information.title : current_title,
        .artist = information.artist,
        .album = information.album,
        .live = 0,
        .audio = 1,
        .paused = SDL_AtomicGet(&paused),
        .can_seek = duration > 0.0,
        .index = (int) current,
        .count = (int) entry_count,
        .channels = 0,
        .volume = config.video.volume,
        .shuffle = config.video.shuffle,
        .repeat = config.video.repeat_mode,
        .uri = current_uri,
        .position = position,
        .duration = duration,
    };
    wasabi_remote_publish(&state, force);
    if (wasabi_remote_list_wanted()) wasabi_remote_publish_list(entries, entry_count);
}

static int handle_command(const wasabi_remote_command *command, double *seek_to) {
    if (command->type == wasabi_remote_toggle || command->type == wasabi_remote_play
        || command->type == wasabi_remote_pause)
        idle_paused = 0;
    switch (command->type) {
        case wasabi_remote_toggle:
            SDL_AtomicSet(&paused, !SDL_AtomicGet(&paused));
            break;
        case wasabi_remote_play:
            SDL_AtomicSet(&paused, 0);
            break;
        case wasabi_remote_pause:
            SDL_AtomicSet(&paused, 1);
            break;
        case wasabi_remote_seek:
            *seek_to = command->value;
            break;
        case wasabi_remote_skip:
            *seek_to = position + command->value;
            break;
        case wasabi_remote_next:
            return track_next;
        case wasabi_remote_previous:
            if (position > 3.0) {
                *seek_to = 0.0;
                break;
            }
            return track_previous;
        case wasabi_remote_volume: {
            int volume = config.video.volume + (int) command->value;
            config.video.volume = (int16_t) (volume < 0 ? 0 : volume > 200 ? 200 : volume);
            break;
        }
        case wasabi_remote_set_volume: {
            const int volume = (int) command->value;
            config.video.volume = (int16_t) (volume < 0 ? 0 : volume > 200 ? 200 : volume);
            break;
        }
        case wasabi_remote_stop:
            return track_stopped;
        case wasabi_remote_jump:
            if (command->value >= 0.0 && (size_t) command->value < entry_count) {
                jump_target = (size_t) command->value;
                return track_jump;
            }
            break;
        case wasabi_remote_shuffle:
            config.video.shuffle = (int16_t) !config.video.shuffle;
            break;
        case wasabi_remote_repeat:
            config.video.repeat_mode = (int16_t) ((config.video.repeat_mode + 1) % 3);
            break;
        default:
            break;
    }
    publish(1);
    return -1;
}

static void idle_tick(void) {
    const uint32_t now = SDL_GetTicks();
    if (idle_deadline && !SDL_TICKS_PASSED(now, idle_deadline)) return;
    idle_deadline = now + BACKGROUND_IDLE_CHECK_MS;

    const int idle = config.settings.power.idle.mute && read_line_int_from(BACKGROUND_IDLE_STATE, 1) == 1;
    if (idle && !idle_paused && !SDL_AtomicGet(&paused)) {
        idle_paused = 1;
        SDL_AtomicSet(&paused, 1);
        publish(1);
    } else if (!idle && idle_paused) {
        idle_paused = 0;
        SDL_AtomicSet(&paused, 0);
        publish(1);
    }
}

static int poll_commands(double *seek_to) {
    if (terminate_requested) return track_stopped;
    idle_tick();
    wasabi_remote_command command;
    while (wasabi_remote_poll(&command)) {
        const int result = handle_command(&command, seek_to);
        if (result >= 0) return result;
    }
    return -1;
}

static void tick_history(void) {
    const uint32_t now = SDL_GetTicks();
    if (!history_deadline) history_deadline = now + BACKGROUND_HISTORY_MS;
    if (!SDL_TICKS_PASSED(now, history_deadline)) return;
    history_deadline = now + BACKGROUND_HISTORY_MS;
    if (!SDL_AtomicGet(&paused)) save_progress(0);
}

static track_result play_sequenced(const char *uri, const double start) {
    char soundfont[PATH_MAX];
    if (soundfont_resolve(config.settings.general.soundfont, soundfont, sizeof(soundfont)))
        Mix_SetSoundFonts(soundfont);
    Mix_Music *music = Mix_LoadMUS(uri);
    if (!music) return track_failed;
    duration = Mix_MusicDuration(music);
    if (duration < 0.0) duration = 0.0;
    const int volume = config.video.volume < 100 ? config.video.volume : 100;
    Mix_VolumeMusic(volume * MIX_MAX_VOLUME / 100);
    Mix_SetPostMix(sequenced_postmix, NULL);
    if (Mix_PlayMusic(music, 0) < 0) {
        Mix_SetPostMix(NULL, NULL);
        Mix_FreeMusic(music);
        return track_failed;
    }
    if (start > 0.0 && (duration <= 0.0 || start < duration)) Mix_SetMusicPosition(start);
    wasabi_audio_cleanup(&information);
    wasabi_audio_metadata(NULL, uri, current_title, &information);
    announce_track();

    track_result result = track_finished;
    int was_paused = 0;
    for (;;) {
        double seek_to = -1.0;
        const int command = poll_commands(&seek_to);
        if (command >= 0) {
            result = (track_result) command;
            break;
        }
        const int now_paused = SDL_AtomicGet(&paused);
        if (now_paused != was_paused) {
            if (now_paused)
                Mix_PauseMusic();
            else
                Mix_ResumeMusic();
            was_paused = now_paused;
        }
        if (seek_to >= 0.0) Mix_SetMusicPosition(seek_to);
        const double value = Mix_GetMusicPosition(music);
        if (value >= 0.0) position = value;
        if (!now_paused && !Mix_PlayingMusic()) break;
        publish(0);
        tick_history();
        SDL_Delay(BACKGROUND_IDLE_MS * 5);
    }
    Mix_HaltMusic();
    Mix_SetPostMix(NULL, NULL);
    Mix_FreeMusic(music);
    return result;
}

static track_result play_stream(const char *uri, const double start) {
    AVFormatContext *format = avformat_alloc_context();
    if (!format) return track_failed;
    AVDictionary *options = NULL;
    if (audio_source_open(&format, uri, output.rate, &options) < 0) {
        av_dict_free(&options);
        return track_failed;
    }
    av_dict_free(&options);

    track_result result = track_failed;
    AVCodecContext *decoder = NULL;
    SwrContext *resample = NULL;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    float *converted = malloc((size_t) BACKGROUND_CONVERT * (size_t) output.channels * sizeof(float));
    if (!packet || !frame || !converted || avformat_find_stream_info(format, NULL) < 0) goto done;

    const int stream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (stream < 0) goto done;
    const AVStream *audio = format->streams[stream];
    const AVCodec *codec = avcodec_find_decoder(audio->codecpar->codec_id);
    decoder = codec ? avcodec_alloc_context3(codec) : NULL;
    if (!decoder || avcodec_parameters_to_context(decoder, audio->codecpar) < 0) goto done;
    decoder->pkt_timebase = audio->time_base;
    if (avcodec_open2(decoder, codec, NULL) < 0) goto done;

    duration = format->duration != AV_NOPTS_VALUE ? (double) format->duration / AV_TIME_BASE : 0.0;
    wasabi_audio_cleanup(&information);
    wasabi_audio_metadata(format, uri, current_title, &information);

    double origin = 0.0;
    if (start > 0.0 && (duration <= 0.0 || start < duration)) {
        int64_t target = (int64_t) (start * AV_TIME_BASE);
        if (format->start_time != AV_NOPTS_VALUE) target += format->start_time;
        if (avformat_seek_file(format, -1, INT64_MIN, target, INT64_MAX, AVSEEK_FLAG_BACKWARD) >= 0) origin = start;
    }
    ring_reset(origin);
    position = origin;
    announce_track();
    publish(1);

    AVChannelLayout layout;
    av_channel_layout_default(&layout, output.channels);
    int at_end = 0;
    result = track_finished;
    for (;;) {
        double seek_to = -1.0;
        const int command = poll_commands(&seek_to);
        if (command >= 0) {
            result = (track_result) command;
            break;
        }
        if (seek_to >= 0.0 && duration > 0.0) {
            if (seek_to > duration) seek_to = duration;
            int64_t target = (int64_t) (seek_to * AV_TIME_BASE);
            if (format->start_time != AV_NOPTS_VALUE) target += format->start_time;
            if (avformat_seek_file(format, -1, INT64_MIN, target, INT64_MAX, AVSEEK_FLAG_BACKWARD) >= 0) {
                avcodec_flush_buffers(decoder);
                ring_reset(seek_to);
                at_end = 0;
            }
        }

        position = playback_position();
        publish(0);
        tick_history();

        if (at_end) {
            SDL_LockAudio();
            const int remaining = ring_available();
            SDL_UnlockAudio();
            if (!remaining) break;
            SDL_Delay(BACKGROUND_IDLE_MS);
            continue;
        }

        SDL_LockAudio();
        const int space = ring_space();
        SDL_UnlockAudio();
        if (space < output.capacity / 2 || SDL_AtomicGet(&paused)) {
            SDL_Delay(BACKGROUND_IDLE_MS);
            continue;
        }

        const int read = av_read_frame(format, packet);
        if (read < 0) {
            avcodec_send_packet(decoder, NULL);
            at_end = 1;
        } else if (packet->stream_index != stream) {
            av_packet_unref(packet);
            continue;
        } else {
            avcodec_send_packet(decoder, packet);
            av_packet_unref(packet);
        }

        while (avcodec_receive_frame(decoder, frame) >= 0) {
            if (!resample) {
                if (swr_alloc_set_opts2(
                        &resample, &layout, AV_SAMPLE_FMT_FLT, output.rate, &frame->ch_layout,
                        (enum AVSampleFormat) frame->format, frame->sample_rate, 0, NULL
                    ) < 0
                    || swr_init(resample) < 0) {
                    swr_free(&resample);
                    av_frame_unref(frame);
                    continue;
                }
            }
            uint8_t *planes[1] = {(uint8_t *) converted};
            const int frames = swr_convert(
                resample, planes, BACKGROUND_CONVERT, (const uint8_t **) frame->extended_data, frame->nb_samples
            );
            if (frames > 0) ring_write(converted, frames);
            av_frame_unref(frame);
        }
    }
    av_channel_layout_uninit(&layout);

done:
    ring_reset(0.0);
    free(converted);
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&resample);
    avcodec_free_context(&decoder);
    avformat_close_input(&format);
    return result;
}

static size_t random_index(void) {
    if (entry_count < 2) return 0;
    size_t target = (size_t) (random() % (long) (entry_count - 1));
    if (target >= current) target++;
    return target;
}

static int advance(const track_result result) {
    switch (result) {
        case track_stopped:
            return 0;
        case track_jump:
            current = jump_target;
            return 1;
        case track_previous:
            current = current > 0 ? current - 1 : entry_count - 1;
            return 1;
        case track_next:
            current = config.video.shuffle ? random_index() : (current + 1) % entry_count;
            return 1;
        case track_failed:
        case track_finished:
            if (result == track_finished && config.video.repeat_mode == 1) return 1;
            if (config.video.shuffle && entry_count > 1) {
                if (!config.video.auto_play && config.video.repeat_mode != 2) return 0;
                current = random_index();
                return 1;
            }
            if (current + 1 < entry_count && (config.video.auto_play || result == track_failed)) {
                current++;
                return 1;
            }
            if (config.video.repeat_mode == 2 && entry_count > 1) {
                current = 0;
                return 1;
            }
            return 0;
    }
    return 0;
}

static int load_entries(const char *source, const size_t index, const char *track) {
    const int m3u = video_path_extension_is(source, ".m3u")
                    || (video_path_extension_is(source, ".m3u8") && video_playlist_probe(source) == video_playlist_m3u);
    if (m3u && video_playlist_load(source, 0, &entries, &entry_count) >= 0 && entry_count > 0) {
        current = index < entry_count ? index : 0;
        if (track && track[0] && strcmp(entries[current].uri, track) != 0)
            for (size_t item = 0; item < entry_count; item++)
                if (strcmp(entries[item].uri, track) == 0) {
                    current = item;
                    break;
                }
        return 1;
    }

    const char *file = track && track[0] ? track : source;
    if (!strstr(file, "://") && !strchr(file, '#')) {
        size_t selected = 0;
        if (video_folder_playlist(file, &entries, &entry_count, &selected) > 0 && entry_count > 0) {
            current = selected < entry_count ? selected : 0;
            return 1;
        }
    }

    entries = calloc(1, sizeof(*entries));
    if (!entries) return 0;
    entries[0].uri = strdup(file);
    char title[PATH_MAX];
    entries[0].title = strdup(video_title_from_uri(file, title, sizeof(title)));
    if (!entries[0].uri || !entries[0].title) {
        video_library_free(entries, 1);
        entries = NULL;
        return 0;
    }
    entry_count = 1;
    current = 0;
    return 1;
}

static void write_state(void) {
    FILE *file = fopen(WASABI_BACKGROUND_STATE, "w");
    if (!file) return;
    fprintf(file, "%s\n%s\n%.3f\n", source_uri, current_uri, position);
    fclose(file);
}

static int open_output(void) {
    static const int rates[] = {32000, 44100, 48000};
    static const int periods[] = {256, 512, 1024, 2048};
    for (int attempt = 0; attempt < 6; attempt++) {
        if (init_audio_backend_spec(rates[config.video.sample_rate], periods[config.video.audio_period])) break;
        usleep(100000U << attempt);
    }
    Uint16 format = 0;
    if (!Mix_QuerySpec(&output.rate, &format, &output.channels) || format != AUDIO_F32LSB || output.channels <= 0)
        return 0;
    output.capacity = output.rate * BACKGROUND_RING_SECONDS + 1;
    output.ring = calloc((size_t) output.capacity * (size_t) output.channels, sizeof(*output.ring));
    return output.ring != NULL;
}

int wasabi_background_run(const char *source, const size_t index, const char *track, const double start) {
    signal(SIGTERM, handle_terminate);
    signal(SIGINT, handle_terminate);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    snprintf(source_uri, sizeof(source_uri), "%s", source ? source : "");
    if (!load_entries(source_uri, index, track)) return EXIT_FAILURE;

    create_directories(WASABI_BACKGROUND_PID, 1);
    write_text_to_file(WASABI_BACKGROUND_PID, "w", INT, (int) getpid());

    wasabi_session_begin(entries[current].uri);
    if (read_line_int_from(IDLE_GAME_INHIBIT, 1) == (int) getpid()) remove(IDLE_GAME_INHIBIT);
    write_text_to_file(IDLE_SLEEP_INHIBIT, "w", INT, (int) getpid());
    wasabi_eq_changed();
    if (!open_output()) {
        Mix_CloseAudio();
        remove(WASABI_BACKGROUND_PID);
        wasabi_session_end();
        video_library_free(entries, entry_count);
        entries = NULL;
        entry_count = 0;
        return EXIT_FAILURE;
    }
    wasabi_remote_open();
    Mix_HookMusic(background_callback, &hook_owner);
    srandom((unsigned) SDL_GetTicks() ^ (unsigned) getpid());

    double resume = start;
    int failures = 0;
    for (;;) {
        snprintf(current_uri, sizeof(current_uri), "%s", entries[current].uri);
        snprintf(
            current_title, sizeof(current_title), "%s",
            entries[current].title && entries[current].title[0] ? entries[current].title : current_uri
        );
        position = resume;
        duration = 0.0;
        history_deadline = 0;

        track_result result;
        if (video_path_is_sequenced(current_uri)) {
            Mix_HookMusic(NULL, NULL);
            result = play_sequenced(current_uri, resume);
            Mix_HookMusic(background_callback, &hook_owner);
        } else {
            result = play_stream(current_uri, resume);
        }
        resume = 0.0;

        save_progress(result == track_finished);
        failures = result == track_failed ? failures + 1 : 0;
        if (failures >= (int) entry_count || failures >= 8) break;
        if (result == track_finished) position = duration;
        if (result == track_stopped || !advance(result)) break;
    }

    write_state();
    Mix_HookMusic(NULL, NULL);
    Mix_CloseAudio();
    wasabi_remote_close();
    wasabi_audio_cleanup(&information);
    wasabi_session_end();
    video_library_free(entries, entry_count);
    free(output.ring);
    if (read_line_int_from(WASABI_BACKGROUND_PID, 1) == (int) getpid()) {
        remove(WASABI_BACKGROUND_PID);
        remove(WASABI_BACKGROUND_TRACK);
    }
    return EXIT_SUCCESS;
}
