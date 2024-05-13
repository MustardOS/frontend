#pragma once

#define MAX_BUFFER_SIZE 512
#define DISP_BUF_SIZE   33554432

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define SCREEN_WAIT   256

#define MAX_EVENTS 4

#define SYS_DEVICE "/dev/input/event0"
#define JOY_DEVICE "/dev/input/event1"
#define RTC_DEVICE "/dev/rtc0"

#define ITEM_SKIP 13
#define DUMMY_ROM "ZZZZZZZZ.ZZZ"
#define DUMMY_DIR "00000DIR"

#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define MUOS_CONTENT_LAUNCH "/opt/muos/script/mux/launch.sh"

#define MUOS_TWEAK_UPDATE "/opt/muos/script/mux/tweak.sh"
#define MUOS_THEME_UPDATE "/opt/muos/script/mux/theme.sh"
#define MUOS_WEBSV_UPDATE "/opt/muos/script/web/service.sh"

#define MUOS_BACKUP_SCRIPT_DIR "/mnt/mmc/muos/backup"

#define MUOS_ARCHIVE_EXTRACT "/opt/muos/script/mux/extract.sh"
#define MUOS_ARCHIVE_BACKUP  "/opt/muos/script/mux/backup.sh"

#define MUOS_CONFIG_FILE "/opt/muos/config/config.txt"
#define MUOS_DEVICE_FILE "/opt/muos/config/device.txt"

#define MUOS_SCHEME_DIR "/opt/muos/theme/scheme"
#define MUOS_GLYPH_DIR "/opt/muos/theme/glyph"

#define MUOS_ASSIGN_DIR    "/mnt/mmc/MUOS/info/assign"
#define MUOS_ACTIVITY_DIR  "/mnt/mmc/MUOS/info/activity"
#define MUOS_CACHE_DIR     "/mnt/mmc/MUOS/info/cache"
#define MUOS_CATALOGUE_DIR "/mnt/mmc/MUOS/info/catalogue"
#define MUOS_CORE_DIR      "/mnt/mmc/MUOS/info/core"
#define MUOS_FAVOURITE_DIR "/mnt/mmc/MUOS/info/favourite"
#define MUOS_HISTORY_DIR   "/mnt/mmc/MUOS/info/history"

#define MUOS_THEME_PATH "mnt/mmc/MUOS/theme"
#define MUOS_FONT_PATH  "opt/muos/theme/font"
#define MUOS_IMAGE_PATH "opt/muos/theme/image"
#define MUOS_MUSIC_PATH "opt/muos/theme/music"
#define MUOS_SOUND_PATH "opt/muos/theme/sound"
#define MUOS_SUPP_PATH  "opt/muos/supporter"

#define MUOS_CORE_PATH "mnt/mmc/MUOS/core"
#define MUOS_INFO_PATH "mnt/mmc/MUOS/info"

#define MUOS_CATALOGUE_PATH "mnt/mmc/MUOS/info/catalogue"

#define MUOS_CORE_FILE   "/mnt/mmc/MUOS/info/core.json"
#define MUOS_NAME_FILE   "/mnt/mmc/MUOS/info/name.ini"
#define MUOS_ASSIGN_FILE "/mnt/mmc/MUOS/info/assign.ini"

#define MUOS_PASS_FILE   "/mnt/mmc/MUOS/info/pass.ini"

#define MUOS_NET_FILE "/opt/muos/config/network.txt"
#define NETWORK_ADDRESS "/opt/muos/config/address.txt"

#define RA_CONFIG_FILE "/mnt/mmc/MUOS/.retroarch/retroarch.cfg"
#define RA_CONFIG_CRC "59749d83"

#define GOVERNOR_FILE "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define SCALE_MN_FILE "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq"

