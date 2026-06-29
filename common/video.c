#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
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
#include "config.h"

#define PRESENT_MS               16   // Presentation timer period (main thread)
#define FADE_STEP_MS             16   // How many ms between fade steps
#define FADE_STEPS               24   // Fade steps... duh
#define DIM_TARGET               160  // How much the background dims
#define DISPLAY_SLACK            5    // The frame when is "due" within this many ms
#define LOOP_BACK_MS             500  // Backward PTS jump larger than this means the stream looped
#define VFRAME_Q                 12   // Decoded video frames buffered for ready display
#define AUDIO_RING_SEC           3    // Audio ring capacity
#define AUDIO_TMP_FRAMES         8192 // Conversion in stereo frames
#define PREVIEW_VIDEO_BOX_PAD_H  60   // Padding on left/right
#define PREVIEW_VIDEO_BOX_PAD_V  70   // Padding on top/bottom
#define PREVIEW_VIDEO_CORNER_RAD 14   // Rounded corner radius on video
#define WALLPAPER_STALL_MS       50   // Main-loop stalls above this are treated as pauses
#define WALLPAPER_DEF_FRAME_MS   33   // Fallback frame pacing for wallpaper videos
#define WALLPAPER_MIN_FRAME_MS   8    // Upper frame rate clamp for wallpaper pacing
#define WALLPAPER_MAX_FRAME_MS   100  // Lower frame rate clamp for wallpaper pacing

static char video_path[MAX_BUFFER_SIZE];
static char saved_wall_path[MAX_BUFFER_SIZE];
static char preview_pending_path[MAX_BUFFER_SIZE];

static lv_timer_t *preview_timer = NULL;
static lv_timer_t *present_timer = NULL;
static lv_timer_t *fade_timer = NULL;

static SDL_Thread *decode_thread = NULL;
static SDL_mutex *vq_lock = NULL;
static SDL_cond *vq_cond = NULL;
static volatile int decode_run = 0;
static int64_t last_pts = INT64_MIN;

static SDL_Renderer *sdl_ren = NULL;
static SDL_Texture *video_tex = NULL;
static SDL_Rect video_dst;

static int dim_alpha = 0;
static int fade_step = 0;
static int has_frame = 0;
static int is_wallpaper = 0;
static uint32_t stall_last_ticks = 0;
static uint32_t wallpaper_frame_ms = WALLPAPER_DEF_FRAME_MS;
static uint32_t wallpaper_next_ticks = 0;

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
static AVFrame *present_frame = NULL;
static AVPacket *av_pkt = NULL;
static AVRational video_tb;
static int video_si = -1;
static int audio_si = -1;

static AVFrame *vq[VFRAME_Q];
static int vq_head = 0;
static int vq_tail = 0;
static int vq_count = 0;

static SwrContext *swr = NULL;
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

static uint32_t stream_frame_ms(const AVStream *stream) {
    AVRational rate = {0, 1};

    if (stream) {
        if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
            rate = stream->avg_frame_rate;
        } else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
            rate = stream->r_frame_rate;
        }
    }

    if (rate.num > 0 && rate.den > 0) {
        const double fps = av_q2d(rate);

        if (fps > 0.0 && fps < 1000.0) {
            uint32_t frame_ms = (uint32_t) (1000.0 / fps + 0.5);

            if (frame_ms < WALLPAPER_MIN_FRAME_MS) frame_ms = WALLPAPER_MIN_FRAME_MS;
            if (frame_ms > WALLPAPER_MAX_FRAME_MS) frame_ms = WALLPAPER_MAX_FRAME_MS;

            return frame_ms;
        }
    }

    return WALLPAPER_DEF_FRAME_MS;
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

static AVFrame *vq_at(const int i) {
    return vq[(vq_head + i) % VFRAME_Q];
}

static void vq_pop(void) {
    if (vq_count <= 0) return;

    av_frame_unref(vq[vq_head]);
    vq_head = (vq_head + 1) % VFRAME_Q;
    vq_count--;
}

static int vq_pop_into(AVFrame *dst) {
    if (vq_count <= 0 || !dst) return 0;

    av_frame_unref(dst);
    av_frame_move_ref(dst, vq[vq_head]);
    vq_head = (vq_head + 1) % VFRAME_Q;
    vq_count--;

    return 1;
}

