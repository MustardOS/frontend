#include "audio_transition.h"
#include "audio_source.h"

#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char uri[4096];
    size_t playlist_index;
    int rate;
    int channels;
    int capacity;
    int frames;
    int read;
    float *samples;
    SDL_Thread *thread;
    SDL_atomic_t stop;
    SDL_atomic_t ready;
    SDL_atomic_t handoff;
} transition_state;

static transition_state transition;

static int decode_transition(void *unused __attribute__((unused))) {
    AVFormatContext *format = NULL;
    AVCodecContext *decoder = NULL;
    SwrContext *resample = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    int stream = -1;

    AVDictionary *options = NULL;
    const int opened = audio_source_open(&format, transition.uri, transition.rate, &options);
    av_dict_free(&options);
    if (opened < 0 || avformat_find_stream_info(format, NULL) < 0) goto done;
    stream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (stream < 0) goto done;
    const AVCodec *codec = avcodec_find_decoder(format->streams[stream]->codecpar->codec_id);
    if (!codec) goto done;
    decoder = avcodec_alloc_context3(codec);
    if (!decoder || avcodec_parameters_to_context(decoder, format->streams[stream]->codecpar) < 0
        || avcodec_open2(decoder, codec, NULL) < 0)
        goto done;
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) goto done;

    while (!SDL_AtomicGet(&transition.stop) && transition.frames < transition.capacity
           && av_read_frame(format, packet) >= 0) {
        if (packet->stream_index != stream || avcodec_send_packet(decoder, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);
        while (!SDL_AtomicGet(&transition.stop) && transition.frames < transition.capacity
               && avcodec_receive_frame(decoder, frame) >= 0) {
            if (!resample) {
                AVChannelLayout output_layout;
                av_channel_layout_default(&output_layout, transition.channels);
                if (swr_alloc_set_opts2(
                        &resample, &output_layout, AV_SAMPLE_FMT_FLT, transition.rate, &frame->ch_layout,
                        (enum AVSampleFormat) frame->format, frame->sample_rate, 0, NULL
                    ) < 0
                    || !resample || swr_init(resample) < 0) {
                    av_channel_layout_uninit(&output_layout);
                    goto done;
                }
                av_channel_layout_uninit(&output_layout);
            }
            const int available = transition.capacity - transition.frames;
            uint8_t *output = (uint8_t *) (transition.samples + (size_t) transition.frames * transition.channels);
            const int converted =
                swr_convert(resample, &output, available, (const uint8_t **) frame->data, frame->nb_samples);
            if (converted > 0) transition.frames += converted;
            av_frame_unref(frame);
        }
    }

    if (!SDL_AtomicGet(&transition.stop) && decoder && transition.frames < transition.capacity
        && avcodec_send_packet(decoder, NULL) >= 0) {
        while (transition.frames < transition.capacity && avcodec_receive_frame(decoder, frame) >= 0) {
            const int available = transition.capacity - transition.frames;
            uint8_t *output = (uint8_t *) (transition.samples + (size_t) transition.frames * transition.channels);
            const int converted =
                resample ? swr_convert(resample, &output, available, (const uint8_t **) frame->data, frame->nb_samples)
                         : 0;
            if (converted > 0) transition.frames += converted;
            av_frame_unref(frame);
        }
    }

done:
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&resample);
    avcodec_free_context(&decoder);
    avformat_close_input(&format);
    if (!SDL_AtomicGet(&transition.stop) && transition.frames > 0) SDL_AtomicSet(&transition.ready, 1);
    return 0;
}

void audio_transition_cancel(void) {
    SDL_AtomicSet(&transition.stop, 1);
    if (transition.thread) SDL_WaitThread(transition.thread, NULL);
    transition.thread = NULL;
    SDL_LockAudio();
    free(transition.samples);
    memset(&transition, 0, sizeof(transition));
    SDL_UnlockAudio();
}

