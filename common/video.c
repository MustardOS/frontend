#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/version.h>
#include "video.h"
#include "display.h"
#include "options.h"

#define DECODE_MS        10   // Decoding timer period
#define FADE_STEP_MS     16   // How many ms between fade steps
#define FADE_STEPS       24   // Fade steps... duh
#define DIM_TARGET       160  // How much the background dims
#define DECODE_AHEAD_MS  200  // Stop reading once video is this far ahead of clock
#define DISPLAY_SLACK    5    // The frame when is "due" within this many ms
#define VFRAME_Q         12   // Decoded video frames buffered for ready display
#define AUDIO_RING_SEC   3    // Audio ring capacity
#define AUDIO_TMP_FRAMES 8192 // Conversion in stereo frames
#define VIDEO_BOX_PAD_H  60   // Padding on left/right
#define VIDEO_BOX_PAD_V  70   // Padding on top/bottom
#define VIDEO_CORNER_RAD 14   // Rounded corner radius on video

static char video_path[MAX_BUFFER_SIZE];
static lv_timer_t *preview_timer = NULL;
static lv_timer_t *decode_timer = NULL;
static lv_timer_t *fade_timer = NULL;

static SDL_Renderer *sdl_ren = NULL;
static SDL_Texture *video_tex = NULL;
static SDL_Rect video_dst;

static int dim_alpha = 0;
static int fade_step = 0;
static int has_frame = 0;

static int use_iyuv = 0;
static struct SwsContext *sws_ctx = NULL;
static uint8_t *sws_buf = NULL;

static SDL_Texture *round_target = NULL;
static SDL_Texture *corner_mask = NULL;
static SDL_BlendMode mask_blend = SDL_BLENDMODE_NONE;
static int use_round = 0;

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec = NULL;
static AVCodecContext *audio_dec = NULL;
static AVFrame *av_frame = NULL;
static AVPacket *av_pkt = NULL;
static AVRational video_tb;
static int video_si = -1;
static int audio_si = -1;

static AVFrame *vq[VFRAME_Q];
static int vq_head = 0;
static int vq_tail = 0;
static int vq_count = 0;

static struct SwrContext *swr = NULL;
static int mix_freq = 0;
static int mix_ch = 0;
static float *ring_buf = NULL;
static int ring_frames = 0;
static volatile int ring_r = 0;
static volatile int ring_w = 0;
static float audio_tmp[AUDIO_TMP_FRAMES * 2];

static uint32_t play_start_ms = 0;

static int64_t frame_pts_ms(const AVFrame *f) {
    int64_t pts = f->best_effort_timestamp;

    if (pts == AV_NOPTS_VALUE) pts = f->pts;
    if (pts == AV_NOPTS_VALUE) return 0;

    return (int64_t) ((double) pts * av_q2d(video_tb) * 1000.0);
}

static void vq_push(AVFrame *src) {
    if (vq_count >= VFRAME_Q) {
        av_frame_unref(src);
        return;
    }

    av_frame_move_ref(vq[vq_tail], src);
    vq_tail = (vq_tail + 1) % VFRAME_Q;

    vq_count++;
}

static AVFrame *vq_at(int i) {
    return vq[(vq_head + i) % VFRAME_Q];
}

static void vq_pop(void) {
    if (vq_count <= 0) return;

    av_frame_unref(vq[vq_head]);
    vq_head = (vq_head + 1) % VFRAME_Q;
    vq_count--;
}

static void vq_clear(void) {
    int n = vq_count;
    while (n-- > 0) vq_pop();
    vq_head = vq_tail = 0;
}

static int ring_avail(void) {
    return (ring_w - ring_r + ring_frames) % ring_frames;
}

static int ring_space(void) {
    return ring_frames - ring_avail() - 1;
}

static void ring_write(const float *src, int nframes) {
    if (nframes <= 0) return;

    int space = ring_space();
    if (nframes > space) nframes = space;

    int tail = ring_w;
    int first = (nframes < ring_frames - tail) ? nframes : ring_frames - tail;
    memcpy(ring_buf + tail * 2, src, (size_t) first * 2 * sizeof(float));
    memcpy(ring_buf, src + first * 2, (size_t) (nframes - first) * 2 * sizeof(float));

    ring_w = (ring_w + nframes) % ring_frames;
}

