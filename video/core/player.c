#include "player.h"
#include "audio.h"
#include "audio_transition.h"
#include "audio_source.h"
#include "../ui/ui_loading.h"
#include "../ui/ui_audio.h"
#include "ui_pause.h"
#include "video.h"
#include "state.h"
#include "../settings/assets.h"
#include "../settings/session.h"
#include "../settings/settings.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <curl/curl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <module/muxshare.h>
#include <common/display/screenshot.h>
#include <common/platform/audio.h>
#include <common/platform/soundfont.h>
#include <common/runtime/exec.h>
#include <common/storage/fileio.h>

#define VIDEO_QUEUE_SIZE                   120
#define LOCAL_VIDEO_QUEUE_SIZE             16
#define LIVE_VIDEO_QUEUE_SECONDS           4.0
#define LIVE_VIDEO_QUEUE_MEMORY            (96U * 1024U * 1024U)
#define LIVE_VIDEO_TARGET                  8
#define LIVE_AUDIO_BUFFER_HEADROOM_SECONDS 3
#define LIVE_AUDIO_UNDERRUN_GRACE          5
#define AUDIO_CONVERT_FRAMES               65536
#define AUDIO_SPEED_BATCH_FRAMES           1024
#define AUDIO_SPEED_PHASE_ONE              UINT64_C(4294967296)
#define SPEED_RAMP_MS                      350.0
#define PRESENT_INTERVAL_MIN_MS            4
#define PRESENT_INTERVAL_MAX_MS            12
#define PRESENT_INTERVAL_NORMAL_MS         16
#define PRESENT_INTERVAL_IDLE_MS           33
#define PRESENT_INTERVAL_BLEND_MS          2
#define PRESENT_EARLY_SECONDS              0.002
#define PLAYBACK_UI_INTERVAL_MS            33
#define CHANNEL_SWITCH_DELAY_MS            1000
#define LIVE_AUDIO_CLOCK_HARD_SECONDS      0.250
#define LIVE_AUDIO_CLOCK_SOFT_SECONDS      0.020
#define LIVE_AUDIO_CLOCK_STEP_SECONDS      0.001
#define LIVE_AUDIO_CLOCK_CORRECTION_FACTOR 0.125
#define HLS_MANIFEST_LIMIT                 (512U * 1024U)
#define LIVE_BUFFER_FAILURE_MS             15000U
#define LIVE_AUTOMATIC_BITRATE_LIMIT       4000000LL
#define PACKET_CACHE_LIMIT                 256

typedef struct packet_node {
    AVPacket *packet;
    size_t bytes;
    struct packet_node *next;
} packet_node;

typedef struct {
    int stream;
    int64_t bitrate;
    int width;
    int height;
} live_variant;

typedef struct {
    AVFormatContext *format;
    AVCodecContext *video_decoder;
    AVCodecContext *audio_decoder;
    AVPacket *packet;
    AVPacket *demux_packet;
    AVFrame *decode_frame;
    AVFrame *present_frame;
    AVFrame *blend_frame;
    AVFrame *filter_frame;
    AVFrame *video_queue[VIDEO_QUEUE_SIZE];
    int video_head;
    int video_tail;
    int video_count;
    int video_queue_limit;
    int video_stream;
    int audio_stream;
    AVRational video_time_base;
    AVRational audio_time_base;
    SDL_Thread *decode_thread;
    SDL_Thread *demux_thread;
    SDL_mutex *lock;
    SDL_cond *condition;
    SwrContext *resample;
    AVFilterGraph *filter_graph;
    AVFilterContext *filter_source;
    AVFilterContext *filter_sink;
    int filter_width;
    int filter_height;
    int filter_format;
    float *audio_ring;
    float *audio_convert;
    float *audio_speed_convert;
    float *audio_speed_previous;
    float *audio_speed_filter;
    uint64_t audio_speed_phase;
    int audio_speed_primed;
    int audio_capacity;
    int audio_read;
    int audio_write;
    int live_audio_underruns;
    int audio_rate;
    int audio_channels;
    double audio_frames_played;
    double audio_origin;
    int audio_clock_valid;
    double audio_callback_position;
    double audio_callback_duration;
    double audio_callback_rate;
    uint32_t audio_callback_ticks;
    SDL_atomic_t stop;
    SDL_atomic_t paused;
    SDL_atomic_t eof;
    SDL_atomic_t buffering;
    int seek_pending;
    double seek_target;
    double position;
    double duration;
    double clock_origin;
    double clock_speed;
    uint32_t clock_ticks;
    SDL_atomic_t interrupt_ticks;
    int history_enabled;
    int deinterlace;
    int live;
    packet_node *packet_head;
    packet_node *packet_tail;
    packet_node *packet_free;
    int packet_free_count;
    size_t packet_bytes;
    size_t packet_limit;
    int demux_eof;
    live_variant *live_variants;
    size_t live_variant_count;
    int64_t live_selected_bitrate;
    int opened_live_quality;
    int opened_live_buffer;
    int live_manifest_resolved;
    int restart_requested;
    int completed;
    char title[PATH_MAX];
    char uri[PATH_MAX];
    char container_uri[PATH_MAX];
    uint32_t history_save_deadline;
    double last_history_position;
    lv_timer_t *present_timer;
    uint32_t present_interval;
    double presented_timestamp;
    double blend_timestamp;
    int frame_blend;
    uint64_t blend_present_deadline;
    uint64_t blend_present_interval;
    int present_dirty;
    uint32_t ui_tick_deadline;
    int ui_ready;
    int ui_input_consumed;
    int menu_resume_playback;
    SDL_atomic_t filter_revision;
    int filter_applied_revision;
    uint32_t image_preview_deadline;
    int live_preview_pending;
    const video_library_entry *playlist;
    size_t playlist_count;
    size_t playlist_index;
    int playlist_channels;
    size_t *playlist_selection;
    int playlist_switch_requested;
    int folder_playlist;
    const video_library_entry *folder;
    size_t folder_count;
    size_t folder_index;
    size_t *folder_selection;
    int folder_switch_requested;
    size_t channel_pending_index;
    uint32_t channel_switch_deadline;
    int channel_switch_pending;
    int content_switch_requested;
    int buffering_visible;
    int live_failed;
    uint32_t buffering_started_at;
    float audio_filter_state[8];
    float audio_filter_input[8];
    int audio_only;
    int tracker_audio;
    int fast_forward_active;
    int slow_motion_active;
    double speed_current;
    double speed_start;
    double speed_target;
    uint32_t speed_ramp_started;
    int speed_ramping;
    SDL_SpinLock speed_lock;
    SDL_atomic_t speed_q16;
    double transition_resume_origin;
    wasabi_audio_info audio_information;
    Mix_Music *sequenced_audio;
} video_player;

static video_player player;
static int network_ready;
typedef struct {
    video_library_entry *entries;
    size_t count;
    size_t selected;
    char source[PATH_MAX];
} subsong_playlist;

static subsong_playlist subsongs;
static char audio_hook_owner;
static int video_codec_unsupported;
static unsigned char *shuffle_seen;
static size_t shuffle_seen_count;
static char shuffle_identity[PATH_MAX];
static uint32_t shuffle_random_state;

static int live_audio_target_seconds(void) {
    if (config.wasabi.live_buffer >= 32) return 8;
    if (config.wasabi.live_buffer >= 16) return 6;
    if (config.wasabi.live_buffer >= 8) return 4;
    return 2;
}

static int live_video_queue_limit(void) {
    if (!player.live || !player.format || player.video_stream < 0) return LOCAL_VIDEO_QUEUE_SIZE;
    AVStream *stream = player.format->streams[player.video_stream];
    const AVRational rate = av_guess_frame_rate(player.format, stream, NULL);
    const double frames_per_second = rate.num > 0 && rate.den > 0 ? av_q2d(rate) : 30.0;
    const int width = player.video_decoder ? player.video_decoder->width : stream->codecpar->width;
    const int height = player.video_decoder ? player.video_decoder->height : stream->codecpar->height;
    int limit = (int) ceil(frames_per_second * LIVE_VIDEO_QUEUE_SECONDS);
    if (width > 0 && height > 0) {
        const size_t frame_bytes = (size_t) width * (size_t) height * 3U / 2U;
        const int memory_limit = frame_bytes ? (int) (LIVE_VIDEO_QUEUE_MEMORY / frame_bytes) : VIDEO_QUEUE_SIZE;
        if (limit > memory_limit) limit = memory_limit;
    }
    if (limit < LIVE_VIDEO_TARGET) limit = LIVE_VIDEO_TARGET;
    if (limit > VIDEO_QUEUE_SIZE) limit = VIDEO_QUEUE_SIZE;
    return limit;
}

static int live_performance_begin(char *previous, const size_t size) {
    if (!previous || size < 2 || !device.cpu.governor[0]) return 0;
    FILE *file = fopen(device.cpu.governor, "r");
    if (!file) return 0;
    const int read = fscanf(file, "%63s", previous);
    fclose(file);
    if (read != 1 || !previous[0] || strcmp(previous, "performance") == 0) return 0;
    if (set_scaling_governor("performance", 0) != 0) {
        previous[0] = '\0';
        return 0;
    }
    return 1;
}

static void live_performance_end(const int changed, const char *previous) {
    if (changed && previous && previous[0]) set_scaling_governor(previous, 0);
}

static int idle_saver_suppressed(void) {
    return !config.video.idle_screensaver;
}

static void request_seek_to_mode(double target, int show_position);
static void resume_playback(void);
static void stop_playback(void);
static void switch_playlist(size_t index);
static void switch_folder(size_t index);
static int audio_available(void);
static void set_fast_forward(int active);
static void set_slow_motion(int active);
static void start_audio_transition(void);

static double playback_speed_locked(const uint32_t now) {
    if (player.live || player.sequenced_audio) return 1.0;
    if (!player.speed_ramping) {
        const double speed = player.speed_current > 0.0 ? player.speed_current : 1.0;
        SDL_AtomicSet(&player.speed_q16, (int) llround(speed * 65536.0));
        return speed;
    }
    const double elapsed = (double) (now - player.speed_ramp_started);
    if (elapsed >= SPEED_RAMP_MS) {
        player.speed_current = player.speed_target;
        player.speed_ramping = 0;
        SDL_AtomicSet(&player.speed_q16, (int) llround(player.speed_current * 65536.0));
        return player.speed_current;
    }
    const double progress = elapsed / SPEED_RAMP_MS;
    const double eased = progress * progress * (3.0 - 2.0 * progress);
    player.speed_current = player.speed_start + (player.speed_target - player.speed_start) * eased;
    SDL_AtomicSet(&player.speed_q16, (int) llround(player.speed_current * 65536.0));
    return player.speed_current;
}

static double playback_speed(void) {
    SDL_AtomicLock(&player.speed_lock);
    const double speed = playback_speed_locked(SDL_GetTicks());
    SDL_AtomicUnlock(&player.speed_lock);
    return speed;
}

static double audio_callback_speed(void) {
    const int value = SDL_AtomicGet(&player.speed_q16);
    return value > 0 ? (double) value / 65536.0 : 1.0;
}

static void set_speed_target(const double target) {
    SDL_AtomicLock(&player.speed_lock);
    player.speed_start = playback_speed_locked(SDL_GetTicks());
    player.speed_target = target;
    player.speed_ramp_started = SDL_GetTicks();
    player.speed_ramping = fabs(player.speed_start - target) > 0.0001;
    if (!player.speed_ramping) player.speed_current = target;
    SDL_AtomicUnlock(&player.speed_lock);
}

static double fast_forward_speed(void) {
    static const double speed[] = {2.0, 3.0, 4.0, 8.0};
    return speed[config.video.fast_forward_speed];
}

static double slow_motion_speed(void) {
    static const double speed[] = {0.5, 0.25, 0.125};
    return speed[config.video.slow_motion_speed];
}

static void shuffle_prepare(void) {
    if (!player.playlist || player.playlist_count < 2) return;
    const char *identity = player.container_uri[0] ? player.container_uri : player.playlist[0].uri;
    if (shuffle_seen_count == player.playlist_count && strcmp(shuffle_identity, identity) == 0) return;
    free(shuffle_seen);
    shuffle_seen = calloc(player.playlist_count, sizeof(*shuffle_seen));
    shuffle_seen_count = shuffle_seen ? player.playlist_count : 0;
    snprintf(shuffle_identity, sizeof(shuffle_identity), "%s", identity);
}

static void shuffle_reset(void) {
    free(shuffle_seen);
    shuffle_seen = NULL;
    shuffle_seen_count = 0;
    shuffle_identity[0] = '\0';
}

static size_t shuffle_next(void) {
    shuffle_prepare();
    if (!shuffle_seen || player.playlist_index >= shuffle_seen_count) return SIZE_MAX;
    shuffle_seen[player.playlist_index] = 1;
    size_t available = 0;
    for (size_t index = 0; index < shuffle_seen_count; index++)
        available += !shuffle_seen[index];
    if (!available && config.video.repeat_mode == 2) {
        memset(shuffle_seen, 0, shuffle_seen_count);
        shuffle_seen[player.playlist_index] = 1;
        available = shuffle_seen_count - 1;
    }
    if (!available) return SIZE_MAX;
    if (!shuffle_random_state) shuffle_random_state = SDL_GetTicks() ^ (uint32_t) getpid() ^ 0x9E3779B9U;
    shuffle_random_state ^= shuffle_random_state << 13;
    shuffle_random_state ^= shuffle_random_state >> 17;
    shuffle_random_state ^= shuffle_random_state << 5;
    size_t choice = shuffle_random_state % available;
    for (size_t index = 0; index < shuffle_seen_count; index++) {
        if (shuffle_seen[index]) continue;
        if (!choice--) return index;
    }
    return SIZE_MAX;
}

static size_t next_playlist_index(void) {
    if (!player.playlist || player.playlist_count < 2 || config.video.repeat_mode == 1) return SIZE_MAX;
    if (!config.video.auto_play && config.video.repeat_mode != 2) return SIZE_MAX;
    if (player.folder) return player.playlist_index + 1 < player.playlist_count ? player.playlist_index + 1 : SIZE_MAX;
    if (config.video.shuffle) return shuffle_next();
    if (player.playlist_index + 1 < player.playlist_count) return player.playlist_index + 1;
    return config.video.repeat_mode == 2 ? 0 : SIZE_MAX;
}

static size_t next_folder_index(void) {
    if (!player.folder || player.folder_count < 2) return SIZE_MAX;
    if (!config.video.auto_play && config.video.repeat_mode != 2) return SIZE_MAX;
    if (player.folder_index + 1 < player.folder_count) return player.folder_index + 1;
    return config.video.repeat_mode == 2 ? 0 : SIZE_MAX;
}

static void start_audio_transition(void) {
    if (!player.audio_only || player.sequenced_audio || audio_transition_handoff()
        || (!config.video.gapless && !config.video.crossfade))
        return;
    const size_t next = next_playlist_index();
    if (next == SIZE_MAX || next >= player.playlist_count || video_path_is_sequenced(player.playlist[next].uri)) {
        audio_transition_cancel();
        return;
    }
    audio_transition_start(
        player.playlist[next].uri, next, player.audio_rate, player.audio_channels, config.video.crossfade + 5
    );
}

static uint32_t preferred_present_interval(void) {
    if (!player.format || player.video_stream < 0) return PRESENT_INTERVAL_NORMAL_MS;
    const AVRational rate = av_guess_frame_rate(player.format, player.format->streams[player.video_stream], NULL);
    const double frames = rate.num > 0 && rate.den > 0 ? av_q2d(rate) : 0.0;
    if (frames <= 0.0) return PRESENT_INTERVAL_MAX_MS;
    uint32_t interval = (uint32_t) lround(1000.0 / frames / 4.0);
    if (interval < PRESENT_INTERVAL_MIN_MS) interval = PRESENT_INTERVAL_MIN_MS;
    if (interval > PRESENT_INTERVAL_MAX_MS) interval = PRESENT_INTERVAL_MAX_MS;
    return interval;
}

static int should_blend_frames(void) {
    player.blend_present_interval = 0;
    if (!player.format || player.video_stream < 0) return 0;
    const double panel = display_panel_refresh_hz();
    const AVRational rate = av_guess_frame_rate(player.format, player.format->streams[player.video_stream], NULL);
    const double frames = rate.num > 0 && rate.den > 0 ? av_q2d(rate) : 0.0;
    if (panel <= 0.0 || frames <= 0.0 || frames >= panel * 0.95) return 0;
    const double cadence = panel / frames;
    if (fabs(cadence - round(cadence)) <= 0.02) return 0;
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    if (frequency) player.blend_present_interval = (uint64_t) llround((double) frequency / panel);
    return 1;
}

static int blend_present_due(void) {
    if (!player.frame_blend || !player.blend_present_interval) return 1;
    const uint64_t now = SDL_GetPerformanceCounter();
    if (!player.blend_present_deadline) {
        player.blend_present_deadline = now + player.blend_present_interval;
        return 1;
    }
    if (now < player.blend_present_deadline) return 0;
    do
        player.blend_present_deadline += player.blend_present_interval;
    while (player.blend_present_deadline <= now);
    return 1;
}