int audio_transition_start(
    const char *uri, const size_t playlist_index, const int rate, const int channels, int seconds
) {
    audio_transition_cancel();
    if (!uri || !uri[0] || rate <= 0 || channels <= 0) return 0;
    if (seconds < 3) seconds = 3;
    if (seconds > 15) seconds = 15;
    transition.capacity = rate * seconds;
    transition.samples = malloc((size_t) transition.capacity * channels * sizeof(*transition.samples));
    if (!transition.samples) {
        memset(&transition, 0, sizeof(transition));
        return 0;
    }
    snprintf(transition.uri, sizeof(transition.uri), "%s", uri);
    transition.playlist_index = playlist_index;
    transition.rate = rate;
    transition.channels = channels;
    transition.thread = SDL_CreateThread(decode_transition, "muxmedia-next", NULL);
    if (!transition.thread) {
        free(transition.samples);
        memset(&transition, 0, sizeof(transition));
        return 0;
    }
    return 1;
}

int audio_transition_ready(void) {
    return SDL_AtomicGet(&transition.ready) && transition.samples && transition.read < transition.frames;
}

int audio_transition_uri_matches(const char *uri) {
    return uri && transition.uri[0] && strcmp(uri, transition.uri) == 0;
}

size_t audio_transition_playlist_index(void) {
    return transition.playlist_index;
}

double audio_transition_consumed(void) {
    return transition.rate > 0 ? (double) transition.read / transition.rate : 0.0;
}

int audio_transition_handoff(void) {
    return SDL_AtomicGet(&transition.handoff);
}

void audio_transition_mark_handoff(void) {
    if (audio_transition_ready()) SDL_AtomicSet(&transition.handoff, 1);
}

void audio_transition_release(void) {
    audio_transition_cancel();
}

void audio_transition_mix(
    float *output, const int current_frames, const int requested_frames, const int eof, const double remaining,
    const int crossfade_seconds
) {
    if (!output || requested_frames <= 0 || !audio_transition_ready()) return;
    const int channels = transition.channels;
    int mixed = 0;
    if (crossfade_seconds > 0 && remaining >= 0.0 && remaining <= crossfade_seconds) {
        int count = current_frames;
        if (count > transition.frames - transition.read) count = transition.frames - transition.read;
        const int fade_frames = transition.rate * crossfade_seconds;
        const double fade_progress = (double) crossfade_seconds - remaining;
        for (int frame = 0; frame < count; frame++) {
            float gain = fade_frames > 0 ? (float) ((fade_progress * transition.rate + frame) / fade_frames) : 1.0f;
            if (gain < 0.0f) gain = 0.0f;
            if (gain > 1.0f) gain = 1.0f;
            for (int channel = 0; channel < channels; channel++) {
                const size_t destination = (size_t) frame * channels + channel;
                const size_t source = (size_t) transition.read * channels + channel;
                output[destination] = output[destination] * (1.0f - gain) + transition.samples[source] * gain;
            }
            transition.read++;
        }
        mixed = count;
    }
    if (eof && current_frames < requested_frames) {
        int count = requested_frames - current_frames;
        if (count > transition.frames - transition.read) count = transition.frames - transition.read;
        if (count > 0) {
            memcpy(
                output + (size_t) current_frames * channels, transition.samples + (size_t) transition.read * channels,
                (size_t) count * channels * sizeof(*output)
            );
            transition.read += count;
            mixed += count;
        }
    }
    if (eof && mixed > 0) SDL_AtomicSet(&transition.handoff, 1);
}

int audio_transition_fill_handoff(void *stream, const int length, const int volume) {
    if (!stream || length <= 0 || !audio_transition_handoff() || transition.channels <= 0) return 0;
    const int bytes_per_frame = transition.channels * (int) sizeof(float);
    const int requested = length / bytes_per_frame;
    int count = transition.frames - transition.read;
    if (count > requested) count = requested;
    if (count > 0) {
        memcpy(
            stream, transition.samples + (size_t) transition.read * transition.channels,
            (size_t) count * bytes_per_frame
        );
        transition.read += count;
        if (volume != 100) {
            float *samples = stream;
            const float gain = (float) volume / 100.0f;
            for (int sample = 0; sample < count * transition.channels; sample++) {
                const float boosted = samples[sample] * gain;
                samples[sample] = boosted > 1.0f ? 1.0f : boosted < -1.0f ? -1.0f : boosted;
            }
        }
    }
    if (count < requested)
        memset(
            (unsigned char *) stream + (size_t) count * bytes_per_frame, 0,
            (size_t) (requested - count) * bytes_per_frame
        );
    return 1;
}