static void vq_clear_locked(void) {
    for (int i = 0; i < VFRAME_Q; i++) {
        if (vq[i]) av_frame_unref(vq[i]);
    }

    vq_head = 0;
    vq_tail = 0;
    vq_count = 0;
}

static void reset_playback_timing(const uint32_t ticks) {
    play_start_ms = ticks;
    last_pts = INT64_MIN;
    stall_last_ticks = is_wallpaper ? ticks : 0;
    wallpaper_next_ticks = is_wallpaper ? ticks : 0;
}

static int ring_avail(void) {
    return (ring_w - ring_r + ring_frames) % ring_frames;
}

static int ring_space(void) {
    return ring_frames - ring_avail() - 1;
}

static void ring_write(const float *src, int nframes) {
    if (nframes <= 0) return;

    const int space = ring_space();
    if (nframes > space) nframes = space;

    const int tail = ring_w;
    const int first = nframes < ring_frames - tail ? nframes : ring_frames - tail;
    memcpy(ring_buf + tail * 2, src, (size_t) first * 2 * sizeof(float));
    memcpy(ring_buf, src + first * 2, (size_t) (nframes - first) * 2 * sizeof(float));

    ring_w = (ring_w + nframes) % ring_frames;
}

static void audio_hook_cb(void *udata __attribute__((unused)), Uint8 *stream, const int len) {
    const int nframes = len / (mix_ch * (int) sizeof(float));
    float *out = (float *) stream;

    const int avail = ring_avail();
    const int to_read = avail < nframes ? avail : nframes;

    if (to_read > 0) {
        const int head = ring_r;
        const int first = to_read < ring_frames - head ? to_read : ring_frames - head;

        memcpy(out, ring_buf + head * 2, (size_t) first * 2 * sizeof(float));
        memcpy(out + first * 2, ring_buf, (size_t) (to_read - first) * 2 * sizeof(float));

        ring_r = (ring_r + to_read) % ring_frames;
    }

    if (to_read < nframes) SDL_memset(out + to_read * mix_ch, 0, (size_t) (nframes - to_read) * mix_ch * sizeof(float));
}

static SDL_Texture *make_corner_mask(const int w, const int h) {
    int radius = PREVIEW_VIDEO_CORNER_RAD;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    if (radius < 1) return NULL;

    uint32_t *buf = malloc((size_t) w * h * 4);
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
                const float dx = (float) (x - cx);
                const float dy = (float) (y - cy);
                cov = (float) radius + 0.5f - sqrtf(dx * dx + dy * dy);
                if (cov < 0.0f) cov = 0.0f;
                if (cov > 1.0f) cov = 1.0f;
            }

            const uint8_t a = (uint8_t) (cov * 255.0f + 0.5f);
            buf[y * w + x] = (uint32_t) a << 24 | 0x00FFFFFF;
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