static void set_present_timer_idle(const int idle) {
    if (!player.present_timer) return;
    lv_timer_set_period(player.present_timer, idle ? PRESENT_INTERVAL_IDLE_MS : player.present_interval);
}

static void show_live_loading(const char *title) {
    video_loading_prepare_content();
    video_render_set_static(1);
    video_loading_show_transparent(lang.muxmedia.buffering_live_tv, title);
    display_composite_frame();
}

static void begin_live_buffering(void) {
    if (!player.live || SDL_AtomicGet(&player.buffering)) return;
    player.buffering_started_at = SDL_GetTicks();
    player.live_failed = 0;
    SDL_AtomicSet(&player.buffering, 1);
    set_present_timer_idle(0);
    if (player.condition) SDL_CondBroadcast(player.condition);
}

static void show_live_failure(void) {
    if (player.live_failed) return;
    int queued_video = 0;
    size_t queued_packets = 0;
    if (player.lock) {
        SDL_LockMutex(player.lock);
        queued_video = player.video_count;
        queued_packets = player.packet_bytes;
        SDL_UnlockMutex(player.lock);
    }
    SDL_LockAudio();
    const int queued_audio = audio_available();
    SDL_UnlockAudio();
    LOG_ERROR(
        "muxmedia", "Live buffer failed with %d video frames, %d audio frames and %zu packet bytes", queued_video,
        queued_audio, queued_packets
    );
    fprintf(
        stderr, "Wasabi live buffer failed: video=%d audio=%d/%d packets=%zu/%zu demux_eof=%d eof=%d\n", queued_video,
        queued_audio, player.audio_capacity, queued_packets, player.packet_limit, player.demux_eof,
        SDL_AtomicGet(&player.eof)
    );
    player.live_failed = 1;
    player.buffering_visible = 0;
    video_loading_hide();
    video_render_set_static(1);
    display_composite_frame();
    toast_message(lang.muxmedia.live_tv_unavailable, tst_wait_l);
}

static double frame_seconds(const AVFrame *frame, const AVRational time_base) {
    int64_t timestamp = frame->best_effort_timestamp;
    if (timestamp == AV_NOPTS_VALUE) timestamp = frame->pts;
    if (timestamp == AV_NOPTS_VALUE) return -1.0;
    return (double) timestamp * av_q2d(time_base);
}

static double normalise_position(const double timestamp) {
    if (timestamp < 0.0) return timestamp;
    if (player.format && player.format->start_time != AV_NOPTS_VALUE)
        return timestamp - (double) player.format->start_time / AV_TIME_BASE;
    return timestamp;
}

static int audio_available(void) {
    if (player.audio_capacity <= 0) return 0;
    return (player.audio_write - player.audio_read + player.audio_capacity) % player.audio_capacity;
}

static int audio_space(void) {
    return player.audio_capacity - audio_available() - 1;
}

static int audio_empty(void) {
    SDL_LockAudio();
    const int empty = audio_available() == 0;
    SDL_UnlockAudio();
    return empty;
}

static void audio_speed_reset(void) {
    player.audio_speed_phase = 0;
    player.audio_speed_primed = 0;
    if (player.audio_speed_previous)
        memset(player.audio_speed_previous, 0, (size_t) player.audio_channels * sizeof(*player.audio_speed_previous));
    if (player.audio_speed_filter)
        memset(player.audio_speed_filter, 0, (size_t) player.audio_channels * sizeof(*player.audio_speed_filter));
}

static void audio_reset(void) {
    if (!player.audio_ring) return;
    SDL_LockAudio();
    player.audio_read = 0;
    player.audio_write = 0;
    player.audio_frames_played = 0;
    player.audio_clock_valid = 0;
    player.audio_callback_position = 0.0;
    player.audio_callback_duration = 0.0;
    player.audio_callback_rate = 1.0;
    player.audio_callback_ticks = 0;
    player.live_audio_underruns = 0;
    memset(player.audio_filter_state, 0, sizeof(player.audio_filter_state));
    memset(player.audio_filter_input, 0, sizeof(player.audio_filter_input));
    SDL_UnlockAudio();
    audio_speed_reset();
}

static void audio_write_frames_locked(const float *source, int frames) {
    if (!source || frames <= 0 || !player.audio_ring) return;
    const int space = audio_space();
    if (frames > space) frames = space;
    if (frames <= 0) return;

    const int first =
        frames < player.audio_capacity - player.audio_write ? frames : player.audio_capacity - player.audio_write;
    memcpy(
        player.audio_ring + (size_t) player.audio_write * player.audio_channels, source,
        (size_t) first * player.audio_channels * sizeof(float)
    );
    memcpy(
        player.audio_ring, source + (size_t) first * player.audio_channels,
        (size_t) (frames - first) * player.audio_channels * sizeof(float)
    );
    player.audio_write = (player.audio_write + frames) % player.audio_capacity;
}

static void audio_write(const float *source, int frames) {
    int offset = 0;
    while (offset < frames && !SDL_AtomicGet(&player.stop)) {
        SDL_LockMutex(player.lock);
        const int seeking = player.seek_pending;
        SDL_UnlockMutex(player.lock);
        if (seeking) break;

        SDL_LockAudio();
        int count = audio_space();
        if (count == 0 && player.live && (SDL_AtomicGet(&player.buffering) || SDL_AtomicGet(&player.paused))) {
            int discard = audio_available() / 4;
            if (discard < 1) discard = 1;
            player.audio_read = (player.audio_read + discard) % player.audio_capacity;
            if (player.audio_clock_valid && player.audio_rate > 0)
                player.audio_origin += (double) discard / player.audio_rate;
            count = audio_space();
        }
        if (count > frames - offset) count = frames - offset;
        if (count > 0) audio_write_frames_locked(source + (size_t) offset * player.audio_channels, count);
        SDL_UnlockAudio();
        offset += count;
        if (count == 0) SDL_Delay(2);
    }
}

static void audio_speed_write(const float *source, const int frames) {
    if (!source || frames <= 0) return;
    if (player.live || !player.audio_speed_convert || !player.audio_speed_previous || !player.audio_speed_filter
        || player.audio_channels <= 0) {
        audio_write(source, frames);
        return;
    }

    double speed = playback_speed();
    if (fabs(speed - 1.0) < 0.0000001) {
        player.audio_speed_phase = 0;
        player.audio_speed_primed = 0;
        audio_write(source, frames);
        return;
    }
    uint64_t step = (uint64_t) llround(speed * (double) AUDIO_SPEED_PHASE_ONE);
    if (!step) step = 1;
    float filter_alpha = speed > 1.0 ? (float) (2.0 / (speed + 1.0)) : 1.0f;
    int output_frames = 0;

    for (int frame = 0; frame < frames && !SDL_AtomicGet(&player.stop); frame++) {
        const float *input = source + (size_t) frame * player.audio_channels;
        if (!player.audio_speed_primed) {
            memcpy(
                player.audio_speed_filter, input, (size_t) player.audio_channels * sizeof(*player.audio_speed_filter)
            );
            memcpy(
                player.audio_speed_previous, input,
                (size_t) player.audio_channels * sizeof(*player.audio_speed_previous)
            );
            player.audio_speed_primed = 1;
        } else {
            for (int channel = 0; channel < player.audio_channels; channel++) {
                const float sample = input[channel];
                player.audio_speed_filter[channel] += filter_alpha * (sample - player.audio_speed_filter[channel]);
            }
        }

        while (player.audio_speed_phase < AUDIO_SPEED_PHASE_ONE) {
            const float fraction = (float) ((double) player.audio_speed_phase / (double) AUDIO_SPEED_PHASE_ONE);
            float *output = player.audio_speed_convert + (size_t) output_frames * player.audio_channels;
            for (int channel = 0; channel < player.audio_channels; channel++)
                output[channel] =
                    player.audio_speed_previous[channel]
                    + (player.audio_speed_filter[channel] - player.audio_speed_previous[channel]) * fraction;

            if (++output_frames == AUDIO_SPEED_BATCH_FRAMES) {
                audio_write(player.audio_speed_convert, output_frames);
                output_frames = 0;
                speed = playback_speed();
                step = (uint64_t) llround(speed * (double) AUDIO_SPEED_PHASE_ONE);
                if (!step) step = 1;
                filter_alpha = speed > 1.0 ? (float) (2.0 / (speed + 1.0)) : 1.0f;
            }
            player.audio_speed_phase += step;
        }
        player.audio_speed_phase -= AUDIO_SPEED_PHASE_ONE;
        memcpy(
            player.audio_speed_previous, player.audio_speed_filter,
            (size_t) player.audio_channels * sizeof(*player.audio_speed_previous)
        );
    }

    if (output_frames > 0) audio_write(player.audio_speed_convert, output_frames);
}

static void audio_callback(void *unused __attribute__((unused)), Uint8 *stream, const int length) {
    if (audio_transition_fill_handoff(stream, length, config.video.volume)) return;
    const int bytes_per_frame = player.audio_channels * (int) sizeof(float);
    if (bytes_per_frame <= 0) {
        SDL_memset(stream, 0, (size_t) length);
        return;
    }

    const int requested = length / bytes_per_frame;
    float *output = (float *) stream;
    if (SDL_AtomicGet(&player.paused) || SDL_AtomicGet(&player.buffering)) {
        SDL_memset(stream, 0, (size_t) length);
        return;
    }

    int available = audio_available();
    if (player.live && !SDL_AtomicGet(&player.eof) && available < requested) {
        if (++player.live_audio_underruns >= LIVE_AUDIO_UNDERRUN_GRACE) begin_live_buffering();
        SDL_memset(stream, 0, (size_t) length);
        return;
    }
    player.live_audio_underruns = 0;
    if (available > requested) available = requested;
    const double speed = audio_callback_speed();
    if (player.audio_clock_valid && player.audio_rate > 0) {
        player.audio_callback_position = player.audio_origin + player.audio_frames_played / (double) player.audio_rate;
        player.audio_callback_duration = (double) available / player.audio_rate;
        player.audio_callback_rate = speed;
        player.audio_callback_ticks = SDL_GetTicks();
    }
    if (available > 0) {
        const int first = available < player.audio_capacity - player.audio_read
                              ? available
                              : player.audio_capacity - player.audio_read;
        memcpy(
            output, player.audio_ring + (size_t) player.audio_read * player.audio_channels,
            (size_t) first * bytes_per_frame
        );
        memcpy(
            output + (size_t) first * player.audio_channels, player.audio_ring,
            (size_t) (available - first) * bytes_per_frame
        );
        player.audio_read = (player.audio_read + available) % player.audio_capacity;
    }
    if (available < requested)
        SDL_memset(
            output + (size_t) available * player.audio_channels, 0, (size_t) (requested - available) * bytes_per_frame
        );

    int output_frames = available;
    if (player.audio_only && audio_transition_ready()) {
        double remaining = player.duration > 0.0 && player.duration > player.position
                               ? player.duration - player.position
                           : player.duration > 0.0 ? 0.0
                                                   : -1.0;
        audio_transition_mix(
            output, available, requested, SDL_AtomicGet(&player.eof), remaining, config.video.crossfade
        );
        if (SDL_AtomicGet(&player.eof) && audio_available() == 0) audio_transition_mark_handoff();
        if (SDL_AtomicGet(&player.eof)) output_frames = requested;
    }

    if (player.audio_clock_valid) player.audio_frames_played += (double) available * speed;

    const int channels = player.audio_channels < 8 ? player.audio_channels : 8;
    const float volume = (float) config.video.volume / 100.0f;
    const int filter = config.video.audio_filter;
    if (filter == 0) {
        if (config.video.volume == 100) {
            video_render_audio_samples(output, output_frames, player.audio_channels);
            return;
        }
        const size_t samples = (size_t) output_frames * (size_t) player.audio_channels;
        for (size_t sample = 0; sample < samples; sample++)
            output[sample] *= volume;
        video_render_audio_samples(output, output_frames, player.audio_channels);
        return;
    }
    for (int frame = 0; frame < output_frames; frame++) {
        for (int channel = 0; channel < channels; channel++) {
            float sample = output[(size_t) frame * player.audio_channels + channel] * volume;
            if (filter == 1) {
                player.audio_filter_state[channel] += 0.15f * (sample - player.audio_filter_state[channel]);
                sample = player.audio_filter_state[channel];
            } else {
                const float filtered =
                    0.90f * (player.audio_filter_state[channel] + sample - player.audio_filter_input[channel]);
                player.audio_filter_input[channel] = sample;
                player.audio_filter_state[channel] = filtered;
                sample = filtered;
            }
            output[(size_t) frame * player.audio_channels + channel] = sample;
        }
    }
    video_render_audio_samples(output, output_frames, player.audio_channels);
}

static void audio_postmix(void *unused __attribute__((unused)), Uint8 *stream, const int length) {
    const int bytes_per_frame = player.audio_channels * (int) sizeof(float);
    if (!stream || bytes_per_frame <= 0 || length < bytes_per_frame) return;
    video_render_audio_samples((const float *) stream, length / bytes_per_frame, player.audio_channels);
}

static void queue_clear_locked(void) {
    for (int i = 0; i < VIDEO_QUEUE_SIZE; ++i)
        if (player.video_queue[i]) av_frame_unref(player.video_queue[i]);
    player.video_head = 0;
    player.video_tail = 0;
    player.video_count = 0;
}

static int queue_push(AVFrame *frame) {
    SDL_LockMutex(player.lock);
    while (!SDL_AtomicGet(&player.stop) && !player.seek_pending && player.video_count >= player.video_queue_limit) {
        if (player.live && (SDL_AtomicGet(&player.buffering) || SDL_AtomicGet(&player.paused))) {
            av_frame_unref(player.video_queue[player.video_head]);
            player.video_head = (player.video_head + 1) % player.video_queue_limit;
            player.video_count--;
            break;
        }
        SDL_CondWait(player.condition, player.lock);
    }
    if (SDL_AtomicGet(&player.stop) || player.seek_pending) {
        SDL_UnlockMutex(player.lock);
        av_frame_unref(frame);
        return 0;
    }
    av_frame_move_ref(player.video_queue[player.video_tail], frame);
    player.video_tail = (player.video_tail + 1) % player.video_queue_limit;
    player.video_count++;
    SDL_UnlockMutex(player.lock);
    return 1;
}

static int queue_pop_locked(AVFrame *destination) {
    if (player.video_count <= 0) return 0;
    av_frame_unref(destination);
    av_frame_move_ref(destination, player.video_queue[player.video_head]);
    player.video_head = (player.video_head + 1) % player.video_queue_limit;
    player.video_count--;
    SDL_CondSignal(player.condition);
    return 1;
}

static void packet_queue_clear_locked(void) {
    while (player.packet_head) {
        packet_node *node = player.packet_head;
        player.packet_head = node->next;
        av_packet_free(&node->packet);
        free(node);
    }
    player.packet_tail = NULL;
    player.packet_bytes = 0;
    while (player.packet_free) {
        packet_node *node = player.packet_free;
        player.packet_free = node->next;
        av_packet_free(&node->packet);
        free(node);
    }
    player.packet_free_count = 0;
}

static packet_node *packet_node_acquire(void) {
    packet_node *node = NULL;
    SDL_LockMutex(player.lock);
    if (player.packet_free) {
        node = player.packet_free;
        player.packet_free = node->next;
        player.packet_free_count--;
    }
    SDL_UnlockMutex(player.lock);

    if (!node) {
        node = calloc(1, sizeof(*node));
        if (!node) return NULL;
        node->packet = av_packet_alloc();
        if (!node->packet) {
            free(node);
            return NULL;
        }
    }
    node->next = NULL;
    node->bytes = 0;
    return node;
}

static int packet_queue_push(AVPacket *source) {
    packet_node *node = packet_node_acquire();
    if (!node) return 0;
    node->bytes = source->size > 0 ? (size_t) source->size : 1U;
    av_packet_move_ref(node->packet, source);

    SDL_LockMutex(player.lock);
    while (!SDL_AtomicGet(&player.stop) && player.packet_head
           && player.packet_bytes + node->bytes > player.packet_limit)
        SDL_CondWait(player.condition, player.lock);
    if (SDL_AtomicGet(&player.stop)) {
        SDL_UnlockMutex(player.lock);
        av_packet_unref(node->packet);
        SDL_LockMutex(player.lock);
        node->next = player.packet_free;
        player.packet_free = node;
        player.packet_free_count++;
        SDL_UnlockMutex(player.lock);
        return 0;
    }
    if (player.packet_tail)
        player.packet_tail->next = node;
    else
        player.packet_head = node;
    player.packet_tail = node;
    player.packet_bytes += node->bytes;
    SDL_CondBroadcast(player.condition);
    SDL_UnlockMutex(player.lock);
    return 1;
}

