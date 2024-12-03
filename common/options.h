#pragma once

#define TEST_IMAGE 1

#define MAX_BUFFER_SIZE 512

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define INTERNAL_PATH  "/opt/muos"
#define INTERNAL_THEME "/opt/muos/default/MUOS/theme/active"

#define LAST_PLAY_FILE "/opt/muos/config/lastplay.txt"

#define STORAGE_PATH   "/run/muos/storage"
#define STORAGE_THEME  "/run/muos/storage/theme/active"

#define INFO_CAT_PATH "/run/muos/storage/info/catalogue"
#define INFO_COR_PATH "/run/muos/storage/info/core"
#define INFO_CFG_PATH "/run/muos/storage/info/config"
#define INFO_CNT_PATH "/run/muos/storage/info/controller"
#define INFO_FAV_PATH "/run/muos/storage/info/favourite"
#define INFO_HIS_PATH "/run/muos/storage/info/history"
#define INFO_NAM_PATH "/run/muos/storage/info/name"

#define MUOS_ACT_LOAD "/tmp/act_go" // Module Action
#define MUOS_AIN_LOAD "/tmp/ain_go" // Application Last Index
#define MUOS_APP_LOAD "/tmp/app_go" // Application Launch
#define MUOS_ASS_LOAD "/tmp/ass_go" // Core/System Assignment Loader
#define MUOS_IDX_LOAD "/tmp/idx_go" // Last Known Item Index
#define MUOS_GOV_LOAD "/tmp/gov_go" // Governor Assignment Loader
#define MUOS_GVR_LOAD "/tmp/gvr_go" // Governor Runtime
#define MUOS_PDI_LOAD "/tmp/pdi_go" // Last Directory String
#define MUOS_PIK_LOAD "/tmp/pik_go" // Customisation Picker Launch
#define MUOS_PIN_LOAD "/tmp/pin_go" // Customisation Picker Last Index
#define MUOS_RES_LOAD "/tmp/res_go" // Full Path of ROM Content Search
#define MUOS_ROM_LOAD "/tmp/rom_go" // ROM Content for Launching
#define MUOS_SAA_LOAD "/tmp/saa_go" // Auto Assign Core Flag
#define MUOS_SAG_LOAD "/tmp/sag_go" // Auto Assign Governor Flag
#define MUOS_SIN_LOAD "/tmp/sin_go" // Storage Preference Last Index
#define MUOS_SYS_LOAD "/tmp/sys_go" // Core/System Assignment Flag
#define MUOS_TIN_LOAD "/tmp/tin_go" // Task Toolkit Last Index

#define BRIGHT_PERC "/tmp/current_brightness_percent"
#define VOLUME_PERC "/tmp/current_volume_percent"

#define FRIENDLY_RESULT "/tmp/f_result.json"

#define FONT_PANEL_FOLDER  "panel"
#define FONT_HEADER_FOLDER "header"
#define FONT_FOOTER_FOLDER "footer"
