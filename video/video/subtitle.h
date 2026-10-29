#pragma once

#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

void subtitle_open(const AVFormatContext *format, const char *uri, int video_width, int video_height);
void subtitle_close(void);
int subtitle_track_count(void);
const char *subtitle_track_label(int index);
int subtitle_track_selected(void);
int subtitle_select(int index);
int subtitle_stream(void);
void subtitle_packet(const AVPacket *packet);
void subtitle_flush(void);
int subtitle_set_position(double position);
void subtitle_render(SDL_Renderer *renderer, const SDL_Rect *video, double angle);
void subtitle_settings_changed(void);
