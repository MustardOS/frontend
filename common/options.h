#pragma once

#define TEST_IMAGE 1
#define MUX_CALLER "MustardOS FE Spectacular"

#define MAX_BUFFER_SIZE 1024

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define IDLE_MS 16 /* ~60 FPS */

#define MU_OBJ_FLAG_HIDE_FLOAT (LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING)

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

#define USED_RESET "/opt/muos/config/system/used_reset"
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

#define INTERNAL_PATH    "/opt/muos/"
#define INTERNAL_THEME   INTERNAL_PATH "default/MUOS/theme/active"
#define INTERNAL_OVERLAY INTERNAL_PATH "share/overlay"

#define LAST_PLAY_FILE INTERNAL_PATH "config/boot/last_play"
#define BGM_SILENCE    INTERNAL_PATH "share/media/silence.ogg"

#define RUN_PATH "/run/muos/"

#define ADD_MODE_WORK "/tmp/add_mode_work"
#define ADD_MODE_DONE "/tmp/add_mode_done"
#define ADD_MODE_FROM "/tmp/add_mode_from"

#define COLLECTION_DIR "/tmp/collection_dir"

#define CONF_DEVICE_PATH INTERNAL_PATH "device/config/"
#define CONF_CONFIG_PATH INTERNAL_PATH "config/"
#define CONF_KIOSK_PATH  INTERNAL_PATH "kiosk/"

#define RUN_STORAGE_PATH RUN_PATH "storage/"

#define STORAGE_THEME RUN_STORAGE_PATH "theme/active"
#define STORAGE_SHOTS RUN_STORAGE_PATH "screenshot"
#define STORAGE_MUSIC RUN_STORAGE_PATH "music"
#define STORAGE_SOUND RUN_STORAGE_PATH "sound"

#define INFO_CAT_PATH RUN_STORAGE_PATH "info/catalogue"
#define INFO_COR_PATH RUN_STORAGE_PATH "info/core"
#define INFO_CFG_PATH RUN_STORAGE_PATH "info/config"
#define INFO_CNT_PATH RUN_STORAGE_PATH "info/controller"
#define INFO_COL_PATH RUN_STORAGE_PATH "info/collection"
#define INFO_CKS_PATH RUN_STORAGE_PATH "info/collection/kiosk"
#define INFO_GCD_PATH RUN_STORAGE_PATH "info/gamecontrollerdb"
#define INFO_HIS_PATH RUN_STORAGE_PATH "info/history"
#define INFO_NAM_PATH RUN_STORAGE_PATH "info/name"
#define INFO_ACT_PATH RUN_STORAGE_PATH "info/track"

#define MUOS_APPS_PATH "MUOS/application"
#define MUOS_TASK_PATH "MUOS/task"
#define MUOS_INFO_PATH "MUOS/info"

#define STORE_LOC_BIOS "MUOS/bios"
#define STORE_LOC_RARC "MUOS/retroarch"
#define STORE_LOC_MUSC "MUOS/music"
#define STORE_LOC_SOUN "MUOS/sound"
#define STORE_LOC_SAVE "MUOS/save"
#define STORE_LOC_SCRS "MUOS/screenshot"
#define STORE_LOC_THEM "MUOS/theme"
#define STORE_LOC_PCAT "MUOS/package/catalogue"
#define STORE_LOC_PCON "MUOS/package/config"
#define STORE_LOC_PLOG "MUOS/package/bootlogo"
#define STORE_LOC_LANG "MUOS/language"
#define STORE_LOC_NETW "MUOS/network"
#define STORE_LOC_SYCT "MUOS/syncthing"
#define STORE_LOC_INIT "MUOS/init"
#define STORE_LOC_ACTI "MUOS/info/track"

#define STORE_LOC_ASIN MUOS_INFO_PATH "/assign"
#define STORE_LOC_CLOG MUOS_INFO_PATH "/catalogue"
#define STORE_LOC_NAME MUOS_INFO_PATH "/name"
#define STORE_LOC_CONF MUOS_INFO_PATH "/config"
#define STORE_LOC_CORE MUOS_INFO_PATH "/core"
#define STORE_LOC_GCDB MUOS_INFO_PATH "/gamecontrollerdb"
#define STORE_LOC_COLL MUOS_INFO_PATH "/collection"
#define STORE_LOC_HIST MUOS_INFO_PATH "/history"
#define STORE_LOC_PLAY MUOS_INFO_PATH "/track"

#define MUOS_ACT_LOAD "/tmp/act_go" // Module Action
#define MUOS_AIN_LOAD "/tmp/ain_go" // Application Last Index
#define MUOS_APL_LOAD "/tmp/apl_go" // Application Content Loader
#define MUOS_APP_LOAD "/tmp/app_go" // Application Launch
#define MUOS_ASS_LOAD "/tmp/ass_go" // Core/System Assignment Loader
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

#define MUX_AUTH "/tmp/mux_auth" // Muxpass Config Authorization
#define MUX_LAUNCHER_AUTH "/tmp/mux_launcher_auth" // Muxpass App and Launcher Authorization

#define BRIGHT_PERC "/tmp/current_brightness_percent"
#define VOLUME_PERC "/tmp/current_volume_percent"

#define PLAYTIME_DATA   "playtime_data.json"
#define FRIENDLY_RESULT "/tmp/f_result.json"
#define MANUAL_RA_LOAD  "/tmp/ra_no_load"
#define APP_LAUNCHER    "mux_launch.sh"

#define THEME_DATA   "theme_data.json"

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