static void wallpaper_cb(SDL_Renderer *r) {
    if (!has_frame || !video_tex) return;
    SDL_RenderCopy(r, video_tex, NULL, &video_dst);
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

static int ensure_texture(const AVFrame *f) {
    if (video_tex) return 1;
    if (!sdl_ren) return 0;

    const int w = f->width;
    const int h = f->height;

    const enum AVPixelFormat fmt = (enum AVPixelFormat) f->format;

    const int dw = lv_disp_get_hor_res(NULL);
    const int dh = lv_disp_get_ver_res(NULL);

    if (is_wallpaper) {
        int tex_w = w, tex_h = h;

        switch (config.visual.background_scale) {
            case 1:
                tex_w = dw;
                tex_h = h * dw / w;
                if (tex_h > dh) {
                    tex_h = dh;
                    tex_w = w * dh / h;
                }
                break;
            case 2:
                tex_w = dw;
                tex_h = dh;
                break;
            default:
                break;
        }

        if (tex_w == w && tex_h == h && (fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUVJ420P)) {
            video_tex = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);
            if (video_tex) use_iyuv = 1;
        }

        if (!video_tex) {
            use_iyuv = 0;
            sws_ctx = sws_getContext(w, h, fmt, tex_w, tex_h, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);
            sws_buf = malloc((size_t) tex_w * tex_h * 4);
            if (sws_ctx && sws_buf)
                video_tex =
                    SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tex_w, tex_h);
        }

        if (!video_tex) return 0;
        SDL_SetTextureScaleMode(video_tex, SDL_ScaleModeLinear);

        video_dst = config.visual.background_scale >= 2 ? (SDL_Rect) {0, 0, tex_w, tex_h}
                                                        : (SDL_Rect) {(dw - tex_w) / 2, (dh - tex_h) / 2, tex_w, tex_h};

    } else {
        const int want_round = SDL_RenderTargetSupported(sdl_ren);

        if (!want_round && (fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUVJ420P)) {
            video_tex = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);
            if (video_tex) use_iyuv = 1;
        }

        if (!video_tex) {
            use_iyuv = 0;
            sws_ctx = sws_getContext(w, h, fmt, w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);
            sws_buf = malloc((size_t) w * h * 4);
            if (sws_ctx && sws_buf)
                video_tex = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        }

        if (!video_tex) return 0;
        SDL_SetTextureScaleMode(video_tex, SDL_ScaleModeLinear);

        const int bw = dw - 2 * PREVIEW_VIDEO_BOX_PAD_H;
        const int bh = dh - 2 * PREVIEW_VIDEO_BOX_PAD_V;
        int vw = bw, vh = h * bw / w;

        if (vh > bh) {
            vh = bh;
            vw = w * bh / h;
        }

        video_dst =
            (SDL_Rect) {PREVIEW_VIDEO_BOX_PAD_H + (bw - vw) / 2, PREVIEW_VIDEO_BOX_PAD_V + (bh - vh) / 2, vw, vh};

        use_round = 0;
        if (want_round) {
            round_target = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, vw, vh);
            corner_mask = make_corner_mask(vw, vh);

            if (round_target && corner_mask) {
                SDL_SetTextureBlendMode(round_target, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(round_target, SDL_ScaleModeLinear);
                mask_blend = SDL_ComposeCustomBlendMode(
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO,
                    SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD
                );
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
    }

    return 1;
}

static void video_upload(const AVFrame *f) {
    if (!ensure_texture(f)) return;

    int ok = 0;

    if (use_iyuv) {
        ok = SDL_UpdateYUVTexture(
                 video_tex, NULL, f->data[0], f->linesize[0], f->data[1], f->linesize[1], f->data[2], f->linesize[2]
             )
             == 0;
    } else if (sws_ctx && sws_buf) {
        int tw = 0;
        SDL_QueryTexture(video_tex, NULL, NULL, &tw, NULL);
        if (tw <= 0) tw = f->width;

        uint8_t *dst[4] = {sws_buf, NULL, NULL, NULL};
        const int dls[4] = {tw * 4, 0, 0, 0};

        ok = sws_scale(sws_ctx, (const uint8_t **) f->data, f->linesize, 0, f->height, dst, dls) > 0;
        if (ok) SDL_UpdateTexture(video_tex, NULL, sws_buf, tw * 4);
    }

    if (ok) has_frame = 1;
}

static void decode_audio_pkt(const AVPacket *pkt) {
    if (!ring_buf || avcodec_send_packet(audio_dec, pkt) < 0) return;

    while (avcodec_receive_frame(audio_dec, av_frame) >= 0) {
        if (!swr) {
#if LIBAVUTIL_VERSION_MAJOR >= 58
            AVChannelLayout stereo_layout = AV_CHANNEL_LAYOUT_STEREO;
            swr_alloc_set_opts2(
                &swr, &stereo_layout, AV_SAMPLE_FMT_FLT, mix_freq, &av_frame->ch_layout,
                (enum AVSampleFormat) av_frame->format, av_frame->sample_rate, 0, NULL
            );
#else
            const int64_t in_layout = av_frame->channel_layout ? (int64_t) av_frame->channel_layout
                                                               : av_get_default_channel_layout(av_frame->channels);
            swr = swr_alloc_set_opts(
                NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, mix_freq, in_layout,
                (enum AVSampleFormat) av_frame->format, av_frame->sample_rate, 0, NULL
            );
#endif
            if (swr) swr_init(swr);
        }

        if (swr) {
            uint8_t *outp = (uint8_t *) audio_tmp;
            const int n =
                swr_convert(swr, &outp, AUDIO_TMP_FRAMES, (const uint8_t **) av_frame->data, av_frame->nb_samples);
            if (n > 0) {
                SDL_LockAudio();
                ring_write(audio_tmp, n);
                SDL_UnlockAudio();
            }
        }

        av_frame_unref(av_frame);
    }
}

