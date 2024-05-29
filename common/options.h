#pragma once

#define TEST_IMAGE 1

#define MAX_BUFFER_SIZE 512

#define ITEM_SKIP 13
#define DUMMY_ROM "ZZZZZZZZ.ZZZ"
#define DUMMY_DIR "00000DIR"

#define TIME_STRING_12 "%I:%M %p"
#define TIME_STRING_24 "%H:%M"

#define INTERNAL_PATH "opt/muos"

#define RA_CONFIG_FILE "/mnt/mmc/MUOS/.retroarch/retroarch.cfg"
#define RA_CONFIG_CRC "59749d83"

#define MUOS_ACT_LOAD "/tmp/act_go"
#define MUOS_ASS_LOAD "/tmp/ass_go"
#define MUOS_SAA_LOAD "/tmp/saa_go"
#define MUOS_ROM_LOAD "/tmp/rom_go"
#define MUOS_SYS_LOAD "/tmp/sys_go"

#define BL_RST_FILE    "/opt/muos/config/brightness.txt"
#define VOL_RST_FILE   "/opt/muos/config/volume.txt"

#if defined(RG35XX)
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