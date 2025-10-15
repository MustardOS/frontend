#pragma once

#define TEST_IMAGE 0
#define MUX_CALLER "MustardOS FE Spectacular"

#define MAX_BUFFER_SIZE 1024

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define IDLE_MS 16 /* ~60 FPS */

#define MU_OBJ_FLAG_HIDE_FLOAT (LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING)
#define MU_OBJ_MAIN_DEFAULT    (LV_PART_MAIN | LV_STATE_DEFAULT)
#define MU_OBJ_MAIN_FOCUS      (LV_PART_MAIN | LV_STATE_FOCUSED)
#define MU_OBJ_MAIN_SCROLL     (LV_PART_MAIN | LV_STATE_SCROLLED)
#define MU_OBJ_INDI_DEFAULT    (LV_PART_INDICATOR | LV_STATE_DEFAULT)
#define MU_OBJ_INDI_FOCUS      (LV_PART_INDICATOR | LV_STATE_FOCUSED)
#define MU_OBJ_SELECT_DEFAULT  (LV_PART_SELECTED | LV_STATE_DEFAULT)
#define MU_OBJ_SELECT_FOCUS    (LV_PART_SELECTED | LV_STATE_FOCUSED)

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

#define LOCAL_TIME "/etc/localtime"

#define OPT_PATH "/opt/muos/"
#define RUN_PATH "/run/muos/"

#define USED_RESET OPT_PATH "config/system/used_reset"
#define DONE_RESET "/tmp/done_reset"

#define OSK_UPPER "ABC"
#define OSK_LOWER "abc"
#define OSK_CHAR  "!@#"
#define OSK_DONE  "OK"

#define CHIME_DONE "/tmp/chime_done"
#define SAFE_QUIT  "/tmp/safe_quit"

#define EXPLORE_DIR "/tmp/explore_dir"
#define EXPLORE_NAME "/tmp/explore_name"

#define STORAGE_PATH "/mnt/union/ROMS"
#define OPTION_SKIP  "/tmp/skip_opt"

#define INTERNAL_THEME   OPT_PATH "share/theme/active"
#define INTERNAL_OVERLAY OPT_PATH "share/overlay"

#define LAST_PLAY_FILE OPT_PATH "config/boot/last_play"
#define BGM_SILENCE    OPT_PATH "share/media/silence.ogg"

#define ADD_MODE_WORK "/tmp/add_mode_work"
#define ADD_MODE_DONE "/tmp/add_mode_done"
#define ADD_MODE_FROM "/tmp/add_mode_from"

#define COLLECTION_DIR "/tmp/collection_dir"

#define CONF_DEVICE_PATH OPT_PATH "device/config/"
#define CONF_CONFIG_PATH OPT_PATH "config/"
#define CONF_KIOSK_PATH  OPT_PATH "kiosk/"

#define RUN_STORAGE_PATH RUN_PATH "storage/"
#define RUN_SHARE_PATH   OPT_PATH "share/"

#define STORAGE_THEME RUN_STORAGE_PATH "theme/active"
#define STORAGE_SHOTS RUN_STORAGE_PATH "screenshot"
#define STORAGE_MUSIC RUN_STORAGE_PATH "music"
#define STORAGE_SOUND RUN_SHARE_PATH   "media/sound"
#define STORAGE_LANG  RUN_SHARE_PATH   "language"
#define THEME_CAT_PATH STORAGE_THEME   "/catalogue"

#define INFO_CAT_PATH RUN_STORAGE_PATH "info/catalogue"
#define INFO_COR_PATH RUN_SHARE_PATH   "info/core"
#define INFO_CFG_PATH RUN_SHARE_PATH   "info/config"
#define INFO_CNT_PATH RUN_SHARE_PATH   "info/controller"
#define INFO_COL_PATH RUN_STORAGE_PATH "info/collection"
#define INFO_CKS_PATH RUN_STORAGE_PATH "info/collection/kiosk"
#define INFO_GCD_PATH RUN_SHARE_PATH   "info/gamecontrollerdb"
#define INFO_HIS_PATH RUN_STORAGE_PATH "info/history"
#define INFO_NAM_PATH RUN_STORAGE_PATH "info/name"
#define INFO_ACT_PATH RUN_STORAGE_PATH "info/track"