static int vq_push_wait(AVFrame *src) {
    if (!src) return 0;

    if (!vq_lock) {
        vq_push(src);
        return 1;
    }

    SDL_LockMutex(vq_lock);

    while (decode_run && vq_count >= VFRAME_Q) {
        SDL_CondWait(vq_cond, vq_lock);
    }

    if (!decode_run) {
        SDL_UnlockMutex(vq_lock);
        av_frame_unref(src);
        return 0;
    }

    vq_push(src);
    SDL_UnlockMutex(vq_lock);

    return 1;
}

static void decode_video_pkt(const AVPacket *pkt) {
    if (avcodec_send_packet(video_dec, pkt) < 0) return;

    while (avcodec_receive_frame(video_dec, av_frame) >= 0) {
        if (!vq_push_wait(av_frame)) break;
    }
}

static void drain_video_decoder(void) {
    if (!video_dec) return;

    const int send_ret = avcodec_send_packet(video_dec, NULL);
    if (send_ret < 0 && send_ret != AVERROR_EOF) return;

    while (decode_run) {
        const int recv_ret = avcodec_receive_frame(video_dec, av_frame);

        if (recv_ret == AVERROR_EOF || recv_ret == AVERROR(EAGAIN)) break;
        if (recv_ret < 0) break;

        if (!vq_push_wait(av_frame)) break;
    }
}

static void reset_audio_ring(void) {
    if (!ring_buf) return;

    SDL_LockAudio();
    ring_r = 0;
    ring_w = 0;
    SDL_UnlockAudio();
}

