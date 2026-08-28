#pragma once

#define MUINPUT_STATE_DIR "/run/muinput"

int runtime_prepare_state_dir(void);
int runtime_acquire_instance_lock(void);