static void audio_hook_cb(void *udata, Uint8 *stream, int len) {
    (void) udata;
    int nframes = len / (mix_ch * (int) sizeof(float));
    float *out = (float *) stream;

    int avail = ring_avail();
    int to_read = (avail < nframes) ? avail : nframes;

    if (to_read > 0) {
        int head = ring_r;
        int first = (to_read < ring_frames - head) ? to_read : ring_frames - head;

        memcpy(out, ring_buf + head * 2, (size_t) first * 2 * sizeof(float));
        memcpy(out + first * 2, ring_buf, (size_t) (to_read - first) * 2 * sizeof(float));

        ring_r = (ring_r + to_read) % ring_frames;
    }

    if (to_read < nframes)SDL_memset(out + to_read * mix_ch, 0, (size_t) (nframes - to_read) * mix_ch * sizeof(float));
}

static SDL_Texture *make_corner_mask(int w, int h) {
    int radius = VIDEO_CORNER_RAD;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    if (radius < 1) return NULL;

    uint32_t * buf = (uint32_t *) malloc((size_t) w * h * 4);
    if (!buf) return NULL;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float cov = 1.0f;
            int cx = -1;
            int cy = -1;

            if (x < radius && y < radius) {
                cx = radius;
                cy = radius;
            } else if (x >= w - radius && y < radius) {
                cx = w - radius - 1;
                cy = radius;
            } else if (x < radius && y >= h - radius) {
                cx = radius;
                cy = h - radius - 1;
            } else if (x >= w - radius && y >= h - radius) {
                cx = w - radius - 1;
                cy = h - radius - 1;
            }

            if (cx >= 0) {
                float dx = (float) (x - cx);
                float dy = (float) (y - cy);
                cov = (float) radius + 0.5f - sqrtf(dx * dx + dy * dy);
                if (cov < 0.0f) cov = 0.0f;
                if (cov > 1.0f) cov = 1.0f;
            }

            uint8_t a = (uint8_t) (cov * 255.0f + 0.5f);
            buf[y * w + x] = ((uint32_t) a << 24) | 0x00FFFFFF;
        }
    }

    SDL_Texture *tex = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (tex) SDL_UpdateTexture(tex, NULL, buf, w * 4);

    free(buf);
    return tex;
}

static void sdl_overlay_cb(SDL_Renderer *r) {
    if (dim_alpha <= 0) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t) dim_alpha);
    SDL_RenderFillRect(r, NULL);

    if (!has_frame || !video_tex) return;

    if (use_round && round_target && corner_mask) {
        SDL_SetRenderTarget(r, round_target);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderClear(r);
        SDL_SetTextureBlendMode(video_tex, SDL_BLENDMODE_NONE);
        SDL_RenderCopy(r, video_tex, NULL, NULL);
        SDL_RenderCopy(r, corner_mask, NULL, NULL);
        SDL_SetRenderTarget(r, NULL);
        SDL_RenderCopy(r, round_target, NULL, &video_dst);
    } else {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderFillRect(r, &video_dst);
        SDL_RenderCopy(r, video_tex, NULL, &video_dst);
    }
}

static void fade_cb(lv_timer_t *t) {
    fade_step++;
    dim_alpha = DIM_TARGET * fade_step / FADE_STEPS;

    if (fade_step >= FADE_STEPS) {
        dim_alpha = DIM_TARGET;
        lv_timer_del(t);
        fade_timer = NULL;
    }

    display_composite_frame();
}