#define MUOS_ARCH_PATH "ARCHIVE"
#define MUOS_BASE_PATH "MUOS"
#define MUOS_APPS_PATH MUOS_BASE_PATH "/application"
#define MUOS_TASK_PATH MUOS_BASE_PATH "/task"
#define MUOS_INFO_PATH MUOS_BASE_PATH "/info"

#define STORE_LOC_APPS MUOS_BASE_PATH "/application"
#define STORE_LOC_BIOS MUOS_BASE_PATH "/bios"
#define STORE_LOC_SAVE MUOS_BASE_PATH "/save"
#define STORE_LOC_SCRS MUOS_BASE_PATH "/screenshot"
#define STORE_LOC_THEM MUOS_BASE_PATH "/theme"
#define STORE_LOC_PACK MUOS_BASE_PATH "/package"
#define STORE_LOC_PCAT STORE_LOC_PACK "/catalogue"
#define STORE_LOC_PCON STORE_LOC_PACK "/config"
#define STORE_LOC_NETW MUOS_BASE_PATH "/network"
#define STORE_LOC_SYCT MUOS_BASE_PATH "/syncthing"
#define STORE_LOC_INIT MUOS_BASE_PATH "/init"
#define STORE_LOC_TRAK MUOS_BASE_PATH "/info/track"

#define STORE_LOC_ASIN RUN_SHARE_PATH "info/assign"
#define STORE_LOC_CLOG MUOS_INFO_PATH "/catalogue"
#define STORE_LOC_MUSI MUOS_BASE_PATH "/music"
#define STORE_LOC_NAME MUOS_INFO_PATH "/name"
#define STORE_LOC_CONF RUN_SHARE_PATH "info/config"
#define STORE_LOC_CORE RUN_SHARE_PATH "info/core"
#define STORE_LOC_GCDB RUN_SHARE_PATH "info/gamecontrollerdb"
#define STORE_LOC_COLL MUOS_INFO_PATH "/collection"
#define STORE_LOC_HIST MUOS_INFO_PATH "/history"
#define STORE_LOC_PLAY MUOS_INFO_PATH "/track"

#define MUOS_ACT_LOAD "/tmp/act_go" // Module Action
#define MUOS_AIN_LOAD "/tmp/ain_go" // Application Last Index
#define MUOS_APL_LOAD "/tmp/apl_go" // Application Content Loader
#define MUOS_APP_LOAD "/tmp/app_go" // Application Launch
#define MUOS_ASS_LOAD "/tmp/ass_go" // Core/System Assignment Loader
#define MUOS_ASS_FROM "/tmp/ass_from" // Core/System Assignment Loader return to module
#define MUOS_CON_LOAD "/tmp/con_go" // Control Scheme
#define MUOS_DBI_LOAD "/tmp/dbi_go" // Device Backup Last Index
#define MUOS_IDX_LOAD "/tmp/idx_go" // Last Known Item Index
#define MUOS_GOV_LOAD "/tmp/gov_go" // Governor Assignment Loader
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

#define MUX_BOOT_AUTH   "/tmp/mux_boot_auth"

#define BRIGHT_PERC "/tmp/current_brightness_percent"
#define VOLUME_PERC "/tmp/current_volume_percent"

#define MUX_BLANK       "/tmp/mux_blank"
#define PLAYTIME_DATA   "playtime_data.json"
#define FRIENDLY_RESULT "/tmp/f_result.json"
#define MANUAL_RA_LOAD  "/tmp/ra_no_load"
#define APP_LAUNCHER    "mux_launch.sh"

#define THEME_DATA   "theme_data.json"
#define EXTRA_DATA   "extra_data.json"

#define FONT_PANEL_FOLDER  "panel"
#define FONT_HEADER_FOLDER "header"
#define FONT_FOOTER_FOLDER "footer"

#define CFG_INT_FIELD(FIELD, PATH, DEFAULT)                       \
    snprintf(buffer, sizeof(buffer), "%s", PATH);                 \
    FIELD = (int)({                                               \
        char *ep;                                                 \
        long value = strtol(read_all_char_from(buffer), &ep, 10); \
        *ep ? DEFAULT : value;                                    \
    });

#define CFG_STR_FIELD(FIELD, PATH, DEFAULT)                                     \
    snprintf(buffer, sizeof(buffer), "%s", PATH);                               \
    strncpy(FIELD, read_all_char_from(buffer) ?: DEFAULT, MAX_BUFFER_SIZE - 1); \
    FIELD[MAX_BUFFER_SIZE - 1] = '\0';