static int restart_stream(void) {
    int seek_ok = -1;

    if (video_si >= 0) seek_ok = av_seek_frame(fmt_ctx, video_si, 0, AVSEEK_FLAG_BACKWARD);
    if (seek_ok < 0) seek_ok = av_seek_frame(fmt_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (seek_ok < 0) return 0;

    if (video_dec) avcodec_flush_buffers(video_dec);
    if (audio_dec) avcodec_flush_buffers(audio_dec);

    SDL_LockMutex(vq_lock);

    if (!is_wallpaper) {
        vq_clear_locked();
        reset_playback_timing(SDL_GetTicks());
    }

    SDL_CondSignal(vq_cond);
    SDL_UnlockMutex(vq_lock);

    if (swr) {
        swr_free(&swr);
        swr = NULL;
    }

    reset_audio_ring();
    return 1;
}

static int decode_thread_fn(void *unused __attribute__((unused))) {
    while (1) {
        SDL_LockMutex(vq_lock);
        while (decode_run && vq_count >= VFRAME_Q - 2)
            SDL_CondWait(vq_cond, vq_lock);
        const int run = decode_run;
        SDL_UnlockMutex(vq_lock);

        if (!run) break;

        if (av_read_frame(fmt_ctx, av_pkt) < 0) {
            av_packet_unref(av_pkt);
            drain_video_decoder();
            if (!restart_stream()) break;
            continue;
        }

        if (av_pkt->stream_index == video_si && video_dec) {
            decode_video_pkt(av_pkt);
        } else if (av_pkt->stream_index == audio_si && audio_dec) {
            decode_audio_pkt(av_pkt);
        }

        av_packet_unref(av_pkt);
    }

    return 0;
}

static void present_cb(lv_timer_t *t __attribute__((unused))) {
    if (!fmt_ctx || !present_frame) return;

    const uint32_t ticks = SDL_GetTicks();
    int moved_frame = 0;
    int64_t moved_pts = 0;

    SDL_LockMutex(vq_lock);

    if (is_wallpaper) {
        if (stall_last_ticks > 0) {
            const uint32_t delta = ticks - stall_last_ticks;

            if (delta > WALLPAPER_STALL_MS) {
                wallpaper_next_ticks = ticks;
            }
        }

        stall_last_ticks = ticks;
        if (wallpaper_next_ticks == 0) wallpaper_next_ticks = ticks;

        if (vq_count > 0 && ticks + DISPLAY_SLACK >= wallpaper_next_ticks) {
            moved_pts = frame_pts_ms(vq_at(0));
            moved_frame = vq_pop_into(present_frame);
            if (moved_frame) last_pts = moved_pts;
            SDL_CondSignal(vq_cond);

            wallpaper_next_ticks += wallpaper_frame_ms;
            if (ticks > wallpaper_next_ticks + WALLPAPER_STALL_MS) {
                wallpaper_next_ticks = ticks + wallpaper_frame_ms;
            }
        }
    } else {
        if (vq_count > 0) {
            const int64_t head_pts = frame_pts_ms(vq_at(0));

            if (last_pts != INT64_MIN && head_pts + LOOP_BACK_MS < last_pts) {
                play_start_ms = ticks - (uint32_t) (head_pts > 0 ? head_pts : 0);
                last_pts = INT64_MIN;
            }
        }

        const int64_t now = (int64_t) ticks - (int64_t) play_start_ms;

        while (vq_count > 0 && frame_pts_ms(vq_at(0)) <= now + DISPLAY_SLACK) {
            if (vq_count >= 2 && frame_pts_ms(vq_at(1)) <= now + DISPLAY_SLACK) {
                vq_pop();
                SDL_CondSignal(vq_cond);
                continue;
            }

            moved_pts = frame_pts_ms(vq_at(0));
            moved_frame = vq_pop_into(present_frame);
            if (moved_frame) last_pts = moved_pts;
            SDL_CondSignal(vq_cond);
            break;
        }
    }

    SDL_UnlockMutex(vq_lock);

    if (!moved_frame) return;

    video_upload(present_frame);

    if (has_frame) display_composite_frame();
}

static int start_decode_thread(void) {
    vq_lock = SDL_CreateMutex();
    vq_cond = SDL_CreateCond();

    if (!vq_lock || !vq_cond) return 0;

    decode_run = 1;
    decode_thread = SDL_CreateThread(decode_thread_fn, "video_decode", NULL);
    if (!decode_thread) {
        decode_run = 0;
        return 0;
    }

    return 1;
}

static void stop_decode_thread(void) {
    if (decode_thread) {
        SDL_LockMutex(vq_lock);
        decode_run = 0;
        SDL_CondSignal(vq_cond);
        SDL_UnlockMutex(vq_lock);

        SDL_WaitThread(decode_thread, NULL);
        decode_thread = NULL;
    }

    if (vq_cond) {
        SDL_DestroyCond(vq_cond);
        vq_cond = NULL;
    }

    if (vq_lock) {
        SDL_DestroyMutex(vq_lock);
        vq_lock = NULL;
    }
}

static void cleanup(void) {
    stop_decode_thread();

    if (is_wallpaper) {
        display_clear_video_background();
    } else {
        display_clear_video_overlay();
    }

    dim_alpha = 0;
    has_frame = 0;
    fade_step = 0;
    is_wallpaper = 0;
    last_pts = INT64_MIN;

    if (fade_timer) {
        lv_timer_del(fade_timer);
        fade_timer = NULL;
    }

    if (present_timer) {
        lv_timer_del(present_timer);
        present_timer = NULL;
    }

    Mix_HookMusic(NULL, NULL);

    if (swr) {
        swr_free(&swr);
        swr = NULL;
    }

    if (ring_buf) {
        reset_audio_ring();
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

    if (present_frame) {
        av_frame_free(&present_frame);
        present_frame = NULL;
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
    stall_last_ticks = 0;
    wallpaper_frame_ms = WALLPAPER_DEF_FRAME_MS;
    wallpaper_next_ticks = 0;
    sdl_ren = NULL;
}

static void preview_open(void) {
    if (avformat_open_input(&fmt_ctx, video_path, NULL, NULL) < 0) return;

    fmt_ctx->probesize = 1000000;
    fmt_ctx->max_analyze_duration = 2000000;

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) goto fail;

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        const AVCodecParameters *par = fmt_ctx->streams[i]->codecpar;

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
    present_frame = av_frame_alloc();
    av_pkt = av_packet_alloc();

    if (!av_frame || !present_frame || !av_pkt) goto fail;
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
            ring_buf = calloc((size_t) ring_frames * 2, sizeof(float));

            if (ring_buf) {
                ring_r = ring_w = 0;
                Mix_HaltMusic();
                Mix_HookMusic(audio_hook_cb, NULL);
            }
        }
    }

    reset_playback_timing(SDL_GetTicks());

    if (!start_decode_thread()) goto fail;

    display_set_video_overlay(sdl_overlay_cb);

    present_timer = lv_timer_create(present_cb, PRESENT_MS, NULL);
    fade_timer = lv_timer_create(fade_cb, FADE_STEP_MS, NULL);

    return;

fail:
    cleanup();
}