static int ensure_texture(AVFrame *f) {
    if (video_tex) return 1;
    if (!sdl_ren) return 0;

    int w = f->width;
    int h = f->height;

    enum AVPixelFormat fmt = (enum AVPixelFormat) f->format;
    int want_round = SDL_RenderTargetSupported(sdl_ren);

    if (!want_round && (fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUVJ420P)) {
        video_tex = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (video_tex) use_iyuv = 1;
    }

    if (!video_tex) {
        use_iyuv = 0;
        sws_ctx = sws_getContext(w, h, fmt, w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);
        sws_buf = (uint8_t *) malloc((size_t) w * h * 4);
        if (sws_ctx && sws_buf) video_tex = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    }

    if (!video_tex) return 0;

    SDL_SetTextureScaleMode(video_tex, SDL_ScaleModeLinear);

    int dw = lv_disp_get_hor_res(NULL);
    int dh = lv_disp_get_ver_res(NULL);

    int bw = dw - 2 * VIDEO_BOX_PAD_H;
    int bh = dh - 2 * VIDEO_BOX_PAD_V;
    int vw = bw;
    int vh = h * bw / w;

    if (vh > bh) {
        vh = bh;
        vw = w * bh / h;
    }

    video_dst = (SDL_Rect) {VIDEO_BOX_PAD_H + (bw - vw) / 2, VIDEO_BOX_PAD_V + (bh - vh) / 2, vw, vh};

    use_round = 0;
    if (want_round) {
        round_target = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, vw, vh);
        corner_mask = make_corner_mask(vw, vh);

        if (round_target && corner_mask) {
            SDL_SetTextureBlendMode(round_target, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(round_target, SDL_ScaleModeLinear);

            mask_blend = SDL_ComposeCustomBlendMode(
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD);

            SDL_SetTextureBlendMode(corner_mask, mask_blend);
            use_round = 1;
        } else {
            if (round_target) {
                SDL_DestroyTexture(round_target);
                round_target = NULL;
            }

            if (corner_mask) {
                SDL_DestroyTexture(corner_mask);
                corner_mask = NULL;
            }
        }
    }

    return 1;
}

static void video_upload(AVFrame *f) {
    if (!ensure_texture(f)) return;

    int ok = 0;

    if (use_iyuv) {
        ok = (SDL_UpdateYUVTexture(video_tex, NULL, f->data[0], f->linesize[0], f->data[1], f->linesize[1], f->data[2], f->linesize[2]) == 0);
    } else if (sws_ctx && sws_buf) {
        uint8_t *dst[4] = {sws_buf, NULL, NULL, NULL};
        int dls[4] = {f->width * 4, 0, 0, 0};

        ok = (sws_scale(sws_ctx, (const uint8_t *const *) f->data, f->linesize, 0, f->height, dst, dls) > 0);
        if (ok) SDL_UpdateTexture(video_tex, NULL, sws_buf, f->width * 4);
    }

    if (ok) has_frame = 1;
}

static void decode_audio_pkt(AVPacket *pkt) {
    if (!ring_buf || avcodec_send_packet(audio_dec, pkt) < 0) return;

    while (avcodec_receive_frame(audio_dec, av_frame) >= 0) {
        if (!swr) {
#if LIBAVUTIL_VERSION_MAJOR >= 58
            AVChannelLayout stereo_layout = AV_CHANNEL_LAYOUT_STEREO;
            swr_alloc_set_opts2(&swr, &stereo_layout, AV_SAMPLE_FMT_FLT, mix_freq,
                                &av_frame->ch_layout, (enum AVSampleFormat) av_frame->format,
                                av_frame->sample_rate, 0, NULL);
#else
            int64_t in_layout = av_frame->channel_layout
                                    ? (int64_t) av_frame->channel_layout
                                    : av_get_default_channel_layout(av_frame->channels);
            swr = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, mix_freq,
                                     in_layout, (enum AVSampleFormat) av_frame->format,
                                     av_frame->sample_rate, 0, NULL);
#endif
            if (swr) swr_init(swr);
        }

        if (swr) {
            uint8_t *outp = (uint8_t *) audio_tmp;
            int n = swr_convert(swr, &outp, AUDIO_TMP_FRAMES, (const uint8_t **) av_frame->data, av_frame->nb_samples);
            if (n > 0) {
                SDL_LockAudio();
                ring_write(audio_tmp, n);
                SDL_UnlockAudio();
            }
        }

        av_frame_unref(av_frame);
    }
}

