#pragma once

#define TEST_IMAGE 1

#define MAX_BUFFER_SIZE 512

#define DUMMY_DIR "00000DIR"

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define INTERNAL_PATH "/opt/muos"
#define STORAGE_PATH  "/run/muos/storage"

#define MUOS_ACT_LOAD "/tmp/act_go" // Module Action
#define MUOS_APP_LOAD "/tmp/app_go" // Application Launch
#define MUOS_ASS_LOAD "/tmp/ass_go" // Core/System Assignment Loader
#define MUOS_IDX_LOAD "/tmp/idx_go" // Last Known Item Index
#define MUOS_PDI_LOAD "/tmp/pdi_go" // Last Directory String
#define MUOS_ROM_LOAD "/tmp/rom_go" // ROM Content for Launching
#define MUOS_SAA_LOAD "/tmp/saa_go" // Auto Assignment Flag
#define MUOS_SYS_LOAD "/tmp/sys_go" // Core/System Assignment Flag

#define BRIGHT_FILE "/opt/muos/config/brightness.txt"
#define VOLUME_FILE "/opt/muos/config/volume.txt"

#define BRIGHT_PERC "/tmp/current_brightness_percent"
#define VOLUME_PERC "/tmp/current_volume_percent"

#define FONT_PANEL_FOLDER  "panel"
#define FONT_HEADER_FOLDER "header"
#define FONT_FOOTER_FOLDER "footer"