static void preview_timer_cb(lv_timer_t *t __attribute__((unused))) {

    lv_timer_pause(preview_timer);
    if (!preview_pending_path[0]) return;

    snprintf(video_path, sizeof(video_path), "%s", preview_pending_path);
    preview_pending_path[0] = '\0';
    cleanup();
    preview_open();
}

static void wallpaper_open(void) {
    is_wallpaper = 1;

    if (avformat_open_input(&fmt_ctx, video_path, NULL, NULL) < 0) goto fail;

    fmt_ctx->probesize = 32768;
    fmt_ctx->max_analyze_duration = 0;

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) goto fail;

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        const AVCodecParameters *par = fmt_ctx->streams[i]->codecpar;

        if (par->codec_type == AVMEDIA_TYPE_VIDEO && video_si < 0) {
            const AVCodec *dec = avcodec_find_decoder(par->codec_id);
            if (!dec) continue;
            video_dec = avcodec_alloc_context3(dec);

            if (!video_dec) continue;
            avcodec_parameters_to_context(video_dec, par);

            video_dec->thread_count = 2;
            video_dec->thread_type = FF_THREAD_SLICE;

            if (avcodec_open2(video_dec, dec, NULL) < 0) {
                avcodec_free_context(&video_dec);
                continue;
            }

            video_si = (int) i;
            video_tb = fmt_ctx->streams[i]->time_base;

            wallpaper_frame_ms = stream_frame_ms(fmt_ctx->streams[i]);
        }
    }

    if (video_si < 0) goto fail;

    sdl_ren = display_get_renderer();
    if (!sdl_ren) goto fail;

    av_frame = av_frame_alloc();
    present_frame = av_frame_alloc();
    av_pkt = av_packet_alloc();
    if (!av_frame || !present_frame || !av_pkt) goto fail;

    for (int i = 0; i < VFRAME_Q; i++) {
        vq[i] = av_frame_alloc();
        if (!vq[i]) goto fail;
    }

    vq_head = vq_tail = vq_count = 0;
    reset_playback_timing(SDL_GetTicks());

    for (int i = 0; i < 32 && !has_frame; i++) {
        if (av_read_frame(fmt_ctx, av_pkt) < 0) {
            av_packet_unref(av_pkt);
            break;
        }

        if (av_pkt->stream_index == video_si) decode_video_pkt(av_pkt);
        av_packet_unref(av_pkt);

        if (vq_count > 0) {
            video_upload(vq_at(0));
            vq_pop();
        }
    }

    const uint32_t start_ticks = SDL_GetTicks();
    reset_playback_timing(start_ticks);
    if (has_frame) wallpaper_next_ticks = start_ticks + wallpaper_frame_ms;

    if (!start_decode_thread()) goto fail;

    display_set_video_background(wallpaper_cb);
    present_timer = lv_timer_create(present_cb, PRESENT_MS, NULL);
    return;

fail:
    cleanup();
}

void video_wallpaper_play(const char *path) {
    if (video_wallpaper_active() && strcmp(video_path, path) == 0) return;

    cleanup();
    snprintf(video_path, sizeof(video_path), "%s", path);
    wallpaper_open();
}

void video_wallpaper_stop(void) {
    cleanup();
    video_path[0] = '\0';
}

int video_wallpaper_active(void) {
    return is_wallpaper && present_timer != NULL;
}

void video_preview_arm(const char *path, const int delay_ms, const lv_obj_t *container, const lv_obj_t *box_img) {
    (void) container;
    (void) box_img;

    if (is_wallpaper && video_path[0] && !saved_wall_path[0]) {
        snprintf(saved_wall_path, sizeof(saved_wall_path), "%s", video_path);
    }

    snprintf(preview_pending_path, sizeof(preview_pending_path), "%s", path);

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
    return present_timer != NULL && !is_wallpaper;
}

void video_preview_cancel(void) {
    if (preview_timer) lv_timer_pause(preview_timer);
    preview_pending_path[0] = '\0';

    if (is_wallpaper) return;

    cleanup();
    video_path[0] = '\0';

    if (saved_wall_path[0]) {
        snprintf(video_path, sizeof(video_path), "%s", saved_wall_path);
        saved_wall_path[0] = '\0';
        wallpaper_open();
    }
}

void video_preview_destroy(void) {
    if (preview_timer) {
        lv_timer_del(preview_timer);
        preview_timer = NULL;
    }

    preview_pending_path[0] = '\0';
    cleanup();
    saved_wall_path[0] = '\0';
    video_path[0] = '\0';
}