static void decode_video_pkt(AVPacket *pkt) {
    if (avcodec_send_packet(video_dec, pkt) < 0) return;

    while (avcodec_receive_frame(video_dec, av_frame) >= 0) {
        vq_push(av_frame);
    }
}

static void restart_stream(void) {
    av_seek_frame(fmt_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (video_dec) avcodec_flush_buffers(video_dec);
    if (audio_dec) avcodec_flush_buffers(audio_dec);

    if (swr) {
        swr_free(&swr);
        swr = NULL;
    }

    vq_clear();
    SDL_LockAudio();

    ring_r = ring_w = 0;
    SDL_UnlockAudio();

    play_start_ms = SDL_GetTicks();
}

static void decode_cb(lv_timer_t *t) {
    (void) t;
    if (!fmt_ctx) return;

    int64_t now = (int64_t) (SDL_GetTicks() - play_start_ms);
    int shown = 0;

    while (vq_count > 0 && frame_pts_ms(vq_at(0)) <= now + DISPLAY_SLACK) {
        if (vq_count >= 2 && frame_pts_ms(vq_at(1)) <= now + DISPLAY_SLACK) {
            vq_pop();
            continue;
        }

        video_upload(vq_at(0));
        vq_pop();
        shown = 1;

        break;
    }

    int budget = 24;
    while (budget-- > 0 && vq_count <= VFRAME_Q - 2) {
        if (vq_count > 0 && frame_pts_ms(vq_at(vq_count - 1)) > now + DECODE_AHEAD_MS) break;

        if (av_read_frame(fmt_ctx, av_pkt) < 0) {
            restart_stream();
            av_packet_unref(av_pkt);
            break;
        }

        if (av_pkt->stream_index == video_si && video_dec) {
            decode_video_pkt(av_pkt);
        } else if (av_pkt->stream_index == audio_si && audio_dec) {
            decode_audio_pkt(av_pkt);
        }

        av_packet_unref(av_pkt);
    }

    if (shown && has_frame) display_composite_frame();
}

static void cleanup(void) {
    display_clear_video_overlay();

    dim_alpha = 0;
    has_frame = 0;
    fade_step = 0;

    if (fade_timer) {
        lv_timer_del(fade_timer);
        fade_timer = NULL;
    }

    if (decode_timer) {
        lv_timer_del(decode_timer);
        decode_timer = NULL;
    }

    Mix_HookMusic(NULL, NULL);

    if (swr) swr_free(&swr);

    if (ring_buf) {
        SDL_LockAudio();
        ring_r = ring_w = 0;
        SDL_UnlockAudio();
        free(ring_buf);
        ring_buf = NULL;
    }

    ring_frames = 0;
    mix_freq = mix_ch = 0;

    for (int i = 0; i < VFRAME_Q; i++) {
        if (vq[i]) {
            av_frame_free(&vq[i]);
            vq[i] = NULL;
        }
    }

    vq_head = vq_tail = vq_count = 0;

    if (audio_dec) avcodec_free_context(&audio_dec);

    if (av_frame) {
        av_frame_free(&av_frame);
        av_frame = NULL;
    }

    if (av_pkt) {
        av_packet_free(&av_pkt);
        av_pkt = NULL;
    }

    if (video_dec) avcodec_free_context(&video_dec);

    if (fmt_ctx) avformat_close_input(&fmt_ctx);

    if (video_tex) {
        SDL_DestroyTexture(video_tex);
        video_tex = NULL;
    }

    if (round_target) {
        SDL_DestroyTexture(round_target);
        round_target = NULL;
    }

    if (corner_mask) {
        SDL_DestroyTexture(corner_mask);
        corner_mask = NULL;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }

    if (sws_buf) {
        free(sws_buf);
        sws_buf = NULL;
    }

    video_si = audio_si = -1;
    use_iyuv = use_round = 0;

    sdl_ren = NULL;
}

static void preview_open(void) {
    if (avformat_open_input(&fmt_ctx, video_path, NULL, NULL) < 0) return;

    fmt_ctx->probesize = 1000000;
    fmt_ctx->max_analyze_duration = 2000000;

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) goto fail;

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        AVCodecParameters *par = fmt_ctx->streams[i]->codecpar;

        if (par->codec_type == AVMEDIA_TYPE_VIDEO && video_si < 0) {
            const AVCodec *dec = avcodec_find_decoder(par->codec_id);
            if (!dec) continue;

            video_dec = avcodec_alloc_context3(dec);
            if (!video_dec) continue;

            avcodec_parameters_to_context(video_dec, par);

            video_dec->thread_count = 0;
            video_dec->thread_type = FF_THREAD_SLICE;

            if (avcodec_open2(video_dec, dec, NULL) < 0) {
                avcodec_free_context(&video_dec);
                continue;
            }

            video_si = (int) i;
            video_tb = fmt_ctx->streams[i]->time_base;
        }

        if (par->codec_type == AVMEDIA_TYPE_AUDIO && audio_si < 0) {
            const AVCodec *dec = avcodec_find_decoder(par->codec_id);
            if (!dec) continue;

            audio_dec = avcodec_alloc_context3(dec);
            if (!audio_dec) continue;

            avcodec_parameters_to_context(audio_dec, par);
            if (avcodec_open2(audio_dec, dec, NULL) < 0) {
                avcodec_free_context(&audio_dec);
                continue;
            }

            audio_si = (int) i;
        }
    }

    if (video_si < 0) goto fail;

    sdl_ren = display_get_renderer();
    if (!sdl_ren) goto fail;

    av_frame = av_frame_alloc();
    av_pkt = av_packet_alloc();

    if (!av_frame || !av_pkt) goto fail;
    for (int i = 0; i < VFRAME_Q; i++) {
        vq[i] = av_frame_alloc();
        if (!vq[i]) goto fail;
    }

    vq_head = vq_tail = vq_count = 0;

    if (audio_si >= 0 && audio_dec) {
        Uint16 mix_fmt;
        Mix_QuerySpec(&mix_freq, &mix_fmt, &mix_ch);

        if (mix_freq > 0 && mix_ch > 0 && mix_fmt == AUDIO_F32LSB) {
            ring_frames = mix_freq * AUDIO_RING_SEC;
            ring_buf = (float *) calloc((size_t) ring_frames * 2, sizeof(float));

            if (ring_buf) {
                ring_r = ring_w = 0;
                Mix_HaltMusic();
                Mix_HookMusic(audio_hook_cb, NULL);
            }
        }
    }

    play_start_ms = SDL_GetTicks();
    display_set_video_overlay(sdl_overlay_cb);

    decode_timer = lv_timer_create(decode_cb, DECODE_MS, NULL);
    fade_timer = lv_timer_create(fade_cb, FADE_STEP_MS, NULL);

    return;

    fail:
    cleanup();
}

static void preview_timer_cb(lv_timer_t *t) {
    (void) t;

    lv_timer_pause(preview_timer);
    if (video_path[0]) preview_open();
}

void video_preview_arm(const char *path, int delay_ms, lv_obj_t *container, lv_obj_t *box_img) {
    (void) container;
    (void) box_img;

    cleanup();

    snprintf(video_path, sizeof(video_path), "%s", path);
    if (!preview_timer) {
        preview_timer = lv_timer_create(preview_timer_cb, (uint32_t) delay_ms, NULL);
        lv_timer_pause(preview_timer);
    } else {
        lv_timer_set_period(preview_timer, (uint32_t) delay_ms);
    }

    lv_timer_reset(preview_timer);
    lv_timer_resume(preview_timer);
}

int video_preview_active(void) {
    return decode_timer != NULL;
}

void video_preview_cancel(void) {
    if (preview_timer) lv_timer_pause(preview_timer);

    cleanup();
    video_path[0] = '\0';
}

void video_preview_destroy(void) {
    if (preview_timer) {
        lv_timer_del(preview_timer);
        preview_timer = NULL;
    }

    cleanup();
    video_path[0] = '\0';
}
