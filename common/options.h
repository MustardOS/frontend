#pragma once

#define TEST_IMAGE 1

#define MAX_BUFFER_SIZE 512

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define INTERNAL_PATH  "/opt/muos/"
#define INTERNAL_THEME INTERNAL_PATH "default/MUOS/theme/active"

#define KIOSK_CONFIG   INTERNAL_PATH "config/kiosk.ini"
#define LAST_PLAY_FILE INTERNAL_PATH "config/lastplay.txt"

#define RUN_PATH "/run/muos/"

#define RUN_DEVICE_PATH  RUN_PATH "device/"
#define RUN_GLOBAL_PATH  RUN_PATH "global/"
#define RUN_KIOSK_PATH   RUN_PATH "kiosk/"
#define RUN_STORAGE_PATH RUN_PATH "storage/"
#define RUN_SYSTEM_PATH  RUN_PATH "system/"

#define STORAGE_THEME RUN_STORAGE_PATH "theme/active"

#define INFO_CAT_PATH RUN_STORAGE_PATH "info/catalogue"
#define INFO_COR_PATH RUN_STORAGE_PATH "info/core"
#define INFO_CFG_PATH RUN_STORAGE_PATH "info/config"
#define INFO_CNT_PATH RUN_STORAGE_PATH "info/controller"
#define INFO_FAV_PATH RUN_STORAGE_PATH "info/favourite"
#define INFO_HIS_PATH RUN_STORAGE_PATH "info/history"
#define INFO_NAM_PATH RUN_STORAGE_PATH "info/name"

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
