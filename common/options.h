#pragma once

#define TEST_IMAGE 1
#define MUX_CALLER "MustardOS FE Spectacular"

#define MAX_BUFFER_SIZE 1024

// The rough calculation is as follows if a theme uses
// a unique font for each individual module and panel...
// module_count * (screen + content section + header + footer)
#define FONT_CACHE_MAX  256

#define RTC_MAX_RETRIES 5
#define RTC_RETRY_DELAY 1

#define THEME_PREVIEW_DELAY 1000
#define DEFAULT_TEMPERATURE 30

#define IDLE_MS 16 // ~60 FPS
#define IDLE_FZ (12 * 60 * 60 * 1000U) // Freeze for 12 hours!

#define MU_OBJ_FLAG_HIDE_FLOAT (LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING)
#define MU_OBJ_MAIN_DEFAULT    (LV_PART_MAIN | LV_STATE_DEFAULT)
#define MU_OBJ_MAIN_FOCUS      (LV_PART_MAIN | LV_STATE_FOCUSED)
#define MU_OBJ_MAIN_SCROLL     (LV_PART_MAIN | LV_STATE_SCROLLED)
#define MU_OBJ_INDI_DEFAULT    (LV_PART_INDICATOR | LV_STATE_DEFAULT)
#define MU_OBJ_INDI_FOCUS      (LV_PART_INDICATOR | LV_STATE_FOCUSED)
#define MU_OBJ_SELECT_DEFAULT  (LV_PART_SELECTED | LV_STATE_DEFAULT)
#define MU_OBJ_SELECT_FOCUS    (LV_PART_SELECTED | LV_STATE_FOCUSED)

#define TIMER_DATETIME  8192
#define TIMER_BLUETOOTH 4096
#define TIMER_NETWORK   4096
#define TIMER_BATTERY   4096
#define TIMER_CAPACITY  8192
#define TIMER_STATUS    1024
#define TIMER_SYSINFO   2048
#define TIMER_IDLE      512
#define TIMER_REFRESH   IDLE_MS

#define TIME_STRING    "%Y-%m-%d %H:%M"
#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define LOCAL_TIME "/etc/localtime"

#define OPT_PATH "/opt/muos/"
#define RUN_PATH "/run/muos/"

#define IDLE_STATE RUN_PATH "idle_state"

#define USED_RESET OPT_PATH "config/system/used_reset"
#define DONE_RESET "/tmp/done_reset"

#define OSK_UPPER "ABC"
#define OSK_LOWER "abc"
#define OSK_CHAR  "!@#"
#define OSK_DONE  "OK"

#define CHIME_DONE "/tmp/chime_done"
#define SAFE_QUIT  "/tmp/safe_quit"

#define EXPLORE_DIR  "/tmp/explore_dir"
#define EXPLORE_NAME "/tmp/explore_name"

#define UNION_ROM_PATH "/mnt/union/ROMS"
#define OPTION_SKIP    "/tmp/skip_opt"

#define INTERNAL_THEME OPT_PATH "share/theme/MustardOS"
#define LAST_PLAY_FILE OPT_PATH "config/boot/last_play"
#define BGM_SILENCE    OPT_PATH "share/media/silence.ogg"

#define ADD_MODE_WORK "/tmp/add_mode_work"
#define ADD_MODE_DONE "/tmp/add_mode_done"
#define ADD_MODE_FROM "/tmp/add_mode_from"

#define COLLECTION_DIR "/tmp/collection_dir"

#define CONF_DEVICE_PATH OPT_PATH "device/config/"
#define CONF_CONFIG_PATH OPT_PATH "config/"
#define CONF_KIOSK_PATH  OPT_PATH "kiosk/"
#define SORTING_CONFIG_PATH CONF_CONFIG_PATH "sort/"

#define RUN_STORAGE_PATH RUN_PATH "storage/"
#define OPT_SHARE_PATH   OPT_PATH "share/"

#define STORAGE_SHOTS   RUN_STORAGE_PATH "screenshot"
#define STORAGE_MUSIC   RUN_STORAGE_PATH "music"

#define STORAGE_HOTKEY  OPT_SHARE_PATH "hotkey"
#define STORAGE_SOUND   OPT_SHARE_PATH "media/sound"
#define STORAGE_LANG    OPT_SHARE_PATH "language"
#define STORAGE_OVERLAY OPT_SHARE_PATH "overlay"
#define STORAGE_FILTER  OPT_SHARE_PATH "filter"

#define INFO_CFG_PATH OPT_SHARE_PATH "info/config"
#define INFO_COR_PATH OPT_SHARE_PATH "info/core"
#define INFO_CNT_PATH OPT_SHARE_PATH "info/controller"
#define INFO_GCD_PATH OPT_SHARE_PATH "info/gamecontrollerdb"

#define INFO_CAT_PATH RUN_STORAGE_PATH "info/catalogue"
#define INFO_COL_PATH RUN_STORAGE_PATH "info/collection"
#define INFO_CKS_PATH RUN_STORAGE_PATH "info/collection/kiosk"
#define INFO_HIS_PATH RUN_STORAGE_PATH "info/history"
#define INFO_NAM_PATH RUN_STORAGE_PATH "info/name"
#define INFO_ACT_PATH RUN_STORAGE_PATH "info/track"

