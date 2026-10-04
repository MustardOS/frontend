#pragma once

#include "../core/audio.h"

int wasabi_audio_ui_init(const wasabi_audio_info *information);
int wasabi_audio_ui_update(double position, double duration, int paused);
void wasabi_audio_ui_modes_changed(void);
void wasabi_audio_ui_shutdown(void);
void wasabi_audio_ui_set_hidden(int hidden);
