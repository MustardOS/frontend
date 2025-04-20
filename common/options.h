#pragma once

#define TEST_IMAGE 1

#define MAX_BUFFER_SIZE 1024

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define IDLE_MS 16 /* ~60 FPS */

#define TIMER_DATETIME  16384
#define TIMER_CAPACITY  16384
#define TIMER_STATUS    1024
#define TIMER_BLUETOOTH 4096
#define TIMER_NETWORK   4096
#define TIMER_BATTERY   4096
#define TIMER_REFRESH   IDLE_MS
#define TIMER_SYSINFO   1024

#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define OSK_UPPER "ABC"
#define OSK_LOWER "abc"
#define OSK_CHAR  "!@#"
#define OSK_DONE  "OK"

#define EXPLORE_DIR "/tmp/explore_dir"

#define CONTENT_PATH "/mnt/union/ROMS"
#define OPTION_SKIP  "/tmp/skip_opt"

#define INTERNAL_PATH    "/opt/muos/"
#define INTERNAL_THEME   INTERNAL_PATH "default/MUOS/theme/active"
#define INTERNAL_OVERLAY INTERNAL_PATH "share/overlay"

#define KIOSK_CONFIG   INTERNAL_PATH "config/kiosk.ini"
#define LAST_PLAY_FILE INTERNAL_PATH "config/lastplay.txt"
#define MUOS_VERSION   INTERNAL_PATH "config/version.txt"

#define RUN_PATH "/run/muos/"

#define ADD_MODE_WORK "/tmp/add_mode_work"
#define ADD_MODE_DONE "/tmp/add_mode_done"
#define ADD_MODE_FROM "/tmp/add_mode_from"

#define RUN_DEVICE_PATH  RUN_PATH "device/"
#define RUN_GLOBAL_PATH  RUN_PATH "global/"
#define RUN_KIOSK_PATH   RUN_PATH "kiosk/"
#define RUN_STORAGE_PATH RUN_PATH "storage/"
#define RUN_SYSTEM_PATH  RUN_PATH "system/"

#define STORAGE_THEME RUN_STORAGE_PATH "theme/active"
#define STORAGE_SHOTS RUN_STORAGE_PATH "screenshot"

#define INFO_CAT_PATH RUN_STORAGE_PATH "info/catalogue"
#define INFO_COR_PATH RUN_STORAGE_PATH "info/core"
#define INFO_CFG_PATH RUN_STORAGE_PATH "info/config"
#define INFO_CNT_PATH RUN_STORAGE_PATH "info/controller"
#define INFO_COL_PATH RUN_STORAGE_PATH "info/collection"
#define INFO_HIS_PATH RUN_STORAGE_PATH "info/history"
#define INFO_NAM_PATH RUN_STORAGE_PATH "info/name"

#define MUOS_APPS_PATH "MUOS/application"
#define MUOS_TASK_PATH "MUOS/task"
#define MUOS_INFO_PATH "MUOS/info"

#define STORE_LOC_BIOS "MUOS/bios"
#define STORE_LOC_RARC "MUOS/retroarch"
#define STORE_LOC_MUSC "MUOS/music"
#define STORE_LOC_SAVE "MUOS/save"
#define STORE_LOC_SCRS "MUOS/screenshot"
#define STORE_LOC_THEM "MUOS/theme"
#define STORE_LOC_PCAT "MUOS/package/catalogue"
#define STORE_LOC_PCON "MUOS/package/config"
#define STORE_LOC_LANG "MUOS/language"
#define STORE_LOC_NETW "MUOS/network"
#define STORE_LOC_SYCT "MUOS/syncthing"
#define STORE_LOC_INIT "MUOS/init"

#define STORE_LOC_ASIN MUOS_INFO_PATH "/assign"
#define STORE_LOC_CLOG MUOS_INFO_PATH "/catalogue"
#define STORE_LOC_NAME MUOS_INFO_PATH "/name"
#define STORE_LOC_CONF MUOS_INFO_PATH "/config"
#define STORE_LOC_CORE MUOS_INFO_PATH "/core"
#define STORE_LOC_COLL MUOS_INFO_PATH "/collection"
#define STORE_LOC_HIST MUOS_INFO_PATH "/history"

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
#define APP_LAUNCHER    "mux_launch.sh"

#define FONT_PANEL_FOLDER  "panel"
#define FONT_HEADER_FOLDER "header"
#define FONT_FOOTER_FOLDER "footer"