static int packet_queue_pop(AVPacket *destination) {
    SDL_LockMutex(player.lock);
    while (!SDL_AtomicGet(&player.stop) && !player.packet_head && !player.demux_eof)
        SDL_CondWait(player.condition, player.lock);
    if (!player.packet_head) {
        SDL_UnlockMutex(player.lock);
        return 0;
    }
    packet_node *node = player.packet_head;
    player.packet_head = node->next;
    if (!player.packet_head) player.packet_tail = NULL;
    player.packet_bytes -= node->bytes;
    av_packet_unref(destination);
    av_packet_move_ref(destination, node->packet);
    const int cache = player.packet_free_count < PACKET_CACHE_LIMIT;
    if (cache) {
        node->next = player.packet_free;
        player.packet_free = node;
        player.packet_free_count++;
    }
    SDL_CondBroadcast(player.condition);
    SDL_UnlockMutex(player.lock);
    if (!cache) {
        av_packet_free(&node->packet);
        free(node);
    }
    return 1;
}

static int interrupt_io(void *unused __attribute__((unused))) {
    if (SDL_AtomicGet(&player.stop)) return 1;
    const uint32_t started = (uint32_t) SDL_AtomicGet(&player.interrupt_ticks);
    if (!started) return 0;
    return SDL_GetTicks() - started > (player.live ? 15000U : 30000U);
}

static int64_t live_stream_bitrate(const unsigned int stream_index) {
    int64_t bitrate = player.format->streams[stream_index]->codecpar->bit_rate;
    for (unsigned int index = 0; index < player.format->nb_programs; index++) {
        AVProgram *program = player.format->programs[index];
        int contains = 0;
        for (unsigned int item = 0; item < program->nb_stream_indexes; item++)
            if (program->stream_index[item] == stream_index) {
                contains = 1;
                break;
            }
        if (!contains) continue;
        const AVDictionaryEntry *entry = av_dict_get(program->metadata, "variant_bitrate", NULL, 0);
        if (!entry) continue;
        char *end = NULL;
        const long long value = strtoll(entry->value, &end, 10);
        if (end != entry->value && !*end && value > bitrate) bitrate = value;
    }
    return bitrate;
}

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    int overflow;
} hls_manifest_buffer;

typedef struct {
    char *uri;
    int64_t bitrate;
    int width;
    int height;
    size_t order;
} hls_manifest_variant;