#define MUOS_ACT_LOAD "/tmp/act_go"
#define MUOS_ASS_LOAD "/tmp/ass_go"
#define MUOS_AUT_LOAD "/tmp/aut_go"
#define MUOS_SAA_LOAD "/tmp/saa_go"
#define MUOS_ROM_LOAD "/tmp/rom_go"
#define MUOS_SYS_LOAD "/tmp/sys_go"

#define BL_BRIGHT_FILE "/sys/class/backlight/backlight.2/brightness"
#define BL_POWER_FILE  "/sys/class/backlight/backlight.2/bl_power"

#define BL_RST_FILE    "/opt/muos/config/brightness.txt"
#define VOL_RST_FILE   "/opt/muos/config/volume.txt"

#if defined(RELEASE)
#define TEST_IMAGE 0
#else
#define TEST_IMAGE 1
#endif

#if defined(RG35XX)
#define HARDWARE "RG35XX"

#define BATT_CAPACITY "/sys/class/power_supply/battery/capacity"

#define BL_DEF      192
#define BL_MAX      512
#define BL_MIN      16
#define BL_INC      16

#define VOL_SPK     224
#define VOL_DEF	    20
#define VOL_MAX	    40
#define VOL_MIN	    0
#define VOL_INC	    4

#define VOL_SPK_MASTER "DAC PA"
#define VOL_SPK_SWITCH "speaker on off switch" // yes really...
#define VOL_SPK_LEFT   "DAC FL Gain"
#define VOL_SPK_RIGHT  "DAC FR Gain"

#define JOY_A      0
#define JOY_B      1
#define JOY_X      2
#define JOY_Y      3

#define JOY_POWER  4

#define JOY_SELECT 7
#define JOY_START  8
#define JOY_MENU   9
#define JOY_MQUICK 255 // There is no quick press on MENU for the OG
#define JOY_PLUS   10
#define JOY_MINUS  11

#define JOY_L1     5
#define JOY_R1     6
#define JOY_L2     2
#define JOY_R2     5

#define JOY_UP     7
#define JOY_DOWN   7
#define JOY_LEFT   6
#define JOY_RIGHT  6
#endif

#if defined(RG35XXPLUS) || defined(RG28XX)

#if defined(RG28XX)
#define HARDWARE "RG28XX"
#else
#define HARDWARE "RG35XXPLUS"
#endif

#define BATT_CAPACITY "/sys/class/power_supply/axp2202-battery/capacity"
#define BATT_HEALTH   "/sys/class/power_supply/axp2202-battery/health"
#define BATT_VOLTAGE  "/sys/class/power_supply/axp2202-battery/voltage_now"
#define BATT_CHARGER  "/sys/class/power_supply/axp2202-usb/online"

#define BL_DEF      100
#define BL_MAX      255
#define BL_MIN      0
#define BL_INC      10

#define VOL_SPK     100
#define VOL_DEF	    60
#define VOL_MAX	    100
#define VOL_MIN	    0
#define VOL_INC	    10

#define VOL_SPK_MASTER "digital volume"

#define JOY_A      304
#define JOY_B      305
#define JOY_X      307
#define JOY_Y      306

#define JOY_POWER  116 // REMINDER: This actually uses event0 NOT event1 like the rest

#define JOY_SELECT 310
#define JOY_START  311
#define JOY_MENU   312
#define JOY_MQUICK 354 // This is a quick press of MENU
#define JOY_PLUS   115
#define JOY_MINUS  114

#define JOY_L1     308
#define JOY_R1     309
#define JOY_L2     314
#define JOY_R2     315
#define JOY_L3     313
#define JOY_R3     316

#define JOY_UP     17
#define JOY_DOWN   17
#define JOY_LEFT   16
#define JOY_RIGHT  16

#define JOY_LUP    3
#define JOY_LDOWN  3
#define JOY_LLEFT  2
#define JOY_LRIGHT 2

#define JOY_RUP    5
#define JOY_RDOWN  5
#define JOY_RLEFT  4
#define JOY_RRIGHT 4
#endif