#define MUOS_ARCH_PATH "ARCHIVE"
#define MUOS_BASE_PATH "MUOS"

#define MUOS_APPS_PATH MUOS_BASE_PATH "/application"
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

#define STORE_LOC_ASIN OPT_SHARE_PATH "info/assign"
#define STORE_LOC_CLOG MUOS_INFO_PATH "/catalogue"
#define STORE_LOC_MUSI MUOS_BASE_PATH "/music"
#define STORE_LOC_NAME MUOS_INFO_PATH "/name"
#define STORE_LOC_COLL MUOS_INFO_PATH "/collection"
#define STORE_LOC_HIST MUOS_INFO_PATH "/history"

#define MUOS_ACT_LOAD "/tmp/act_go" // Module Action
#define MUOS_AIN_LOAD "/tmp/ain_go" // Application Last Index
#define MUOS_AIX_LOAD "/tmp/aix_go" // Core/System Assignment Index
#define MUOS_APL_LOAD "/tmp/apl_go" // Application Content Loader
#define MUOS_APP_LOAD "/tmp/app_go" // Application Launch
#define MUOS_ASS_FROM "/tmp/ass_fm" // Core/System Assignment Module Return
#define MUOS_ASS_LOAD "/tmp/ass_go" // Core/System Assignment Loader
#define MUOS_BTL_LOAD "/tmp/btl_go" // Refresh Bootlogo on Restart or Shutdown
#define MUOS_CIX_LOAD "/tmp/cix_go" // Content Item Index
#define MUOS_CON_LOAD "/tmp/con_go" // Control Scheme
#define MUOS_DBI_LOAD "/tmp/dbi_go" // Device Backup Last Index
#define MUOS_FLT_LOAD "/tmp/flt_go" // Colour Filter Name
#define MUOS_GOV_LOAD "/tmp/gov_go" // Governor Assignment Loader
#define MUOS_HST_LOAD "/tmp/hst_go" // Last History Index
#define MUOS_IDX_LOAD "/tmp/idx_go" // Last Known Item Index
#define MUOS_OPI_LOAD "/tmp/opi_go" // Content Options Last Index
#define MUOS_PDI_LOAD "/tmp/pdi_go" // Last Directory String
#define MUOS_PIK_LOAD "/tmp/pik_go" // Customisation Picker Launch
#define MUOS_PIN_LOAD "/tmp/pin_go" // Customisation Picker Last Index
#define MUOS_RAC_LOAD "/tmp/rac_go" // RetroArch Configuration
#define MUOS_RES_LOAD "/tmp/res_go" // Full Path of ROM Content Search
#define MUOS_ROM_LOAD "/tmp/rom_go" // ROM Content for Launching
#define MUOS_SAA_LOAD "/tmp/saa_go" // Auto Assign Core Flag
#define MUOS_SAG_LOAD "/tmp/sag_go" // Auto Assign Governor Flag
#define MUOS_SAR_LOAD "/tmp/sar_go" // Auto Assign RetroArch Config Flag
#define MUOS_SIN_LOAD "/tmp/sin_go" // Storage Preference Last Index
#define MUOS_SYS_LOAD "/tmp/sys_go" // Core/System Assignment Flag
#define MUOS_TIN_LOAD "/tmp/tin_go" // Task Toolkit Last Index

#define MUX_BOOT_AUTH   "/tmp/mux_boot_auth"
#define MUX_BLANK       "/tmp/mux_blank"
#define MUX_DO_REFRESH  "/tmp/hdmi_do_refresh"
#define FRIENDLY_RESULT "/tmp/f_result.json"
#define MANUAL_RA_LOAD  "/tmp/ra_no_load"
#define PLAYTIME_DATA   "playtime_data.json"
#define APP_LAUNCHER    "mux_launch.sh"
#define APP_LANGUAGE    "mux_lang.ini"
#define THEME_DATA      "theme_data.json"
#define EXTRA_DATA      "extra_data.json"
#define LANG_ARCHIVE    "lang.muxzip"
#define FONT_PANEL_DIR  "panel"
#define FONT_HEADER_DIR "header"
#define FONT_FOOTER_DIR "footer"

#define CFG_INT_FIELD(FIELD, PATH, DEFAULT)           \
    do {                                              \
        snprintf(buffer, sizeof(buffer), "%s", PATH); \
        cfg_write_def_int(buffer, DEFAULT);           \
        FIELD = read_line_int_from(buffer, 1);        \
    } while(0)

#define CFG_STR_FIELD(FIELD, PATH, DEFAULT)                                  \
    do {                                                                     \
        snprintf(buffer, sizeof(buffer), "%s", PATH);                        \
        cfg_write_def_char(buffer, DEFAULT);                                 \
        strncpy(FIELD, read_line_char_from(buffer, 1), MAX_BUFFER_SIZE - 1); \
        FIELD[MAX_BUFFER_SIZE - 1] = '\0';                                   \
    } while(0)