static size_t hls_manifest_write(void *contents, const size_t size, const size_t count, void *userdata) {
    hls_manifest_buffer *buffer = userdata;
    const size_t bytes = size * count;
    if (!bytes || buffer->overflow) return bytes;
    if (bytes > HLS_MANIFEST_LIMIT || buffer->size > HLS_MANIFEST_LIMIT - bytes) {
        buffer->overflow = 1;
        return 0;
    }
    const size_t required = buffer->size + bytes + 1;
    if (required > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 32U * 1024U;
        while (capacity < required && capacity < HLS_MANIFEST_LIMIT + 1U) {
            const size_t next = capacity * 2U;
            capacity = next > capacity && next <= HLS_MANIFEST_LIMIT + 1U ? next : HLS_MANIFEST_LIMIT + 1U;
        }
        char *data = realloc(buffer->data, capacity);
        if (!data) return 0;
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, contents, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';
    return bytes;
}

static int64_t hls_attribute_bitrate(const char *line) {
    const char *value = strcasestr(line, "AVERAGE-BANDWIDTH=");
    if (value) {
        value += strlen("AVERAGE-BANDWIDTH=");
    } else {
        value = strcasestr(line, "BANDWIDTH=");
        if (!value) return 0;
        value += strlen("BANDWIDTH=");
    }
    char *end = NULL;
    const long long bitrate = strtoll(value, &end, 10);
    return end != value && bitrate > 0 ? bitrate : 0;
}

static void hls_attribute_resolution(const char *line, int *width, int *height) {
    *width = 0;
    *height = 0;
    const char *value = strcasestr(line, "RESOLUTION=");
    if (!value) return;
    value += strlen("RESOLUTION=");
    int parsed_width = 0;
    int parsed_height = 0;
    if (sscanf(value, "%dx%d", &parsed_width, &parsed_height) != 2 || parsed_width <= 0 || parsed_height <= 0) return;
    *width = parsed_width;
    *height = parsed_height;
}

static int compare_hls_manifest_variant(const void *left, const void *right) {
    const hls_manifest_variant *a = left;
    const hls_manifest_variant *b = right;
    if (a->bitrate < b->bitrate) return -1;
    if (a->bitrate > b->bitrate) return 1;
    if (a->order < b->order) return -1;
    if (a->order > b->order) return 1;
    return 0;
}

static size_t automatic_live_variant(const live_variant *variants, const size_t count) {
    if (!variants || !count) return 0;

    const int64_t output_pixels = (int64_t) device.mux.width * device.mux.height;
    size_t selected = 0;
    int has_resolution = 0;
    int found = 0;
    for (size_t index = 0; index < count; index++) {
        if (variants[index].width <= 0 || variants[index].height <= 0) continue;
        has_resolution = 1;
        const int64_t pixels = (int64_t) variants[index].width * variants[index].height;
        if (pixels > output_pixels
            || (variants[index].bitrate > 0 && variants[index].bitrate > LIVE_AUTOMATIC_BITRATE_LIMIT))
            continue;
        selected = index;
        found = 1;
    }
    if (found || has_resolution) return selected;

    for (size_t index = 0; index < count; index++) {
        if (variants[index].bitrate > 0 && variants[index].bitrate > LIVE_AUTOMATIC_BITRATE_LIMIT) break;
        selected = index;
    }
    return selected;
}

static char *hls_absolute_uri(const char *base, const char *uri) {
    if (!base || !uri || !*uri) return NULL;
    if (strncasecmp(uri, "http://", 7) == 0 || strncasecmp(uri, "https://", 8) == 0) return strdup(uri);

    const char *scheme = strstr(base, "://");
    if (!scheme) return NULL;
    const char *authority = scheme + 3;
    const char *path = strchr(authority, '/');
    const size_t origin_length = path ? (size_t) (path - base) : strlen(base);

    if (uri[0] == '/') {
        if (uri[1] == '/') {
            const size_t scheme_length = (size_t) (scheme - base + 1);
            char *absolute = malloc(scheme_length + strlen(uri) + 1);
            if (!absolute) return NULL;
            memcpy(absolute, base, scheme_length);
            strcpy(absolute + scheme_length, uri);
            return absolute;
        }
        char *absolute = malloc(origin_length + strlen(uri) + 1);
        if (!absolute) return NULL;
        memcpy(absolute, base, origin_length);
        strcpy(absolute + origin_length, uri);
        return absolute;
    }

    const char *end = strpbrk(base, "?#");
    if (!end) end = base + strlen(base);
    const char *slash = end;
    while (slash > base && slash[-1] != '/')
        slash--;
    const size_t directory_length = (size_t) (slash - base);
    char *absolute = malloc(directory_length + strlen(uri) + 1);
    if (!absolute) return NULL;
    memcpy(absolute, base, directory_length);
    strcpy(absolute + directory_length, uri);
    return absolute;
}

static char *resolve_live_manifest(const char *uri) {
    if (!uri || (!strcasestr(uri, ".m3u8") && !strcasestr(uri, ".m3u?"))) return NULL;

    static int curl_ready;
    if (!curl_ready) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return NULL;
        curl_ready = 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    hls_manifest_buffer body = {0};
    curl_easy_setopt(curl, CURLOPT_URL, uri);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, hls_manifest_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 8L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MustardOS-Wasabi/1.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    if (config.settings.network.proxy_server[0]) {
        curl_easy_setopt(curl, CURLOPT_PROXY, config.settings.network.proxy_server);
        if (config.settings.network.proxy_noproxy[0])
            curl_easy_setopt(curl, CURLOPT_NOPROXY, config.settings.network.proxy_noproxy);
    }

    const CURLcode result = curl_easy_perform(curl);
    char *effective = NULL;
    char *effective_copy = NULL;
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (result == CURLE_OK && status >= 200 && status < 400
        && curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK && effective)
        effective_copy = strdup(effective);
    curl_easy_cleanup(curl);
    if (!effective_copy || body.overflow || !body.data || !strstr(body.data, "#EXT-X-STREAM-INF")
        || (strcasestr(body.data, "#EXT-X-MEDIA:") && strcasestr(body.data, "TYPE=AUDIO"))) {
        free(effective_copy);
        free(body.data);
        return NULL;
    }

    hls_manifest_variant *variants = NULL;
    size_t variant_count = 0;
    int64_t pending_bitrate = -1;
    int pending_width = 0;
    int pending_height = 0;
    char *save = NULL;
    for (char *line = strtok_r(body.data, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t' || *line == '\r')
            line++;
        size_t length = strlen(line);
        while (length && (line[length - 1] == ' ' || line[length - 1] == '\t' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (strncasecmp(line, "#EXT-X-STREAM-INF:", 18) == 0) {
            pending_bitrate = hls_attribute_bitrate(line + 18);
            hls_attribute_resolution(line + 18, &pending_width, &pending_height);
            continue;
        }
        if (pending_bitrate < 0 || !*line || *line == '#') continue;
        hls_manifest_variant *next = realloc(variants, (variant_count + 1) * sizeof(*next));
        if (!next) break;
        variants = next;
        variants[variant_count].uri = strdup(line);
        variants[variant_count].bitrate = pending_bitrate;
        variants[variant_count].width = pending_width;
        variants[variant_count].height = pending_height;
        variants[variant_count].order = variant_count;
        if (!variants[variant_count].uri) break;
        variant_count++;
        pending_bitrate = -1;
        pending_width = 0;
        pending_height = 0;
    }
    free(body.data);
    if (!variant_count) {
        free(variants);
        free(effective_copy);
        return NULL;
    }

    qsort(variants, variant_count, sizeof(*variants), compare_hls_manifest_variant);
    size_t selected = 0;
    if (config.wasabi.live_quality == 0) {
        live_variant *automatic = calloc(variant_count, sizeof(*automatic));
        if (automatic) {
            for (size_t index = 0; index < variant_count; index++) {
                automatic[index].stream = -1;
                automatic[index].bitrate = variants[index].bitrate;
                automatic[index].width = variants[index].width;
                automatic[index].height = variants[index].height;
            }
            selected = automatic_live_variant(automatic, variant_count);
            free(automatic);
        }
    } else if (config.wasabi.live_quality == 2) {
        selected = variant_count / 2;
    } else if (config.wasabi.live_quality == 3) {
        selected = variant_count - 1;
    }

    player.live_variants = calloc(variant_count, sizeof(*player.live_variants));
    if (player.live_variants) {
        player.live_variant_count = variant_count;
        for (size_t index = 0; index < variant_count; index++) {
            player.live_variants[index].stream = -1;
            player.live_variants[index].bitrate = variants[index].bitrate;
            player.live_variants[index].width = variants[index].width;
            player.live_variants[index].height = variants[index].height;
        }
    }
    player.live_selected_bitrate = variants[selected].bitrate;
    LOG_INFO(
        "muxmedia", "Live rendition %dx%d at %lld bps for %dx%d output", variants[selected].width,
        variants[selected].height, (long long) variants[selected].bitrate, device.mux.width, device.mux.height
    );
    char *selected_uri = hls_absolute_uri(effective_copy, variants[selected].uri);
    for (size_t index = 0; index < variant_count; index++)
        free(variants[index].uri);
    free(variants);
    free(effective_copy);
    if (selected_uri) player.live_manifest_resolved = 1;
    return selected_uri;
}

static int compare_live_variant(const void *left, const void *right) {
    const live_variant *a = left;
    const live_variant *b = right;
    if (a->bitrate < b->bitrate) return -1;
    if (a->bitrate > b->bitrate) return 1;
    return a->stream - b->stream;
}

static int select_live_video_stream(const int automatic) {
    player.live_variants = calloc(player.format->nb_streams, sizeof(*player.live_variants));
    if (!player.live_variants) return automatic;
    for (unsigned int index = 0; index < player.format->nb_streams; index++) {
        if (player.format->streams[index]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) continue;
        player.live_variants[player.live_variant_count++] = (live_variant) {
            .stream = (int) index,
            .bitrate = live_stream_bitrate(index),
            .width = player.format->streams[index]->codecpar->width,
            .height = player.format->streams[index]->codecpar->height,
        };
    }
    if (!player.live_variant_count) return automatic;
    qsort(player.live_variants, player.live_variant_count, sizeof(*player.live_variants), compare_live_variant);

    size_t selected = 0;
    if (config.wasabi.live_quality == 0) {
        selected = automatic_live_variant(player.live_variants, player.live_variant_count);
    } else if (config.wasabi.live_quality == 2) {
        selected = player.live_variant_count / 2;
    } else if (config.wasabi.live_quality == 3) {
        selected = player.live_variant_count - 1;
    }
    player.live_selected_bitrate = player.live_variants[selected].bitrate;
    return player.live_variants[selected].stream;
}

static int live_audio_stream_for_video(const int video_stream) {
    if (!player.format || video_stream < 0) return -1;
    for (unsigned int index = 0; index < player.format->nb_programs; index++) {
        AVProgram *program = player.format->programs[index];
        int contains_video = 0;
        for (unsigned int item = 0; item < program->nb_stream_indexes; item++)
            if ((int) program->stream_index[item] == video_stream) {
                contains_video = 1;
                break;
            }
        if (!contains_video) continue;
        for (unsigned int item = 0; item < program->nb_stream_indexes; item++) {
            const unsigned int stream = program->stream_index[item];
            if (stream < player.format->nb_streams
                && player.format->streams[stream]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                return (int) stream;
        }
    }
    for (unsigned int stream = 0; stream < player.format->nb_streams; stream++)
        if (player.format->streams[stream]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) return (int) stream;
    return -1;
}

static void discard_unselected_live_streams(const int video_stream, const int audio_stream) {
    if (!player.format) return;
    for (unsigned int stream = 0; stream < player.format->nb_streams; stream++)
        player.format->streams[stream]->discard =
            (int) stream == video_stream || (int) stream == audio_stream ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
}

int video_player_live_quality_available(void) {
    return player.live && player.live_variant_count > 1;
}

int video_player_tracker_loop_available(void) {
    return player.audio_only && player.tracker_audio;
}

int video_player_seek_available(void) {
    return !player.live && player.duration > 0.0;
}

int video_player_speed_available(void) {
    return !player.live && !player.sequenced_audio && (player.video_decoder || player.audio_decoder);
}

static void format_live_bitrate(char *buffer, const size_t size, const int64_t bitrate) {
    if (bitrate >= 1000000)
        snprintf(buffer, size, "%.1f Mbps", (double) bitrate / 1000000.0);
    else if (bitrate > 0)
        snprintf(buffer, size, "%lld kbps", (long long) ((bitrate + 500) / 1000));
    else
        snprintf(buffer, size, "%s", lang.generic.unknown);
}

void video_player_live_quality_value(char *buffer, const size_t size) {
    if (!buffer || !size) return;
    if (config.wasabi.live_quality == 0) {
        if (player.live_selected_bitrate > 0) {
            char bitrate[32];
            format_live_bitrate(bitrate, sizeof(bitrate), player.live_selected_bitrate);
            snprintf(buffer, size, "%s (%s)", lang.wasabi_automatic, bitrate);
        } else {
            snprintf(buffer, size, "%s", lang.wasabi_automatic);
        }
        return;
    }
    if (player.live_variant_count) {
        size_t selected = config.wasabi.live_quality == 1   ? 0
                          : config.wasabi.live_quality == 2 ? player.live_variant_count / 2
                                                            : player.live_variant_count - 1;
        format_live_bitrate(buffer, size, player.live_variants[selected].bitrate);
        return;
    }
    snprintf(
        buffer, size, "%s",
        config.wasabi.live_quality == 1   ? lang.generic.low
        : config.wasabi.live_quality == 2 ? lang.generic.medium
                                          : lang.generic.high
    );
}

static const char *hardware_decoder_name(const enum AVCodecID codec) {
    switch (codec) {
        case AV_CODEC_ID_H264:
            return "h264_v4l2m2m";
        case AV_CODEC_ID_HEVC:
            return "hevc_v4l2m2m";
        case AV_CODEC_ID_MPEG1VIDEO:
            return "mpeg1_v4l2m2m";
        case AV_CODEC_ID_MPEG2VIDEO:
            return "mpeg2_v4l2m2m";
        case AV_CODEC_ID_MPEG4:
            return "mpeg4_v4l2m2m";
        case AV_CODEC_ID_VP8:
            return "vp8_v4l2m2m";
        case AV_CODEC_ID_VP9:
            return "vp9_v4l2m2m";
        case AV_CODEC_ID_VC1:
            return "vc1_v4l2m2m";
        default:
            return NULL;
    }
}

static const AVCodec *software_decoder(const enum AVCodecID codec) {
    if (codec == AV_CODEC_ID_AV1) {
        const AVCodec *decoder = avcodec_find_decoder_by_name("libdav1d");
        if (!decoder) decoder = avcodec_find_decoder_by_name("libaom-av1");
        if (decoder) return decoder;
        video_codec_unsupported = 1;
    }
    return avcodec_find_decoder(codec);
}

static AVCodecContext *open_decoder(const int stream_index, const int hardware) {
    AVStream *stream = player.format->streams[stream_index];
    const AVCodec *codec = NULL;
    if (hardware) {
        const char *name = hardware_decoder_name(stream->codecpar->codec_id);
        if (name) codec = avcodec_find_decoder_by_name(name);
    }
    if (!codec) codec = software_decoder(stream->codecpar->codec_id);
    if (!codec) return NULL;

    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context) return NULL;
    if (avcodec_parameters_to_context(context, stream->codecpar) < 0) {
        avcodec_free_context(&context);
        return NULL;
    }
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        int threads = SDL_GetCPUCount();
        if (threads < 1) threads = 1;
        if (threads > 4) threads = 4;
        const int64_t area = (int64_t) stream->codecpar->width * stream->codecpar->height;
        if (area > 0 && area < (int64_t) 640 * 480 && threads > 2)
            threads = 2;
        else if (area > 0 && area < (int64_t) 1280 * 720 && threads > 3)
            threads = 3;
        context->thread_count = threads;
        context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    } else {
        context->thread_count = 1;
        context->thread_type = 0;
    }
    if (avcodec_open2(context, codec, NULL) < 0) {
        avcodec_free_context(&context);
        if (hardware) return open_decoder(stream_index, 0);
        return NULL;
    }
    LOG_INFO("muxmedia", "Using %s for stream %d", codec->name, stream_index);
    return context;
}

static int setup_resampler(const AVFrame *frame) {
    if (player.resample) return 1;
    AVChannelLayout output_layout;
    av_channel_layout_default(&output_layout, player.audio_channels);
    const int result = swr_alloc_set_opts2(
        &player.resample, &output_layout, AV_SAMPLE_FMT_FLT, player.audio_rate, &frame->ch_layout,
        (enum AVSampleFormat) frame->format, frame->sample_rate, 0, NULL
    );
    av_channel_layout_uninit(&output_layout);
    if (result < 0 || !player.resample || swr_init(player.resample) < 0) {
        swr_free(&player.resample);
        return 0;
    }
    return 1;
}

static void decode_audio(const AVPacket *packet) {
    if (!player.audio_decoder || avcodec_send_packet(player.audio_decoder, packet) < 0) return;
    while (!SDL_AtomicGet(&player.stop) && avcodec_receive_frame(player.audio_decoder, player.decode_frame) >= 0) {
        if (setup_resampler(player.decode_frame)) {
            static const int rate_control[] = {0, 25, 50, 100, 200};
            const int deviation = rate_control[config.video.rate_control];
            if (player.live && deviation > 0 && player.audio_capacity > 0) {
                SDL_LockAudio();
                const int fill = audio_available();
                SDL_UnlockAudio();
                int target = player.audio_rate * live_audio_target_seconds();
                if (target >= player.audio_capacity) target = player.audio_capacity - 1;
                int correction = target - fill;
                const int maximum = player.audio_rate * deviation / 10000;
                if (correction > maximum) correction = maximum;
                if (correction < -maximum) correction = -maximum;
                swr_set_compensation(player.resample, correction, player.audio_rate);
            }
            uint8_t *output = (uint8_t *) player.audio_convert;
            const int frames = swr_convert(
                player.resample, &output, AUDIO_CONVERT_FRAMES, (const uint8_t **) player.decode_frame->data,
                player.decode_frame->nb_samples
            );
            if (frames > 0) {
                SDL_LockAudio();
                const double timestamp = normalise_position(frame_seconds(player.decode_frame, player.audio_time_base));
                if (timestamp >= 0.0) {
                    if (!player.audio_clock_valid) {
                        player.audio_origin = timestamp;
                        player.audio_frames_played = 0;
                        player.audio_clock_valid = 1;
                    } else if (player.live && player.audio_rate > 0) {
                        const double expected =
                            player.audio_origin
                            + (double) (player.audio_frames_played + audio_available()) / player.audio_rate;
                        const double difference = timestamp - expected;
                        if (fabs(difference) >= LIVE_AUDIO_CLOCK_HARD_SECONDS) {
                            player.audio_origin += difference;
                        } else if (fabs(difference) >= LIVE_AUDIO_CLOCK_SOFT_SECONDS) {
                            double correction = difference * LIVE_AUDIO_CLOCK_CORRECTION_FACTOR;
                            if (correction > LIVE_AUDIO_CLOCK_STEP_SECONDS)
                                correction = LIVE_AUDIO_CLOCK_STEP_SECONDS;
                            else if (correction < -LIVE_AUDIO_CLOCK_STEP_SECONDS)
                                correction = -LIVE_AUDIO_CLOCK_STEP_SECONDS;
                            player.audio_origin += correction;
                        }
                    }
                }
                SDL_UnlockAudio();
                if (!player.live)
                    audio_speed_write(player.audio_convert, frames);
                else
                    audio_write(player.audio_convert, frames);
            }
        }
        av_frame_unref(player.decode_frame);
    }
}

static int visual_filters_active(const AVFrame *frame) {
    return player.deinterlace && frame && (frame->flags & AV_FRAME_FLAG_INTERLACED);
}

static int setup_video_filters(const AVFrame *frame) {
    if (player.filter_graph && player.filter_width == frame->width && player.filter_height == frame->height
        && player.filter_format == frame->format)
        return 1;
    avfilter_graph_free(&player.filter_graph);
    player.filter_source = NULL;
    player.filter_sink = NULL;
    char arguments[256];
    snprintf(
        arguments, sizeof(arguments), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d", frame->width,
        frame->height, frame->format, player.video_time_base.num, player.video_time_base.den,
        frame->sample_aspect_ratio.num ? frame->sample_aspect_ratio.num : 1,
        frame->sample_aspect_ratio.den ? frame->sample_aspect_ratio.den : 1
    );

    player.filter_graph = avfilter_graph_alloc();
    if (!player.filter_graph) return 0;
    const AVFilter *buffer = avfilter_get_by_name("buffer");
    const AVFilter *sink = avfilter_get_by_name("buffersink");
    if (!buffer || !sink
        || avfilter_graph_create_filter(&player.filter_source, buffer, "source", arguments, NULL, player.filter_graph)
               < 0
        || avfilter_graph_create_filter(&player.filter_sink, sink, "sink", NULL, NULL, player.filter_graph) < 0) {
        avfilter_graph_free(&player.filter_graph);
        player.filter_source = NULL;
        player.filter_sink = NULL;
        return 0;
    }

    AVFilterContext *tail = player.filter_source;
    if (player.deinterlace) {
        AVFilterContext *next = NULL;
        const AVFilter *filter = avfilter_get_by_name("bwdif");
        if (!filter
            || avfilter_graph_create_filter(
                   &next, filter, "deinterlace", "mode=send_frame:parity=auto:deint=interlaced", NULL,
                   player.filter_graph
               ) < 0
            || avfilter_link(tail, 0, next, 0) < 0)
            goto failed;
        tail = next;
    }

    if (avfilter_link(tail, 0, player.filter_sink, 0) < 0 || avfilter_graph_config(player.filter_graph, NULL) < 0) {
    failed:
        avfilter_graph_free(&player.filter_graph);
        player.filter_source = NULL;
        player.filter_sink = NULL;
        return 0;
    }
    player.filter_width = frame->width;
    player.filter_height = frame->height;
    player.filter_format = frame->format;
    return 1;
}

static void decode_video(const AVPacket *packet) {
    if (!player.video_decoder || avcodec_send_packet(player.video_decoder, packet) < 0) return;
    while (!SDL_AtomicGet(&player.stop) && avcodec_receive_frame(player.video_decoder, player.decode_frame) >= 0) {
        const int revision = SDL_AtomicGet(&player.filter_revision);
        if (revision != player.filter_applied_revision) {
            avfilter_graph_free(&player.filter_graph);
            player.filter_source = NULL;
            player.filter_sink = NULL;
            player.filter_applied_revision = revision;
        }
        if (!visual_filters_active(player.decode_frame) || !setup_video_filters(player.decode_frame)) {
            if (!queue_push(player.decode_frame)) break;
            continue;
        }
        if (av_buffersrc_add_frame_flags(player.filter_source, player.decode_frame, 0) < 0) {
            av_frame_unref(player.decode_frame);
            continue;
        }
        while (av_buffersink_get_frame(player.filter_sink, player.filter_frame) >= 0)
            if (!queue_push(player.filter_frame)) break;
    }
}

void video_player_image_settings_changed(void) {
    video_render_settings_changed();
}

void video_player_effect_settings_changed(void) {
    video_render_effects_changed();
}

void video_player_audio_settings_changed(void) {
    if (player.sequenced_audio) Mix_VolumeMusic(config.video.volume * MIX_MAX_VOLUME / 100);
    SDL_LockAudio();
    memset(player.audio_filter_state, 0, sizeof(player.audio_filter_state));
    memset(player.audio_filter_input, 0, sizeof(player.audio_filter_input));
    SDL_UnlockAudio();
    if (!config.video.fast_forward_mode) set_fast_forward(0);
    if (!config.video.slow_motion_mode) set_slow_motion(0);
    if (player.fast_forward_active) set_speed_target(fast_forward_speed());
    if (player.slow_motion_active) set_speed_target(slow_motion_speed());
}

void video_player_audio_ui_changed(void) {
    if (!player.audio_only) return;
    if (!wasabi_audio_ui_init(&player.audio_information)) return;
    wasabi_audio_ui_update(player.position, player.duration, SDL_AtomicGet(&player.paused));
    display_composite_frame();
}

void video_player_modes_changed(void) {
    if (player.audio_only && !audio_transition_handoff()) {
        audio_transition_cancel();
        start_audio_transition();
    }
    video_playback_ui_modes_changed();
    wasabi_audio_ui_modes_changed();
    display_composite_frame();
}

void video_player_transition_settings_changed(void) {
    if (!player.audio_only || audio_transition_handoff()) return;
    audio_transition_cancel();
    start_audio_transition();
}

void video_player_diagnostics(char *buffer, const size_t size) {
    if (!buffer || !size) return;
    const enum AVCodecID video_id = player.video_decoder ? player.video_decoder->codec_id
                                    : player.format && player.video_stream >= 0
                                        ? player.format->streams[player.video_stream]->codecpar->codec_id
                                        : AV_CODEC_ID_NONE;
    const enum AVCodecID audio_id = player.audio_decoder ? player.audio_decoder->codec_id
                                    : player.format && player.audio_stream >= 0
                                        ? player.format->streams[player.audio_stream]->codecpar->codec_id
                                        : AV_CODEC_ID_NONE;
    const char *video_codec = video_id != AV_CODEC_ID_NONE ? avcodec_get_name(video_id) : "None";
    const char *audio_codec = audio_id != AV_CODEC_ID_NONE ? avcodec_get_name(audio_id) : "None";
    int queued_video = 0;
    int queued_audio = 0;
    if (player.lock) {
        SDL_LockMutex(player.lock);
        queued_video = player.video_count;
        SDL_UnlockMutex(player.lock);
    }
    SDL_LockAudio();
    queued_audio = audio_available();
    SDL_UnlockAudio();
    snprintf(
        buffer, size, "%s: %s\n%s: %d\xC3\x97%d\n%s: %d\n%s: %s\n%s: %d Hz / %d channel%s\n%s: %d",
        lang.muxretro.information_screen.section_video, video_codec, lang.muxmedia.resolution,
        player.video_decoder                        ? player.video_decoder->width
        : player.format && player.video_stream >= 0 ? player.format->streams[player.video_stream]->codecpar->width
                                                    : 0,
        player.video_decoder                        ? player.video_decoder->height
        : player.format && player.video_stream >= 0 ? player.format->streams[player.video_stream]->codecpar->height
                                                    : 0,
        lang.muxmedia.queued_frames, queued_video, lang.muxretro.information_screen.section_audio, audio_codec,
        lang.muxmedia.audio_output, player.audio_rate, player.audio_channels, player.audio_channels == 1 ? "" : "s",
        lang.muxmedia.queued_samples, queued_audio
    );
}

void video_player_get_information(video_player_info *information) {
    if (!information) return;
    memset(information, 0, sizeof(*information));

    snprintf(
        information->video_codec, sizeof(information->video_codec), "%s",
        player.video_decoder ? avcodec_get_name(player.video_decoder->codec_id)
        : player.format && player.video_stream >= 0
            ? avcodec_get_name(player.format->streams[player.video_stream]->codecpar->codec_id)
            : lang.generic.none
    );
    snprintf(
        information->audio_codec, sizeof(information->audio_codec), "%s",
        player.sequenced_audio ? "MIDI"
        : player.audio_decoder ? avcodec_get_name(player.audio_decoder->codec_id)
        : player.format && player.audio_stream >= 0
            ? avcodec_get_name(player.format->streams[player.audio_stream]->codecpar->codec_id)
            : lang.generic.none
    );
    const char *pixel_format = player.video_decoder ? av_get_pix_fmt_name(player.video_decoder->pix_fmt) : NULL;
    snprintf(
        information->pixel_format, sizeof(information->pixel_format), "%s",
        pixel_format ? pixel_format : lang.generic.unknown
    );
    if (player.video_decoder) {
        information->width = player.video_decoder->width;
        information->height = player.video_decoder->height;
    } else if (player.format && player.video_stream >= 0) {
        information->width = player.format->streams[player.video_stream]->codecpar->width;
        information->height = player.format->streams[player.video_stream]->codecpar->height;
    }
    if (player.format && player.video_stream >= 0) {
        const AVRational rate = av_guess_frame_rate(player.format, player.format->streams[player.video_stream], NULL);
        if (rate.num && rate.den) information->frame_rate = av_q2d(rate);
    }
    information->audio_rate = player.audio_rate;
    information->audio_channels = player.audio_channels;
    information->position = player.position;
    information->duration = player.duration;
    information->live = player.live;
    information->deinterlace = player.deinterlace;
    information->audio_only = player.audio_only;
    snprintf(information->title, sizeof(information->title), "%s", player.audio_information.title);
    snprintf(information->artist, sizeof(information->artist), "%s", player.audio_information.artist);
    snprintf(information->album, sizeof(information->album), "%s", player.audio_information.album);
    snprintf(information->album_artist, sizeof(information->album_artist), "%s", player.audio_information.album_artist);
    snprintf(information->composer, sizeof(information->composer), "%s", player.audio_information.composer);
    snprintf(information->genre, sizeof(information->genre), "%s", player.audio_information.genre);
    snprintf(information->year, sizeof(information->year), "%s", player.audio_information.year);
    snprintf(information->track, sizeof(information->track), "%s", player.audio_information.track);
    snprintf(information->disc, sizeof(information->disc), "%s", player.audio_information.disc);
    snprintf(information->comment, sizeof(information->comment), "%s", player.audio_information.comment);
    snprintf(information->copyright, sizeof(information->copyright), "%s", player.audio_information.copyright);
    snprintf(information->encoder, sizeof(information->encoder), "%s", player.audio_information.encoder);
    snprintf(information->format, sizeof(information->format), "%s", player.audio_information.format);
    information->bitrate = player.audio_information.bitrate;

    if (player.lock) {
        SDL_LockMutex(player.lock);
        information->queued_video = player.video_count;
        SDL_UnlockMutex(player.lock);
    }
    SDL_LockAudio();
    information->queued_audio = audio_available();
    SDL_UnlockAudio();
}

static void perform_seek(const double target) {
    int64_t timestamp = (int64_t) llround(target * AV_TIME_BASE);
    if (player.format->start_time != AV_NOPTS_VALUE) timestamp += player.format->start_time;
    if (avformat_seek_file(player.format, -1, INT64_MIN, timestamp, INT64_MAX, AVSEEK_FLAG_BACKWARD) < 0) return;

    if (player.video_decoder) avcodec_flush_buffers(player.video_decoder);
    if (player.audio_decoder) avcodec_flush_buffers(player.audio_decoder);
    swr_free(&player.resample);
    avfilter_graph_free(&player.filter_graph);
    player.filter_source = NULL;
    player.filter_sink = NULL;

    SDL_LockMutex(player.lock);
    queue_clear_locked();
    SDL_AtomicSet(&player.eof, 0);
    SDL_CondSignal(player.condition);
    SDL_UnlockMutex(player.lock);
    audio_reset();
    player.presented_timestamp = -1.0;
    player.blend_timestamp = -1.0;
    player.blend_present_deadline = 0;
    player.present_dirty = 0;
    video_render_set_blend(0.0);
}

static void flush_decoders(void) {
    decode_video(NULL);
    decode_audio(NULL);
    if (player.filter_source && player.filter_sink) {
        const int flush_result = av_buffersrc_add_frame_flags(player.filter_source, NULL, 0);
        if (flush_result >= 0)
            while (!SDL_AtomicGet(&player.stop)
                   && av_buffersink_get_frame(player.filter_sink, player.filter_frame) >= 0)
                if (!queue_push(player.filter_frame)) break;
    }
    SDL_AtomicSet(&player.eof, 1);
}

static int demux_thread(void *unused __attribute__((unused))) {
    while (!SDL_AtomicGet(&player.stop)) {
        SDL_AtomicSet(&player.interrupt_ticks, (int) SDL_GetTicks());
        const int result = av_read_frame(player.format, player.demux_packet);
        SDL_AtomicSet(&player.interrupt_ticks, 0);
        if (result < 0) break;
        if (player.demux_packet->stream_index != player.video_stream
            && player.demux_packet->stream_index != player.audio_stream) {
            av_packet_unref(player.demux_packet);
            continue;
        }
        if (!packet_queue_push(player.demux_packet)) break;
    }
    SDL_LockMutex(player.lock);
    player.demux_eof = 1;
    SDL_CondBroadcast(player.condition);
    SDL_UnlockMutex(player.lock);
    return 0;
}

static int decode_thread(void *unused __attribute__((unused))) {
    while (!SDL_AtomicGet(&player.stop)) {
        if (player.live) {
            if (!packet_queue_pop(player.packet)) break;
        } else {
            SDL_LockMutex(player.lock);
            while ((SDL_AtomicGet(&player.paused) || SDL_AtomicGet(&player.eof)) && !SDL_AtomicGet(&player.stop)
                   && !player.seek_pending)
                SDL_CondWait(player.condition, player.lock);
            const int seek = player.seek_pending;
            const double target = player.seek_target;
            player.seek_pending = 0;
            SDL_UnlockMutex(player.lock);
            if (SDL_AtomicGet(&player.stop)) break;
            if (seek) perform_seek(target);

            SDL_AtomicSet(&player.interrupt_ticks, (int) SDL_GetTicks());
            const int result = av_read_frame(player.format, player.packet);
            SDL_AtomicSet(&player.interrupt_ticks, 0);
            if (result < 0) {
                flush_decoders();
                continue;
            }
        }
        if (player.packet->stream_index == player.video_stream)
            decode_video(player.packet);
        else if (player.packet->stream_index == player.audio_stream)
            decode_audio(player.packet);
        av_packet_unref(player.packet);
    }
    if (player.live && !SDL_AtomicGet(&player.stop)) flush_decoders();
    return 0;
}

static double playback_clock(void) {
    if (SDL_AtomicGet(&player.paused) || SDL_AtomicGet(&player.buffering)) return player.clock_origin;
    if (player.sequenced_audio) {
        const double position = Mix_GetMusicPosition(player.sequenced_audio);
        return position >= 0.0 ? position : player.clock_origin;
    }
    SDL_LockAudio();
    const int audio_valid = player.audio_clock_valid && player.audio_rate > 0;
    double audio_position =
        audio_valid ? player.audio_origin + (double) player.audio_frames_played / player.audio_rate : 0.0;
    if (audio_valid && player.audio_callback_ticks && player.audio_callback_duration > 0.0) {
        double elapsed = (double) (SDL_GetTicks() - player.audio_callback_ticks) / 1000.0;
        if (elapsed > player.audio_callback_duration) elapsed = player.audio_callback_duration;
        audio_position = player.audio_callback_position + elapsed * player.audio_callback_rate;
    }
    SDL_UnlockAudio();
    if (audio_valid && player.audio_rate > 0) return audio_position;
    const uint32_t now = SDL_GetTicks();
    const double speed = playback_speed();
    const double elapsed = (double) (now - player.clock_ticks) / 1000.0;
    player.clock_origin += elapsed * (player.clock_speed + speed) * 0.5;
    player.clock_speed = speed;
    player.clock_ticks = now;
    return player.clock_origin;
}

static int frame_is_blank(const uint8_t *pixels, const size_t pixel_count) {
    size_t samples = 0;
    size_t bright = 0;
    uint64_t total = 0;

    for (size_t i = 0; i < pixel_count; i += 7) {
        const uint8_t *pixel = pixels + i * 3U;
        const unsigned int luma = (77U * pixel[0] + 150U * pixel[1] + 29U * pixel[2]) >> 8;
        total += luma;
        bright += luma > 48;
        samples++;
    }

    return samples && total / samples < 16 && bright * 100 < samples;
}

static int playback_near_end(void) {
    if (player.live || player.duration <= 0.0) return 0;
    const double tail = player.duration * 0.1 < 60.0 ? player.duration * 0.1 : 60.0;
    return player.position >= player.duration - tail;
}

static int store_clean_frame(const char *path, const int skip_blank) {
    if (!path || !path[0]) return -1;

    const size_t pixel_count = (size_t) device.screen.width * (size_t) device.screen.height;
    uint8_t *pixels = malloc(pixel_count * 3U);
    if (!pixels) return -1;

    const int ui_was_hidden = display_ui_is_hidden();
    display_set_composite_suppressed(1);
    video_render_set_clean_capture(1);
    display_set_ui_hidden(1);
    int result = display_capture_clean_pixels(pixels, device.screen.width, device.screen.height);
    display_set_ui_hidden(ui_was_hidden);
    video_render_set_clean_capture(0);
    display_set_composite_suppressed(0);
    display_composite_frame();

    if (result == 0 && skip_blank && frame_is_blank(pixels, pixel_count)) result = -1;

    if (result == 0)
        result = screenshot_write_rgb(path, pixels, (uint32_t) device.screen.width, (uint32_t) device.screen.height);
    free(pixels);
    return result;
}

static void save_history(const int complete, const int store_thumbnail) {
    char thumbnail[PATH_MAX] = "";
    const int has_container = player.container_uri[0] && strcmp(player.container_uri, player.uri) != 0;
    const int needs_thumbnail = !player.audio_only && store_thumbnail && !playback_near_end()
                                && ((player.history_enabled && (player.live || (player.position >= 1.0 && !complete)))
                                    || video_collection_contains(player.uri)
                                    || (has_container && video_collection_contains(player.container_uri)));
    if (needs_thumbnail && video_state_thumbnail_path(player.uri, player.position, 0, thumbnail, sizeof(thumbnail)) == 0
        && store_clean_frame(thumbnail, 1) != 0)
        thumbnail[0] = '\0';

    if (thumbnail[0] && has_container) {
        char container_thumbnail[PATH_MAX];
        if (video_state_thumbnail_path(
                player.container_uri, player.position, 0, container_thumbnail, sizeof(container_thumbnail)
            )
            == 0)
            copy_file(thumbnail, container_thumbnail);
    }

    if (!player.history_enabled) return;

    if (player.live) {
        video_history_update(
            has_container ? player.container_uri : player.uri, player.title, 0.0, 0.0, 0,
            thumbnail[0] ? thumbnail : NULL, player.uri, 1
        );
        return;
    }
    if (player.position < 1.0) return;

    video_history_update(
        player.uri, player.title, player.position, player.duration, complete,
        player.audio_only ? ""
        : thumbnail[0]    ? thumbnail
                          : NULL,
        NULL, 0
    );
    player.last_history_position = player.position;
    player.history_save_deadline = SDL_GetTicks() + 30000;
}

static void complete_playback(void) {
    if (player.live || player.completed) return;
    player.completed = 1;

    if (player.tracker_audio && config.video.tracker_loop) {
        player.completed = 0;
        request_seek_to_mode(0.0, 0);
        return;
    }

    if (config.video.repeat_mode == 1) {
        player.restart_requested = 1;
        stop_playback();
        return;
    }
    if (player.playlist && player.playlist_count > 1) {
        const size_t preloaded = audio_transition_playlist_index();
        const size_t next =
            audio_transition_uri_matches(preloaded < player.playlist_count ? player.playlist[preloaded].uri : NULL)
                ? preloaded
                : next_playlist_index();
        if (next != SIZE_MAX) {
            switch_playlist(next);
            return;
        }
    } else if (config.video.repeat_mode == 2) {
        player.restart_requested = 1;
        stop_playback();
        return;
    }

    const size_t folder_next = next_folder_index();
    if (folder_next != SIZE_MAX) {
        switch_folder(folder_next);
        return;
    }

    player.position = player.duration > 0.0 ? player.duration : player.position;
    player.clock_origin = player.position;
    SDL_AtomicSet(&player.paused, 1);
    if (player.sequenced_audio) Mix_PauseMusic();
    set_present_timer_idle(1);
    video_playback_ui_set_paused(1);
}

static void finish_transition_handoff(void) {
    if (!audio_transition_handoff() || !audio_transition_uri_matches(player.uri) || !player.audio_ring
        || player.audio_rate <= 0)
        return;
    const double latest = audio_transition_consumed();
    int discard = (int) llround((latest - player.transition_resume_origin) * player.audio_rate);
    if (discard < 0) discard = 0;
    SDL_LockAudio();
    const int available = audio_available();
    SDL_UnlockAudio();
    if (available < 256) return;
    if (discard >= available) {
        player.transition_resume_origin = latest;
        request_seek_to_mode(latest, 0);
        return;
    }
    SDL_LockAudio();
    player.audio_read = (player.audio_read + discard) % player.audio_capacity;
    player.audio_origin = latest;
    player.audio_frames_played = 0.0;
    player.audio_clock_valid = 1;
    player.audio_callback_position = latest;
    player.audio_callback_duration = 0.0;
    player.audio_callback_rate = 1.0;
    player.audio_callback_ticks = 0;
    SDL_UnlockAudio();
    player.position = latest;
    player.clock_origin = latest;
    player.clock_ticks = SDL_GetTicks();
    player.transition_resume_origin = 0.0;
    audio_transition_release();
    start_audio_transition();
}

static void playback_ui_tick(const uint32_t now) {
    if (!player.ui_ready || (player.ui_tick_deadline && !SDL_TICKS_PASSED(now, player.ui_tick_deadline))) return;
    player.ui_tick_deadline = now + PLAYBACK_UI_INTERVAL_MS;
    video_playback_ui_update_position(player.position, player.duration);
    video_playback_ui_tick();
}

static int prepare_blend_frame(double *timestamp) {
    if (!player.frame_blend || !player.blend_frame || !timestamp) return 0;
    int ready = 0;
    SDL_LockMutex(player.lock);
    if (player.video_count > 0) {
        AVFrame *next = player.video_queue[player.video_head];
        const double next_timestamp = normalise_position(frame_seconds(next, player.video_time_base));
        if (next_timestamp >= 0.0) {
            if (fabs(next_timestamp - player.blend_timestamp) > 0.000001) {
                av_frame_unref(player.blend_frame);
                if (av_frame_ref(player.blend_frame, next) == 0) {
                    player.blend_timestamp = next_timestamp;
                    ready = 2;
                }
            } else {
                ready = 1;
            }
            *timestamp = next_timestamp;
        }
    }
    SDL_UnlockMutex(player.lock);
    if (ready == 2) {
        if (!video_render_upload_next(player.blend_frame)) {
            player.blend_timestamp = -1.0;
            ready = 0;
        }
        av_frame_unref(player.blend_frame);
    }
    return ready != 0;
}

static void present_tick(lv_timer_t *timer __attribute__((unused))) {
    if (SDL_AtomicGet(&player.stop)) return;
    const uint32_t now = SDL_GetTicks();
    finish_transition_handoff();
    if (player.channel_switch_pending && SDL_TICKS_PASSED(now, player.channel_switch_deadline)) {
        const size_t index = player.channel_pending_index;
        player.channel_switch_pending = 0;
        switch_playlist(index);
        return;
    }
    if (player.image_preview_deadline && SDL_TICKS_PASSED(now, player.image_preview_deadline)) {
        player.image_preview_deadline = 0;
        request_seek_to_mode(player.position, 0);
    }
    if (player.live_failed && !SDL_AtomicGet(&player.buffering) && video_render_static_tick())
        display_composite_frame();
    if (player.live && SDL_AtomicGet(&player.buffering)) {
        if (!player.buffering_started_at) player.buffering_started_at = now;
        video_render_set_static(1);

        int queued_video;
        SDL_LockMutex(player.lock);
        queued_video = player.video_count;
        SDL_UnlockMutex(player.lock);

        SDL_LockAudio();
        const int queued_audio = audio_available();
        SDL_UnlockAudio();

        const int at_end = SDL_AtomicGet(&player.eof);
        const int video_ready =
            !player.video_decoder || queued_video >= LIVE_VIDEO_TARGET || (at_end && queued_video > 0);
        const int audio_target = player.audio_rate > 0 ? player.audio_rate * live_audio_target_seconds() : 0;
        const int audio_ready = !player.audio_ring || queued_audio >= audio_target || (at_end && queued_audio > 0);

        if (!player.live_failed && !player.buffering_visible && !video_playback_ui_menu_active()) {
            video_loading_show_transparent(lang.muxmedia.buffering_live_tv, NULL);
            player.buffering_visible = 1;
        }
        if (!video_ready || !audio_ready) {
            if (!player.live_failed
                && (SDL_TICKS_PASSED(now, player.buffering_started_at + LIVE_BUFFER_FAILURE_MS)
                    || (at_end && queued_video == 0 && queued_audio == 0)))
                show_live_failure();
            if (video_render_static_tick()) display_composite_frame();
            playback_ui_tick(now);
            return;
        }

        SDL_LockMutex(player.lock);
        if (player.video_count > 0) {
            const double timestamp =
                normalise_position(frame_seconds(player.video_queue[player.video_head], player.video_time_base));
            if (timestamp >= 0.0) player.position = timestamp;
        }
        SDL_UnlockMutex(player.lock);
        SDL_LockAudio();
        if (player.audio_clock_valid && player.audio_rate > 0) {
            const double audio_position = player.audio_origin + player.audio_frames_played / (double) player.audio_rate;
            const double difference = player.position - audio_position;
            if (difference > 0.0) {
                int discard = (int) llround(difference * player.audio_rate);
                const int available = audio_available();
                if (discard > available) discard = available;
                if (discard > 0) {
                    player.audio_read = (player.audio_read + discard) % player.audio_capacity;
                    player.audio_origin += (double) discard / player.audio_rate;
                }
            }
        }
        SDL_UnlockAudio();
        player.clock_origin = player.position;
        player.clock_ticks = now;
        SDL_AtomicSet(&player.buffering, 0);
        player.buffering_started_at = 0;
        player.live_failed = 0;
        video_render_set_static(0);
        video_loading_hide();
        player.buffering_visible = 0;
    }

    if (player.sequenced_audio && !SDL_AtomicGet(&player.paused) && !Mix_PlayingMusic()) {
        player.position = player.duration > 0.0 ? player.duration : player.position;
        complete_playback();
        return;
    }
    const double clock = playback_clock();
    int moved = 0;

    SDL_LockMutex(player.lock);
    if (player.live_preview_pending && player.video_count > 0) {
        while (player.video_count > 1) {
            av_frame_unref(player.video_queue[player.video_head]);
            player.video_head = (player.video_head + 1) % player.video_queue_limit;
            player.video_count--;
        }
        moved = queue_pop_locked(player.present_frame);
        if (moved) {
            const double timestamp = normalise_position(frame_seconds(player.present_frame, player.video_time_base));
            if (timestamp >= 0.0) player.position = timestamp;
            player.live_preview_pending = 0;
        }
    }
    while (!moved && player.video_count > 0) {
        AVFrame *head = player.video_queue[player.video_head];
        double timestamp = normalise_position(frame_seconds(head, player.video_time_base));
        if (timestamp < 0.0) timestamp = clock;
        if (timestamp > clock + PRESENT_EARLY_SECONDS) break;
        if (player.video_count > 1) {
            AVFrame *next = player.video_queue[(player.video_head + 1) % player.video_queue_limit];
            const double next_timestamp = normalise_position(frame_seconds(next, player.video_time_base));
            if (next_timestamp <= clock + PRESENT_EARLY_SECONDS) {
                av_frame_unref(head);
                player.video_head = (player.video_head + 1) % player.video_queue_limit;
                player.video_count--;
                SDL_CondSignal(player.condition);
                continue;
            }
        }
        moved = queue_pop_locked(player.present_frame);
        if (moved) player.position = timestamp;
        break;
    }
    const int queue_empty = player.video_count == 0;
    SDL_UnlockMutex(player.lock);

    const int present_due = blend_present_due();
    if (moved) {
        int uploaded = 0;
        if (player.frame_blend && fabs(player.blend_timestamp - player.position) <= 0.000001)
            uploaded = video_render_promote_next();
        if (!uploaded) uploaded = video_render_upload(player.present_frame);
        if (uploaded) {
            player.presented_timestamp = player.position;
            player.present_dirty = 1;
        }
        player.blend_timestamp = -1.0;
    }
    if (present_due && player.frame_blend && player.presented_timestamp >= 0.0) {
        double next_timestamp = -1.0;
        if (prepare_blend_frame(&next_timestamp) && next_timestamp > player.presented_timestamp) {
            video_render_set_blend(
                (clock - player.presented_timestamp) / (next_timestamp - player.presented_timestamp)
            );
            player.present_dirty = 1;
        }
    }
    if (video_render_seek_tick()) player.present_dirty = 1;
    if (present_due && player.present_dirty) {
        display_composite_frame();
        player.present_dirty = 0;
    }
    if (player.live && player.video_decoder && queue_empty && !SDL_AtomicGet(&player.eof)) {
        player.clock_origin = player.position;
        player.clock_ticks = SDL_GetTicks();
        begin_live_buffering();
    }
    if (!SDL_AtomicGet(&player.paused) && clock > player.position) player.position = clock;
    if (player.duration > 0.0 && player.position > player.duration) player.position = player.duration;

    playback_ui_tick(now);
    if (player.audio_only) {
        const int progress_changed =
            wasabi_audio_ui_update(player.position, player.duration, SDL_AtomicGet(&player.paused));
        if (video_render_audio_tick() || progress_changed) display_composite_frame();
    }

    if (player.history_enabled && !player.live && player.position >= 1.0
        && SDL_TICKS_PASSED(now, player.history_save_deadline)) {
        if (!SDL_AtomicGet(&player.paused) && fabs(player.position - player.last_history_position) >= 1.0)
            save_history(0, 0);
        else
            player.history_save_deadline = now + 30000;
    }

    if (SDL_AtomicGet(&player.eof) && queue_empty && audio_empty()) {
        if (player.live)
            begin_live_buffering();
        else
            complete_playback();
    }
}

static void request_seek_to_mode(double target, const int show_position) {
    if (player.live || player.duration <= 0.0) return;
    if (target < 0.0) target = 0.0;
    if (target > player.duration) target = player.duration;
    if (show_position && !player.audio_only && !player.sequenced_audio && fabs(target - player.position) > 0.001)
        video_render_seek_effect(target > player.position ? 1 : -1);
    player.position = target;
    player.clock_origin = target;
    player.clock_ticks = SDL_GetTicks();
    player.completed = 0;
    SDL_AtomicSet(&player.eof, 0);
    if (player.sequenced_audio) {
        Mix_SetMusicPosition(target);
        if (show_position) video_playback_ui_show_position(player.position, player.duration);
        return;
    }
    audio_reset();
    SDL_LockMutex(player.lock);
    player.seek_target = target;
    player.seek_pending = 1;
    SDL_CondSignal(player.condition);
    SDL_UnlockMutex(player.lock);
    if (show_position) video_playback_ui_show_position(player.position, player.duration);
}

static void request_seek_to(const double target) {
    request_seek_to_mode(target, 1);
}

static void request_seek(const double amount) {
    request_seek_to(player.position + amount);
}

static void seek_back(void) {
    request_seek(-10.0);
}

static void seek_forward(void) {
    request_seek(10.0);
}

static void seek_back_long(void) {
    request_seek(-60.0);
}

static void seek_forward_long(void) {
    request_seek(60.0);
}

static void toggle_pause(void) {
    if (video_playback_ui_menu_active()) return;
    if (SDL_AtomicGet(&player.paused)) {
        resume_playback();
        return;
    } else {
        player.clock_origin = playback_clock();
        SDL_AtomicSet(&player.paused, 1);
        if (player.sequenced_audio) Mix_PauseMusic();
        set_present_timer_idle(1);
    }
    video_playback_ui_set_paused(SDL_AtomicGet(&player.paused));
}

static void resume_playback(void) {
    if (player.completed && !player.live) {
        player.completed = 0;
        if (player.sequenced_audio) {
            Mix_PlayMusic(player.sequenced_audio, 0);
            Mix_SetMusicPosition(0.0);
            player.position = 0.0;
            player.clock_origin = 0.0;
        } else {
            request_seek_to_mode(0.0, 0);
        }
    }
    if (player.live
        && (config.wasabi.live_quality != player.opened_live_quality
            || config.wasabi.live_buffer != player.opened_live_buffer)) {
        player.restart_requested = 1;
        show_live_loading(player.title);
        stop_playback();
        return;
    }
    if (player.live_failed) {
        SDL_AtomicSet(&player.paused, 0);
        set_present_timer_idle(0);
        video_playback_ui_set_paused(0);
        return;
    }
    if (player.live) {
        SDL_LockMutex(player.lock);
        while (player.video_count > LIVE_VIDEO_TARGET) {
            av_frame_unref(player.video_queue[player.video_head]);
            player.video_head = (player.video_head + 1) % player.video_queue_limit;
            player.video_count--;
        }
        SDL_CondSignal(player.condition);
        SDL_UnlockMutex(player.lock);

        SDL_LockAudio();
        const int keep = player.audio_rate > 0 ? player.audio_rate * live_audio_target_seconds() : 0;
        const int available = audio_available();
        if (keep > 0 && available > keep) {
            const int discard = available - keep;
            player.audio_read = (player.audio_read + discard) % player.audio_capacity;
            if (player.audio_clock_valid) player.audio_origin += (double) discard / player.audio_rate;
        }
        SDL_UnlockAudio();
        begin_live_buffering();
    }
    player.clock_origin = player.position;
    player.clock_ticks = SDL_GetTicks();
    if (!player.live) {
        SDL_LockAudio();
        if (player.audio_clock_valid && player.audio_rate > 0)
            player.audio_origin = player.position - (double) player.audio_frames_played / player.audio_rate;
        SDL_UnlockAudio();
    }
    if (player.audio_ring) Mix_HookMusic(audio_callback, &audio_hook_owner);
    if (player.sequenced_audio) Mix_ResumeMusic();
    SDL_AtomicSet(&player.paused, 0);
    set_present_timer_idle(0);
    if (player.present_timer) {
        lv_timer_resume(player.present_timer);
        lv_timer_ready(player.present_timer);
    }
    if (player.lock) {
        SDL_LockMutex(player.lock);
        SDL_CondSignal(player.condition);
        SDL_UnlockMutex(player.lock);
    }
    video_playback_ui_set_paused(0);
}

static int save_bookmark(const char *name, const int quick) {
    char thumbnail[PATH_MAX] = "";
    int thumbnail_existed = 0;
    if (!player.audio_only) {
        if (video_state_thumbnail_path(player.uri, player.position, 1, thumbnail, sizeof(thumbnail)) != 0) return 0;
        thumbnail_existed = file_exist(thumbnail);
        if (store_clean_frame(thumbnail, 0) != 0) return 0;
    }

    const int result = quick ? video_bookmark_set_quick(
                                   player.uri, player.title, player.position, player.duration,
                                   player.audio_only ? ""
                                   : thumbnail[0]    ? thumbnail
                                                     : NULL
                               )
                             : video_bookmark_add(
                                   player.uri, player.title, name, player.position, player.duration,
                                   player.audio_only ? ""
                                   : thumbnail[0]    ? thumbnail
                                                     : NULL
                               );
    if (result < 0) {
        if (thumbnail[0] && !thumbnail_existed) remove(thumbnail);
        return 0;
    }
    return 1;
}

static void add_bookmark(const char *name) {
    if (player.live || player.position < 1.0) return;
    if (!save_bookmark(name, 0)) play_sound(snd_error);
}

static void save_quick_bookmark(void) {
    if (player.live || player.position < 1.0) return;
    if (!save_bookmark(NULL, 1)) {
        play_sound(snd_error);
        return;
    }
    play_sound(snd_confirm);
    toast_message(lang.muxmedia.save_bookmark, tst_wait_m);
}

static void load_quick_bookmark(void) {
    double position = 0.0;
    if (player.live || !video_bookmark_find_quick(player.uri, &position)) {
        play_sound(snd_error);
        return;
    }
    request_seek_to(position);
    resume_playback();
    play_sound(snd_confirm);
    toast_message(lang.muxmedia.load_bookmark, tst_wait_m);
}

static void toggle_header(void) {
    config.video.header_visibility = (config.video.header_visibility + 1) % 6;
    video_playback_ui_header_changed();
}

static void toggle_repeat(void) {
    if (player.live) return;
    config.video.repeat_mode = (config.video.repeat_mode + 1) % 3;
    video_player_modes_changed();
    play_sound(snd_option);
}

static void toggle_shuffle(void) {
    if (player.live) return;
    config.video.shuffle = !config.video.shuffle;
    video_player_modes_changed();
    play_sound(snd_option);
}

static void update_speed_indicator(void) {
    if (player.fast_forward_active) {
        static const char *const values[] = {"2x", "3x", "4x", "8x"};
        video_playback_ui_show_speed(values[config.video.fast_forward_speed], "fastforward");
    } else if (player.slow_motion_active) {
        static const char *const values[] = {"1/2x", "1/4x", "1/8x"};
        video_playback_ui_show_speed(values[config.video.slow_motion_speed], "slowmotion");
    } else {
        video_playback_ui_show_speed(NULL, NULL);
    }
}

static void set_fast_forward(const int active) {
    if (player.live || player.sequenced_audio) return;
    if (player.fast_forward_active == active && (!active || !player.slow_motion_active)) return;
    player.fast_forward_active = active;
    if (active) player.slow_motion_active = 0;
    set_speed_target(active ? fast_forward_speed() : player.slow_motion_active ? slow_motion_speed() : 1.0);
    update_speed_indicator();
}

static void set_slow_motion(const int active) {
    if (player.live || player.sequenced_audio) return;
    if (player.slow_motion_active == active && (!active || !player.fast_forward_active)) return;
    player.slow_motion_active = active;
    if (active) player.fast_forward_active = 0;
    set_speed_target(active ? slow_motion_speed() : player.fast_forward_active ? fast_forward_speed() : 1.0);
    update_speed_indicator();
}

static void stop_playback(void) {
    SDL_AtomicSet(&player.stop, 1);
    if (player.lock) {
        SDL_LockMutex(player.lock);
        SDL_CondSignal(player.condition);
        SDL_UnlockMutex(player.lock);
    }
    mux_input_stop();
}

static void switch_folder(const size_t index) {
    if (player.folder_selection) *player.folder_selection = index;
    player.folder_switch_requested = 1;
    player.playlist_switch_requested = 1;
    stop_playback();
}

static void switch_playlist(const size_t index) {
    if (!player.playlist || player.playlist_count < 2 || index >= player.playlist_count) return;
    if (player.playlist_selection) *player.playlist_selection = index;
    if (player.live) {
        video_playback_ui_prepare_channel_transition();
        show_live_loading(
            player.playlist[index].title && player.playlist[index].title[0] ? player.playlist[index].title
                                                                            : lang.muxmedia.channels
        );
        player.buffering_visible = 1;
    }
    player.playlist_switch_requested = 1;
    stop_playback();
}

static void step_track(const int direction) {
    if (!player.audio_only || player.live || !player.playlist || player.playlist_count < 2) return;

    size_t index;
    if (direction > 0)
        index = player.playlist_index + 1 < player.playlist_count ? player.playlist_index + 1 : 0;
    else
        index = player.playlist_index > 0 ? player.playlist_index - 1 : player.playlist_count - 1;

    play_sound(snd_navigate);
    switch_playlist(index);
}

static void step_channel(const int direction) {
    if (!player.playlist_channels || player.playlist_count < 2) return;
    const size_t count = player.playlist_count;
    const size_t current = player.channel_switch_pending ? player.channel_pending_index : player.playlist_index;
    const size_t index = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
    if (index == player.playlist_index) {
        player.channel_switch_pending = 0;
        video_playback_ui_hide_channel();
        play_sound(snd_navigate);
        return;
    }
    player.channel_pending_index = index;
    player.channel_switch_pending = 1;
    player.channel_switch_deadline = SDL_GetTicks() + CHANNEL_SWITCH_DELAY_MS;
    video_playback_ui_show_channel(index);
    play_sound(snd_navigate);
}

static void apply_ui_action(const video_ui_action action) {
    switch (action) {
        case video_ui_action_opened:
            set_fast_forward(0);
            set_slow_motion(0);
            if (player.buffering_visible) {
                video_loading_hide();
                player.buffering_visible = 0;
            }
            if (player.live) {
                player.menu_resume_playback = 0;
                break;
            }
            player.menu_resume_playback = !SDL_AtomicGet(&player.paused);
            if (player.menu_resume_playback) {
                player.clock_origin = playback_clock();
                SDL_AtomicSet(&player.paused, 1);
                if (player.sequenced_audio) Mix_PauseMusic();
            }
            set_present_timer_idle(1);
            break;
        case video_ui_action_closed:
            if (player.live) break;
            if (player.menu_resume_playback)
                resume_playback();
            else
                video_playback_ui_set_paused(1);
            break;
        case video_ui_action_load_bookmark: {
            const double position = video_playback_ui_selected_position();
            if (position >= 0.0) request_seek_to(position);
            resume_playback();
            break;
        }
        case video_ui_action_load_playlist:
            switch_playlist(video_playback_ui_selected_playlist());
            break;
        case video_ui_action_switch_content:
            player.content_switch_requested = 1;
            video_loading_show(lang.generic.loading);
            stop_playback();
            break;
        case video_ui_action_restart:
            if (player.live) {
                player.restart_requested = 1;
                show_live_loading(player.title);
                stop_playback();
            } else {
                request_seek_to(0.0);
                resume_playback();
            }
            break;
        case video_ui_action_stop:
            stop_playback();
            break;
        default:
            break;
    }
}

static void handle_menu_press(void) {
    if (player.channel_switch_pending) {
        player.channel_switch_pending = 0;
        video_playback_ui_hide_channel();
    }
    if (!video_playback_ui_naming_active()) video_playback_ui_menu_press();
}

static void handle_menu(void) {
    if (video_playback_ui_naming_active()) return;
    if (video_playback_ui_menu_release()) return;
    const int was_open = video_playback_ui_menu_active();
    apply_ui_action(video_playback_ui_toggle_menu());
    play_sound(was_open ? snd_info_close : snd_info_open);
}

static void handle_confirm(void) {
    if (video_playback_ui_modal_active() || video_playback_ui_naming_active() || video_playback_ui_menu_active())
        player.ui_input_consumed = 1;
    if (video_playback_ui_modal_active()) {
        video_playback_ui_modal_confirm();
        return;
    }
    if (video_playback_ui_naming_active()) {
        char name[128];
        if (video_playback_ui_name_press(name, sizeof(name))) {
            add_bookmark(name);
            video_playback_ui_bookmarks_refresh();
        }
        return;
    }
    if (video_playback_ui_menu_active()) {
        if (!video_playback_ui_confirmable()) return;
        play_sound(snd_confirm);
        apply_ui_action(video_playback_ui_confirm());
    }
}

static void handle_back(void) {
    if (video_playback_ui_modal_active() || video_playback_ui_naming_active() || video_playback_ui_menu_active())
        player.ui_input_consumed = 1;
    if (video_playback_ui_modal_active()) {
        video_playback_ui_modal_cancel();
        return;
    }
    if (video_playback_ui_naming_active()) {
        video_playback_ui_name_backspace();
        return;
    }
    if (video_playback_ui_menu_active()) {
        play_sound(snd_back);
        apply_ui_action(video_playback_ui_back());
    }
}

static void handle_bookmark(void) {
    if (video_playback_ui_naming_active()) {
        video_playback_ui_name_space();
    } else if (video_playback_ui_bookmarks_active()) {
        play_sound(snd_confirm);
        apply_ui_action(video_playback_ui_begin_bookmark_name());
    } else if (video_playback_ui_y_actionable()) {
        play_sound(snd_confirm);
        video_playback_ui_collect();
    }
}

static void handle_delete_bookmark(void) {
    if (video_playback_ui_naming_active()) {
        video_playback_ui_name_cancel();
        return;
    }
    if (!video_playback_ui_bookmarks_active()) {
        video_playback_ui_extra();
        return;
    }
    play_sound(snd_confirm);
    video_playback_ui_request_bookmark_delete();
}

static void handle_clear_bookmark_name(void) {
    if (video_playback_ui_naming_active()) video_playback_ui_name_clear();
}

static void handle_start(void) {
    if (video_playback_ui_naming_active()) {
        char name[128];
        if (video_playback_ui_name_finish(name, sizeof(name))) {
            add_bookmark(name);
            video_playback_ui_bookmarks_refresh();
        }
        return;
    }
}

static void handle_up(void) {
    if (video_playback_ui_modal_active())
        video_playback_ui_modal_move(-1);
    else if (video_playback_ui_naming_active())
        video_playback_ui_name_move(1, -1);
    else if (!video_playback_ui_menu_active() && player.playlist_channels) {
        player.ui_input_consumed = 1;
        step_channel(-1);
    } else
        video_playback_ui_move(1, -1);
}

static void handle_down(void) {
    if (video_playback_ui_modal_active())
        video_playback_ui_modal_move(1);
    else if (video_playback_ui_naming_active())
        video_playback_ui_name_move(1, 1);
    else if (!video_playback_ui_menu_active() && player.playlist_channels) {
        player.ui_input_consumed = 1;
        step_channel(1);
    } else
        video_playback_ui_move(1, 1);
}

static void handle_up_hold(void) {
    if (video_playback_ui_modal_active())
        video_playback_ui_modal_move(-1);
    else if (video_playback_ui_naming_active())
        video_playback_ui_name_move(1, -1);
    else if (!video_playback_ui_menu_active() && player.playlist_channels) {
        player.ui_input_consumed = 1;
        step_channel(-1);
    } else
        video_playback_ui_move_held(1, -1);
}

static void handle_down_hold(void) {
    if (video_playback_ui_modal_active())
        video_playback_ui_modal_move(1);
    else if (video_playback_ui_naming_active())
        video_playback_ui_name_move(1, 1);
    else if (!video_playback_ui_menu_active() && player.playlist_channels) {
        player.ui_input_consumed = 1;
        step_channel(1);
    } else
        video_playback_ui_move_held(1, 1);
}

static void handle_left(void) {
    if (video_playback_ui_naming_active())
        video_playback_ui_name_move(0, -1);
    else if (video_playback_ui_menu_active())
        video_playback_ui_change(-1);
}

static void handle_right(void) {
    if (video_playback_ui_naming_active())
        video_playback_ui_name_move(0, 1);
    else if (video_playback_ui_menu_active())
        video_playback_ui_change(1);
}

static void handle_page_up(void) {
    if (video_playback_ui_naming_active())
        video_playback_ui_name_layer(-1);
    else if (video_playback_ui_menu_active())
        video_playback_ui_section(-1);
}

static void handle_page_down(void) {
    if (video_playback_ui_naming_active())
        video_playback_ui_name_layer(1);
    else if (video_playback_ui_menu_active())
        video_playback_ui_section(1);
}

static int seek_hotkey_direction(const mux_input_type input) {
    if (input == config.video.hotkey_seek_back || input == config.video.hotkey_seek_back_long) return -1;
    if (input == config.video.hotkey_seek_forward || input == config.video.hotkey_seek_forward_long) return 1;
    return 0;
}

static int handle_seek_hotkey(const mux_input_type input) {
    if (input == config.video.hotkey_seek_back)
        seek_back();
    else if (input == config.video.hotkey_seek_forward)
        seek_forward();
    else if (input == config.video.hotkey_seek_back_long)
        seek_back_long();
    else if (input == config.video.hotkey_seek_forward_long)
        seek_forward_long();
    else
        return 0;
    return 1;
}

static void handle_configured_hotkey(const mux_input_type input, const mux_input_action action) {
    if (action == mux_input_release && seek_hotkey_direction(input)) video_render_seek_release();
    if (player.ui_input_consumed) {
        player.ui_input_consumed = 0;
        return;
    }
    if (video_playback_ui_menu_active() || video_playback_ui_naming_active()) return;
    const int menu_combo = mux_input_pressed(mux_input_menu);
    if (!player.live && !player.sequenced_audio) {
        if (input == config.video.hotkey_fast_forward && config.video.fast_forward_mode
            && (menu_combo || player.fast_forward_active)) {
            if (action == mux_input_release && config.video.fast_forward_mode == 1) {
                set_fast_forward(0);
            } else if (action == mux_input_press && menu_combo) {
                video_playback_ui_menu_consume();
                if (config.video.fast_forward_mode == 1)
                    set_fast_forward(1);
                else
                    set_fast_forward(!player.fast_forward_active);
            }
            return;
        }
        if (input == config.video.hotkey_slow_motion && config.video.slow_motion_mode
            && (menu_combo || player.slow_motion_active)) {
            if (action == mux_input_release && config.video.slow_motion_mode == 1) {
                set_slow_motion(0);
            } else if (action == mux_input_press && menu_combo) {
                video_playback_ui_menu_consume();
                if (config.video.slow_motion_mode == 1)
                    set_slow_motion(1);
                else
                    set_slow_motion(!player.slow_motion_active);
            }
            return;
        }
    }
    if (action == mux_input_hold) {
        const int direction = seek_hotkey_direction(input);
        if (menu_combo || !direction) return;
        if (!player.live && player.duration > 0.0 && !player.audio_only && !player.sequenced_audio)
            video_render_seek_hold(direction);
        handle_seek_hotkey(input);
        return;
    }
    if (action != mux_input_press) return;
    if (menu_combo) {
        if (input == config.video.hotkey_save_bookmark)
            save_quick_bookmark();
        else if (input == config.video.hotkey_load_bookmark)
            load_quick_bookmark();
        else if (input == config.video.hotkey_header)
            toggle_header();
        else if (input == config.video.hotkey_quit)
            stop_playback();
        else
            return;
        video_playback_ui_menu_consume();
        return;
    }
    if (input == config.video.hotkey_pause)
        toggle_pause();
    else if (handle_seek_hotkey(input))
        return;
    else if (input == config.video.hotkey_repeat)
        toggle_repeat();
    else if (input == config.video.hotkey_shuffle)
        toggle_shuffle();
    else if (input == mux_input_l2)
        step_track(-1);
    else if (input == mux_input_r2)
        step_track(1);
}

static void player_cleanup(void) {
    const int preserve_transition = player.playlist_switch_requested && audio_transition_handoff();
    video_loading_hide();
    SDL_AtomicSet(&player.stop, 1);
    if (player.lock) {
        SDL_LockMutex(player.lock);
        SDL_CondBroadcast(player.condition);
        SDL_UnlockMutex(player.lock);
    }
    if (player.demux_thread) SDL_WaitThread(player.demux_thread, NULL);
    player.demux_thread = NULL;
    if (player.decode_thread) SDL_WaitThread(player.decode_thread, NULL);
    player.decode_thread = NULL;
    if (player.present_timer) lv_timer_del(player.present_timer);
    player.present_timer = NULL;
    if (!preserve_transition && Mix_GetMusicHookData() == &audio_hook_owner) Mix_HookMusic(NULL, NULL);
    if (!preserve_transition) audio_transition_cancel();
    if (player.sequenced_audio) {
        Mix_SetPostMix(NULL, NULL);
        Mix_HaltMusic();
        Mix_FreeMusic(player.sequenced_audio);
        player.sequenced_audio = NULL;
    }
    if (player.audio_only) wasabi_audio_ui_shutdown();
    if (player.ui_ready) video_playback_ui_shutdown();
    player.ui_ready = 0;
    video_render_close();
    display_set_idle_saver_suppressed_query(NULL);
    display_set_ui_hidden(0);

    if (player.lock) {
        SDL_LockMutex(player.lock);
        queue_clear_locked();
        packet_queue_clear_locked();
        SDL_UnlockMutex(player.lock);
    }
    for (int i = 0; i < VIDEO_QUEUE_SIZE; ++i)
        av_frame_free(&player.video_queue[i]);
    av_frame_free(&player.decode_frame);
    av_frame_free(&player.present_frame);
    av_frame_free(&player.blend_frame);
    av_frame_free(&player.filter_frame);
    av_packet_free(&player.packet);
    av_packet_free(&player.demux_packet);
    avcodec_free_context(&player.video_decoder);
    avcodec_free_context(&player.audio_decoder);
    avformat_close_input(&player.format);
    swr_free(&player.resample);
    avfilter_graph_free(&player.filter_graph);
    free(player.audio_ring);
    free(player.audio_convert);
    free(player.audio_speed_convert);
    free(player.live_variants);
    wasabi_audio_cleanup(&player.audio_information);
    if (player.condition) SDL_DestroyCond(player.condition);
    if (player.lock) SDL_DestroyMutex(player.lock);
    player.condition = NULL;
    player.lock = NULL;
}

static int player_open_failed(const char *stage, const int error) {
    char detail[AV_ERROR_MAX_STRING_SIZE] = "";
    if (error < 0) av_strerror(error, detail, sizeof(detail));
    if (detail[0])
        LOG_ERROR("muxmedia", "Playback initialisation failed at %s: %s", stage, detail);
    else
        LOG_ERROR("muxmedia", "Playback initialisation failed at %s", stage);
    fprintf(stderr, "Wasabi playback initialisation failed at %s%s%s\n", stage, detail[0] ? ": " : "", detail);
    return 0;
}

static Mix_Music *load_sequenced(const char *uri) {
    SDL_RWops *source = SDL_RWFromFile(uri, "rb");
    if (!source) return NULL;
    Uint8 header[12];
    if (SDL_RWread(source, header, 1, sizeof(header)) != sizeof(header) || memcmp(header, "RIFF", 4) != 0
        || memcmp(header + 8, "RMID", 4) != 0) {
        SDL_RWclose(source);
        return Mix_LoadMUS(uri);
    }
    Uint8 chunk[4];
    while (SDL_RWread(source, chunk, 1, sizeof(chunk)) == sizeof(chunk)) {
        const Sint64 size = SDL_ReadLE32(source);
        if (memcmp(chunk, "data", 4) == 0) return Mix_LoadMUSType_RW(source, MUS_MID, 1);
        if (SDL_RWseek(source, size + (size & 1), RW_SEEK_CUR) < 0) break;
    }
    SDL_RWclose(source);
    return NULL;
}

static int player_open_sequenced(const char *uri, const char *title, const video_player_options *options) {
    memset(&player, 0, sizeof(player));
    player.speed_current = player.speed_start = player.speed_target = 1.0;
    player.clock_speed = 1.0;
    SDL_AtomicSet(&player.speed_q16, 65536);
    player.video_stream = -1;
    player.audio_stream = -1;
    player.video_queue_limit = LOCAL_VIDEO_QUEUE_SIZE;
    player.audio_only = 1;
    player.history_enabled = options->keep_history;
    player.playlist = options->playlist;
    player.playlist_count = options->playlist_count;
    player.playlist_index = options->playlist_index;
    player.playlist_channels = options->playlist_channels;
    player.playlist_selection = options->playlist_selection;
    player.folder_playlist = options->folder_playlist;
    player.clock_ticks = SDL_GetTicks();
    snprintf(player.uri, sizeof(player.uri), "%s", uri);
    snprintf(player.title, sizeof(player.title), "%s", title && title[0] ? title : uri);
    snprintf(
        player.container_uri, sizeof(player.container_uri), "%s", options->container_uri ? options->container_uri : ""
    );

    player.lock = SDL_CreateMutex();
    player.condition = SDL_CreateCond();
    if (!player.lock || !player.condition) return player_open_failed("playback synchronisation", AVERROR(ENOMEM));

    Uint16 audio_format = 0;
    if (!Mix_QuerySpec(&player.audio_rate, &audio_format, &player.audio_channels)) {
        if (!init_audio_backend()) return player_open_failed("audio output", 0);
        Mix_QuerySpec(&player.audio_rate, &audio_format, &player.audio_channels);
    }
    char soundfont[PATH_MAX];
    if (soundfont_resolve(config.settings.general.soundfont, soundfont, sizeof(soundfont)))
        Mix_SetSoundFonts(soundfont);
    player.sequenced_audio = load_sequenced(uri);
    if (!player.sequenced_audio) return player_open_failed("MIDI decoder", 0);
    player.duration = Mix_MusicDuration(player.sequenced_audio);
    if (player.duration < 0.0) player.duration = 0.0;
    wasabi_audio_metadata(NULL, player.uri, player.title, &player.audio_information);
    snprintf(player.audio_information.format, sizeof(player.audio_information.format), "MIDI");

    video_state_entry saved = {0};
    double initial_position = options->start_position;
    if (initial_position <= 0.0 && options->resume && video_history_find(uri, &saved) && saved.position > 1.0
        && (saved.duration <= 0.0 || saved.position < saved.duration * 0.95))
        initial_position = saved.position;
    free(saved.uri);
    free(saved.title);
    free(saved.thumbnail);
    free(saved.name);

    video_render_set_content(player.uri, 0);
    video_render_set_audio(1);
    wasabi_settings_set_audio(1);
    if (!video_render_open()) return player_open_failed("video renderer", 0);
    if (!video_playback_ui_init(
            player.title, player.uri, player.container_uri[0] ? player.container_uri : player.uri, 0, player.playlist,
            player.playlist_count, player.playlist_index, player.playlist_channels
        ))
        return player_open_failed("playback interface", 0);
    player.ui_ready = 1;
    if (!wasabi_audio_ui_init(&player.audio_information)) return player_open_failed("audio interface", 0);

    Mix_VolumeMusic(config.video.volume * MIX_MAX_VOLUME / 100);
    Mix_SetPostMix(audio_postmix, NULL);
    if (Mix_PlayMusic(player.sequenced_audio, 0) < 0) return player_open_failed("MIDI playback", 0);
    if (initial_position > 0.0 && (player.duration <= 0.0 || initial_position < player.duration)) {
        Mix_SetMusicPosition(initial_position);
        player.position = initial_position;
    }
    player.clock_origin = player.position;
    player.clock_ticks = SDL_GetTicks();
    player.last_history_position = player.position;
    player.history_save_deadline = SDL_GetTicks() + 30000;
    wasabi_audio_ui_update(player.position, player.duration, 0);
    display_composite_frame();
    player.present_interval = PRESENT_INTERVAL_NORMAL_MS;
    player.present_timer = lv_timer_create(present_tick, player.present_interval, NULL);
    return player.present_timer != NULL;
}

static void subsong_playlist_free(void) {
    video_library_free(subsongs.entries, subsongs.count);
    memset(&subsongs, 0, sizeof(subsongs));
}

static int subsong_playlist_build(const char *path, const int count) {
    if (subsongs.entries && subsongs.count == (size_t) count && strcmp(subsongs.source, path) == 0) return 1;

    subsong_playlist_free();
    subsongs.entries = calloc((size_t) count, sizeof(*subsongs.entries));
    if (!subsongs.entries) return 0;
    subsongs.count = (size_t) count;
    snprintf(subsongs.source, sizeof(subsongs.source), "%s", path);

    char name[PATH_MAX];
    video_title_from_uri(path, name, sizeof(name));
    for (int index = 0; index < count; index++) {
        char text[PATH_MAX + 32];
        snprintf(text, sizeof(text), "%s#%d", path, index + 1);
        subsongs.entries[index].uri = strdup(text);
        snprintf(text, sizeof(text), "%s %d/%d", name, index + 1, count);
        subsongs.entries[index].title = strdup(text);
        if (!subsongs.entries[index].uri || !subsongs.entries[index].title) {
            subsong_playlist_free();
            return 0;
        }
    }
    return 1;
}

static void subsong_playlist_attach(void) {
    if ((player.playlist && !player.folder_playlist) || player.live || !player.audio_only) return;

    wasabi_audio_source source;
    audio_source_resolve(player.uri, &source);
    int current = 1;
    const int count = audio_source_subsongs(player.format, &source, &current);
    if (count < 2 || !subsong_playlist_build(source.path, count)) return;

    subsongs.selected = (size_t) current - 1;
    if (player.playlist) {
        player.folder = player.playlist;
        player.folder_count = player.playlist_count;
        player.folder_index = player.playlist_index;
        player.folder_selection = player.playlist_selection;
    }
    player.playlist = subsongs.entries;
    player.playlist_count = subsongs.count;
    player.playlist_index = subsongs.selected;
    player.playlist_selection = &subsongs.selected;
}

static int output_sample_rate(void) {
    int rate = 0;
    int channels = 0;
    Uint16 format = 0;
    return Mix_QuerySpec(&rate, &format, &channels) ? rate : 0;
}

static int best_video_stream(const AVFormatContext *format) {
    if (!format) return -1;
    int best = -1;
    int64_t best_area = -1;
    for (unsigned int index = 0; index < format->nb_streams; index++) {
        const AVStream *stream = format->streams[index];
        if (!stream || stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO
            || (stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
            continue;
        const int64_t area = (int64_t) stream->codecpar->width * stream->codecpar->height;
        if (best < 0 || area > best_area) {
            best = (int) index;
            best_area = area;
        }
    }
    return best;
}

static int player_open(const char *uri, const char *title, const video_player_options *options) {
    if (video_path_is_sequenced(uri)) return player_open_sequenced(uri, title, options);
    if (!network_ready) {
        const int result = avformat_network_init();
        if (result < 0) return player_open_failed("network", result);
        network_ready = 1;
    }

    const int transition_resume = audio_transition_handoff() && audio_transition_uri_matches(uri);
    const double transition_position = transition_resume ? audio_transition_consumed() : 0.0;
    if (!transition_resume) audio_transition_cancel();
    memset(&player, 0, sizeof(player));
    player.speed_current = player.speed_start = player.speed_target = 1.0;
    player.clock_speed = 1.0;
    SDL_AtomicSet(&player.speed_q16, 65536);
    player.transition_resume_origin = transition_position;
    video_codec_unsupported = 0;
    player.video_stream = -1;
    player.audio_stream = -1;
    player.history_enabled = options->keep_history;
    player.deinterlace = options->deinterlace;
    player.live = options->live;
    player.opened_live_quality = config.wasabi.live_quality;
    player.opened_live_buffer = config.wasabi.live_buffer;
    player.packet_limit = (size_t) config.wasabi.live_buffer * 1024U * 1024U;
    SDL_AtomicSet(&player.buffering, player.live);
    player.buffering_started_at = player.live ? SDL_GetTicks() : 0;
    player.buffering_visible = player.live;
    player.playlist = options->playlist;
    player.playlist_count = options->playlist_count;
    player.playlist_index = options->playlist_index;
    player.playlist_channels = options->playlist_channels;
    player.playlist_selection = options->playlist_selection;
    player.folder_playlist = options->folder_playlist;
    player.clock_ticks = SDL_GetTicks();
    snprintf(player.uri, sizeof(player.uri), "%s", uri);
    snprintf(player.title, sizeof(player.title), "%s", title && title[0] ? title : uri);
    snprintf(
        player.container_uri, sizeof(player.container_uri), "%s", options->container_uri ? options->container_uri : ""
    );

    player.format = avformat_alloc_context();
    if (!player.format) return player_open_failed("format allocation", AVERROR(ENOMEM));
    player.format->interrupt_callback.callback = interrupt_io;
    player.format->interrupt_callback.opaque = &player;

    AVDictionary *open_options = NULL;
    if (player.live) {
        char buffer_size[32];
        snprintf(buffer_size, sizeof(buffer_size), "%zu", player.packet_limit);
        av_dict_set(&open_options, "protocol_whitelist", "file,http,https,tcp,tls,crypto,data", 0);
        av_dict_set(&open_options, "rw_timeout", "15000000", 0);
        av_dict_set(&open_options, "reconnect", "1", 0);
        av_dict_set(&open_options, "reconnect_streamed", "1", 0);
        av_dict_set(&open_options, "reconnect_delay_max", "5", 0);
        av_dict_set(&open_options, "buffer_size", buffer_size, 0);
        av_dict_set(&open_options, "http_persistent", "1", 0);
        av_dict_set(&open_options, "http_multiple", "0", 0);
        av_dict_set(&open_options, "user_agent", "MustardOS-Wasabi/1.0", 0);
    }
    char *resolved_uri = player.live ? resolve_live_manifest(uri) : NULL;
    SDL_AtomicSet(&player.interrupt_ticks, (int) SDL_GetTicks());
    const int open_result =
        player.live ? avformat_open_input(&player.format, resolved_uri ? resolved_uri : uri, NULL, &open_options)
                    : audio_source_open(&player.format, uri, output_sample_rate(), &open_options);
    SDL_AtomicSet(&player.interrupt_ticks, 0);
    free(resolved_uri);
    av_dict_free(&open_options);
    if (open_result < 0) return player_open_failed("input", open_result);
    int live_video = -1;
    int live_audio = -1;
    if (player.live && !player.live_manifest_resolved) {
        const int automatic = av_find_best_stream(player.format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (automatic >= 0) {
            live_video = select_live_video_stream(automatic);
            live_audio = live_audio_stream_for_video(live_video);
            discard_unselected_live_streams(live_video, live_audio);
        }
    }
    const int stream_info_result = avformat_find_stream_info(player.format, NULL);
    if (stream_info_result < 0) return player_open_failed("stream information", stream_info_result);

    player.video_stream = live_video >= 0 ? live_video
                          : player.live   ? av_find_best_stream(player.format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)
                                          : best_video_stream(player.format);
    if (player.live && !player.live_manifest_resolved && player.video_stream >= 0 && live_video < 0) {
        free(player.live_variants);
        player.live_variants = NULL;
        player.live_variant_count = 0;
        player.video_stream = select_live_video_stream(player.video_stream);
    }
    player.audio_stream = live_audio >= 0 ? live_audio
                                          : av_find_best_stream(
                                                player.format, AVMEDIA_TYPE_AUDIO, -1,
                                                player.video_stream >= 0 ? player.video_stream : -1, NULL, 0
                                            );
    if (player.video_stream < 0 && player.audio_stream < 0)
        return player_open_failed("stream selection", AVERROR_STREAM_NOT_FOUND);
    if (player.live) {
        for (unsigned int index = 0; index < player.format->nb_streams; index++)
            player.format->streams[index]->discard =
                (int) index == player.video_stream || (int) index == player.audio_stream ? AVDISCARD_DEFAULT
                                                                                         : AVDISCARD_ALL;
    }
    if (player.format->duration != AV_NOPTS_VALUE) player.duration = (double) player.format->duration / AV_TIME_BASE;
    player.audio_only = !player.live && player.audio_stream >= 0 && player.video_stream < 0;
    player.tracker_audio = player.audio_only && player.format && player.format->iformat && player.format->iformat->name
                           && strstr(player.format->iformat->name, "openmpt") != NULL;
    wasabi_settings_set_audio(player.audio_only);
    if (player.audio_only) wasabi_audio_metadata(player.format, player.uri, player.title, &player.audio_information);
    subsong_playlist_attach();

    player.lock = SDL_CreateMutex();
    player.condition = SDL_CreateCond();
    if (!player.lock || !player.condition) return player_open_failed("playback synchronisation", AVERROR(ENOMEM));

    if (player.video_stream >= 0) {
        player.video_time_base = player.format->streams[player.video_stream]->time_base;
        player.video_decoder = open_decoder(player.video_stream, options->hardware_decode);
        if (video_codec_unsupported) {
            avcodec_free_context(&player.video_decoder);
            player.format->streams[player.video_stream]->discard = AVDISCARD_ALL;
        } else if (!player.video_decoder) {
            return player_open_failed("video decoder", AVERROR_DECODER_NOT_FOUND);
        }
    }
    if (player.audio_stream >= 0) {
        player.audio_time_base = player.format->streams[player.audio_stream]->time_base;
        player.audio_decoder = open_decoder(player.audio_stream, 0);
    }
    player.video_queue_limit = live_video_queue_limit();

    player.packet = av_packet_alloc();
    if (player.live) player.demux_packet = av_packet_alloc();
    player.decode_frame = av_frame_alloc();
    if (player.video_decoder) {
        player.present_frame = av_frame_alloc();
        player.blend_frame = av_frame_alloc();
        player.filter_frame = av_frame_alloc();
    }
    if (!player.packet || (player.live && !player.demux_packet) || !player.decode_frame
        || (player.video_decoder && (!player.present_frame || !player.blend_frame || !player.filter_frame))
        || !player.lock || !player.condition)
        return player_open_failed("playback buffers", AVERROR(ENOMEM));
    if (player.video_decoder) {
        for (int i = 0; i < player.video_queue_limit; ++i) {
            player.video_queue[i] = av_frame_alloc();
            if (!player.video_queue[i]) return player_open_failed("video queue", AVERROR(ENOMEM));
        }
    }

    if (player.audio_decoder) {
        Uint16 audio_format = 0;
        if (!Mix_QuerySpec(&player.audio_rate, &audio_format, &player.audio_channels)) {
            if (!init_audio_backend()) return player_open_failed("audio output", 0);
            Mix_QuerySpec(&player.audio_rate, &audio_format, &player.audio_channels);
        }
        if (player.audio_rate > 0 && player.audio_channels > 0 && audio_format == AUDIO_F32LSB) {
            static const int period_frames[] = {256, 512, 1024, 2048};
            static const int latency_periods[] = {3, 5, 8};
            player.audio_capacity =
                period_frames[config.video.audio_period] * latency_periods[config.video.audio_latency] + 1;
            const int local_video_audio_capacity = player.audio_rate / 2 + 1;
            if (!player.live && !player.audio_only && player.audio_capacity < local_video_audio_capacity)
                player.audio_capacity = local_video_audio_capacity;
            const int live_audio_capacity =
                player.audio_rate * (live_audio_target_seconds() + LIVE_AUDIO_BUFFER_HEADROOM_SECONDS) + 1;
            if (player.live && player.audio_capacity < live_audio_capacity) player.audio_capacity = live_audio_capacity;
            player.audio_ring =
                calloc((size_t) player.audio_capacity * player.audio_channels, sizeof(*player.audio_ring));
            player.audio_convert =
                malloc((size_t) AUDIO_CONVERT_FRAMES * player.audio_channels * sizeof(*player.audio_convert));
            if (!player.live) {
                player.audio_speed_convert = malloc(
                    (size_t) (AUDIO_SPEED_BATCH_FRAMES + 2) * player.audio_channels
                    * sizeof(*player.audio_speed_convert)
                );
                if (player.audio_speed_convert) {
                    player.audio_speed_previous =
                        player.audio_speed_convert + (size_t) AUDIO_SPEED_BATCH_FRAMES * player.audio_channels;
                    player.audio_speed_filter = player.audio_speed_previous + player.audio_channels;
                    audio_speed_reset();
                }
            }
            if (!player.audio_ring || !player.audio_convert || (!player.live && !player.audio_speed_convert))
                return player_open_failed("audio buffers", AVERROR(ENOMEM));
            Mix_HaltMusic();
            Mix_HookMusic(audio_callback, &audio_hook_owner);
        }
    }

    video_state_entry saved = {0};
    double initial_position = options->start_position;
    if (transition_position > initial_position) initial_position = transition_position;
    if (initial_position <= 0.0 && !player.live && options->resume && audio_source_resumable(uri)
        && video_history_find(uri, &saved) && saved.position > 1.0
        && (saved.duration <= 0.0 || saved.position < saved.duration * 0.95))
        initial_position = saved.position;
    if (!player.live && initial_position > 0.0 && (player.duration <= 0.0 || initial_position < player.duration)) {
        player.position = initial_position;
        player.clock_origin = initial_position;
        perform_seek(initial_position);
    }
    free(saved.uri);
    free(saved.title);
    free(saved.thumbnail);
    free(saved.name);

    video_render_set_content(player.uri, player.live);
    video_render_set_audio(player.audio_only);
    if (!video_render_open()) return player_open_failed("video renderer", 0);
    if (!video_playback_ui_init(
            player.title, player.uri, player.container_uri[0] ? player.container_uri : player.uri, player.live,
            player.playlist, player.playlist_count, player.playlist_index, player.playlist_channels
        ))
        return player_open_failed("playback interface", 0);
    player.ui_ready = 1;
    if (player.audio_only) {
        if (!wasabi_audio_ui_init(&player.audio_information)) return player_open_failed("audio interface", 0);
        wasabi_audio_ui_update(player.position, player.duration, 0);
        display_composite_frame();
    }
    if (video_codec_unsupported) toast_message(lang.muxmedia.codec_unsupported, tst_wait_l);
    player.clock_ticks = SDL_GetTicks();
    player.last_history_position = player.position;
    player.history_save_deadline = SDL_GetTicks() + 30000;
    player.frame_blend = should_blend_frames();
    player.present_interval = player.frame_blend ? PRESENT_INTERVAL_BLEND_MS : preferred_present_interval();
    player.presented_timestamp = -1.0;
    player.blend_timestamp = -1.0;
    player.present_timer = lv_timer_create(present_tick, player.present_interval, NULL);
    if (player.live) player.demux_thread = SDL_CreateThread(demux_thread, "muxmedia-demux", NULL);
    player.decode_thread = SDL_CreateThread(decode_thread, "muxmedia-decode", NULL);
    if (!player.present_timer || !player.decode_thread || (player.live && !player.demux_thread))
        return player_open_failed("playback threads", 0);
    if (!audio_transition_handoff()) start_audio_transition();
    return 1;
}

static int player_open_live_failure(const char *uri, const char *title, const video_player_options *options) {
    memset(&player, 0, sizeof(player));
    player.speed_current = player.speed_start = player.speed_target = 1.0;
    player.clock_speed = 1.0;
    SDL_AtomicSet(&player.speed_q16, 65536);
    player.video_stream = -1;
    player.audio_stream = -1;
    player.live = 1;
    player.live_failed = 1;
    player.opened_live_quality = config.wasabi.live_quality;
    player.opened_live_buffer = config.wasabi.live_buffer;
    player.playlist = options->playlist;
    player.playlist_count = options->playlist_count;
    player.playlist_index = options->playlist_index;
    player.playlist_channels = options->playlist_channels;
    player.playlist_selection = options->playlist_selection;
    player.folder_playlist = options->folder_playlist;
    player.clock_ticks = SDL_GetTicks();
    snprintf(player.uri, sizeof(player.uri), "%s", uri);
    snprintf(player.title, sizeof(player.title), "%s", title && title[0] ? title : uri);
    snprintf(
        player.container_uri, sizeof(player.container_uri), "%s", options->container_uri ? options->container_uri : ""
    );

    player.lock = SDL_CreateMutex();
    player.condition = SDL_CreateCond();
    if (!player.lock || !player.condition) return 0;

    video_render_set_content(player.uri, 1);
    if (!video_render_open()) return 0;
    video_render_set_static(1);
    if (!video_playback_ui_init(
            player.title, player.uri, player.container_uri[0] ? player.container_uri : player.uri, 1, player.playlist,
            player.playlist_count, player.playlist_index, player.playlist_channels
        ))
        return 0;
    player.ui_ready = 1;
    player.present_interval = PRESENT_INTERVAL_NORMAL_MS;
    player.present_timer = lv_timer_create(present_tick, player.present_interval, NULL);
    if (!player.present_timer) return 0;

    video_loading_hide();
    display_composite_frame();
    toast_message(lang.muxmedia.live_tv_unavailable, tst_wait_l);
    return 1;
}

int video_player_run(const char *uri, const char *title, const video_player_options *options) {
    if (!uri || !uri[0] || !options) return video_player_failed;
    char previous_governor[64] = "";
    const int governor_changed =
        options->live ? live_performance_begin(previous_governor, sizeof(previous_governor)) : 0;
    wasabi_session_begin(uri);
    int result = video_player_stopped;
    const char *current_uri = uri;
    const char *current_title = title;
    for (;;) {
        display_set_composite_suppressed(0);
        display_set_idle_saver_suppressed_query(idle_saver_suppressed);
        if (options->live) {
            video_render_set_content(uri, 1);
            video_render_open();
            show_live_loading(title);
        }
        if (!player_open(current_uri, current_title, options)) {
            LOG_ERROR("muxmedia", "Unable to open '%s'", current_uri);
            player_cleanup();
            if (!options->live || !player_open_live_failure(uri, title, options)) {
                player_cleanup();
                result = video_player_failed;
                break;
            }
        }

        mux_input_options input_options = {
            .swap_axis = theme.misc.navigation_type == 1,
            .hold_disabled = 1,
            .press_handler =
                {
                    [mux_input_a] = handle_confirm,
                    [mux_input_b] = handle_back,
                    [mux_input_x] = handle_delete_bookmark,
                    [mux_input_start] = handle_start,
                    [mux_input_y] = handle_bookmark,
                    [mux_input_menu] = handle_menu_press,
                    [mux_input_select] = handle_clear_bookmark_name,
                    [mux_input_dpad_up] = handle_up,
                    [mux_input_dpad_down] = handle_down,
                    [mux_input_dpad_left] = handle_left,
                    [mux_input_dpad_right] = handle_right,
                    [mux_input_l1] = handle_page_up,
                    [mux_input_r1] = handle_page_down,
                },
            .release_handler = {[mux_input_menu] = handle_menu},
            .hold_handler =
                {
                    [mux_input_dpad_up] = handle_up_hold,
                    [mux_input_dpad_down] = handle_down_hold,
                    [mux_input_dpad_left] = handle_left,
                    [mux_input_dpad_right] = handle_right,
                    [mux_input_b] = handle_back,
                },
            .input_handler = handle_configured_hotkey,
        };
        init_input(&input_options, 1);
        mux_input_task(&input_options);

        const int complete = player.duration > 0.0 && player.position >= player.duration * 0.95;
        save_history(complete, 1);
        const int switched = player.playlist_switch_requested;
        const int content_switched = player.content_switch_requested;
        const int restart = player.restart_requested;
        const int subsong_switch = switched && !player.folder_switch_requested && subsongs.entries
                                   && player.playlist == subsongs.entries && subsongs.selected < subsongs.count;
        const int quitting = !restart && !subsong_switch && !switched && !content_switched;
        if (quitting) {
            const int ui_was_hidden = display_ui_is_hidden();
            display_set_ui_hidden(1);
            fade_out_screen_forced();
            display_blank_fb();
            display_set_composite_suppressed(1);
            display_set_fade_alpha(0);
            display_set_ui_hidden(ui_was_hidden);
        } else if (player.audio_only) {
            display_mirror_to_fb();
        } else {
            const int ui_was_hidden = display_ui_is_hidden();
            video_render_set_clean_capture(1);
            display_set_ui_hidden(1);
            display_mirror_to_fb();
            display_set_ui_hidden(ui_was_hidden);
            video_render_set_clean_capture(0);
        }
        display_set_composite_suppressed(1);
        player_cleanup();
        if (restart) continue;
        if (subsong_switch) {
            current_uri = subsongs.entries[subsongs.selected].uri;
            current_title = subsongs.entries[subsongs.selected].title;
            continue;
        }
        result = content_switched ? video_player_content_switch
                 : switched       ? video_player_playlist_switch
                                  : video_player_stopped;
        break;
    }
    wasabi_session_end();
    subsong_playlist_free();
    if (result != video_player_playlist_switch) shuffle_reset();
    live_performance_end(governor_changed, previous_governor);
    return result;
}
