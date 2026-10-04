#pragma once

#include <libavutil/frame.h>

int video_render_open(void);
void video_render_set_content(const char *content_path, int live);
void video_render_set_audio(int active);
void video_render_audio_samples(const float *samples, int frames, int channels);
int video_render_audio_tick(void);
int video_render_upload(const AVFrame *frame);
int video_render_upload_next(const AVFrame *frame);
int video_render_promote_next(void);
void video_render_set_blend(double amount);

void video_render_settings_changed(void);
void video_render_effects_changed(void);
void video_render_set_clean_capture(int active);
void video_render_set_static(int active);
int video_render_static_tick(void);
void video_render_seek_effect(int direction);
int video_render_crt_power_off(void);
int video_render_crt_tick(void);
void video_render_seek_hold(int direction);
void video_render_seek_release(void);
int video_render_seek_tick(void);

void video_render_close(void);
