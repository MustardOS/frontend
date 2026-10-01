#pragma once

void video_loading_show(const char *message);
void video_loading_show_detail(const char *message, const char *detail, int opaque);
void video_loading_show_transparent(const char *message, const char *detail);
void video_loading_prepare_content(void);
void video_loading_hide(void);
