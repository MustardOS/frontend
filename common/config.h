#pragma once

#include <stddef.h>
#include <stdint.h>
#include "options.h"

extern struct mux_config config;

#define CFG_DIR_MAX   64
#define CFG_NAME_MAX  64
#define CFG_VALUE_MAX 1024

typedef struct {
    char name[CFG_NAME_MAX];
    char value[CFG_VALUE_MAX];
} cfg_kv_t;

typedef struct {
    cfg_kv_t entries[CFG_DIR_MAX];
    int count;
} cfg_dir_t;

void cfg_dir_scan(cfg_dir_t *d, const char *dir_path);

const char *cfg_dir_get(const cfg_dir_t *d, const char *name);

int cfg_dir_int(const cfg_dir_t *d, const char *name, int fallback);

double cfg_dir_flo(const cfg_dir_t *d, const char *name, double fallback);

struct mux_config {
    struct {
        char BUILD[MAX_BUFFER_SIZE];
        char VERSION[MAX_BUFFER_SIZE];
    } SYSTEM;

    struct {
        int16_t FACTORY_RESET;
        int16_t DEVICE_MODE;
    } BOOT;

    struct {
        int16_t APPS;
        int16_t BIOS;
        int16_t CATALOGUE;
        int16_t CHEATS;
        int16_t COLLECTION;
        int16_t CONFIG;
        int16_t CONTENT;
        int16_t HISTORY;
        int16_t INIT;
        int16_t MERGE;
        int16_t MUSIC;
        int16_t NAME;
        int16_t NETWORK;
        int16_t OVERLAYS;
        int16_t OVERRIDE;
        int16_t PACKAGE;
        int16_t SAVE;
        int16_t SCREENSHOT;
        int16_t SHADERS;
        int16_t SYNCTHING;
        int16_t THEME;
        int16_t TRACK;
        int16_t START;
        int16_t TARGET;
    } BACKUP;

    struct {
        int16_t NOTATION;
        char POOL[MAX_BUFFER_SIZE];
        char CUSTOM[MAX_BUFFER_SIZE];
    } CLOCK;

    struct {
        char INTERFACE[MAX_BUFFER_SIZE];
        int16_t TYPE;
        char SSID[MAX_BUFFER_SIZE];
        char PASS[MAX_BUFFER_SIZE];
        char ADDRESS[MAX_BUFFER_SIZE];
        char GATEWAY[MAX_BUFFER_SIZE];
        char SUBNET[MAX_BUFFER_SIZE];
        char DNS[MAX_BUFFER_SIZE];
    } NETWORK;

    struct {
        struct {
            char DATA[MAX_BUFFER_SIZE];
        } DOWNLOAD;
        struct {
            char DATA[MAX_BUFFER_SIZE];
        } LANGUAGE;
    } EXTRA;

    struct {
        char ACTIVE[MAX_BUFFER_SIZE];
        char STORAGE_THEME[MAX_BUFFER_SIZE];
        char THEME_CAT_PATH[MAX_BUFFER_SIZE];

        struct {
            int16_t ALLTHEMES;
            int16_t RESOLUTION_640x480;
            int16_t RESOLUTION_720x480;
            int16_t RESOLUTION_720x720;
            int16_t RESOLUTION_1024x768;
            int16_t RESOLUTION_1280x720;
            int16_t RESOLUTION_1920x1080;
            int16_t GRID;
            int16_t HDMI;
            int16_t LANGUAGE;
            char LOOKUP[MAX_BUFFER_SIZE];
        } FILTER;

        struct {
            char DATA[MAX_BUFFER_SIZE];
            char PREVIEW[MAX_BUFFER_SIZE];
        } DOWNLOAD;
    } THEME;

    struct {
        struct {
            int16_t ACCELERATE;
            int16_t REPEATDELAY;
            int16_t STICKNAV;
            int16_t THERMAL;
            int16_t FONT;
            int16_t VOLUME;
            int16_t BRIGHTNESS;
            int16_t LED;
            int16_t RANDOMTHEME;
            int16_t RETROWAIT;
            int16_t RETROFREE;
            int16_t RETROCACHE;
            int16_t ACTIVITY;
            int16_t USBFUNCTION;
            int16_t VERBOSE;
            int16_t DEBUGLOG;
            int16_t RUMBLE;
            int16_t USERINIT;
            int16_t DPADSWAP;
            int16_t OVERDRIVE;
            int16_t SWAPFILE;
            int16_t ZRAMFILE;
            int16_t LIDSWITCH;
            int16_t DISPSUSPEND;
            int16_t STAGEOVERLAY;
            int16_t INCBRIGHT;
            int16_t INCVOLUME;
            int16_t MAXGPU;
            int16_t AUDIOREADY;
            int16_t AUDIOSWAP;
            int16_t AUDIOSUSPEND;
            int16_t BTSCANTIMEOUT;
            int16_t SECONDPART;
            int16_t USBPART;
            int16_t TRUSTMODIFY;
            int16_t TRUSTPOWER;
            int16_t TRUSTREMOVE;
        } ADVANCED;
        struct {
            int16_t SCHEDULE_MODE;
            int16_t SUNRISE_TEMP;
            int16_t SUNSET_TEMP;
            int16_t SUNRISE_TIME;
            int16_t SUNSET_TIME;
        } COLOUR;
        struct {
            int16_t SOUND;
            int16_t SOUNDVOL;
            int16_t CHIME;
            int16_t BGM;
            int16_t BGMVOL;
            char STARTUP[MAX_BUFFER_SIZE];
            int16_t BRIGHTNESS;
            int16_t VOLUME;
            int16_t RGB;
            char LANGUAGE[MAX_BUFFER_SIZE];
            int16_t THEME_SCALING;
            int16_t THEME_RESOLUTION;
            int16_t THEME_RESOLUTION_WIDTH;
            int16_t THEME_RESOLUTION_HEIGHT;
            int16_t HKDPAD;
            int16_t HKSHOT;
            int16_t AUDIOSINK;
        } GENERAL;
        struct {
            int16_t RESOLUTION;
            int16_t SPACE;
            int16_t DEPTH;
            int16_t RANGE;
            int16_t SCAN;
            int16_t AUDIO;
        } HDMI;
        struct {
            int16_t MONITOR;
            int16_t BOOT;
            int16_t WAKE;
            int16_t COMPAT;
            int16_t ASYNCLOAD;
            int16_t CONRETRY;
            int16_t WAIT;
            int16_t MODRETRY;
            int16_t PROXY_ENABLED;
            int16_t PROXY_TYPE;
            char PROXY_SERVER[MAX_BUFFER_SIZE];
            char PROXY_NOPROXY[MAX_BUFFER_SIZE];
        } NETWORK;
        struct {
            int16_t GENALPHA;
            int16_t GENANCHOR;
            int16_t GENSCALE;
            int16_t BATALPHA;
            int16_t BATANCHOR;
            int16_t BATSCALE;
            int16_t VOLALPHA;
            int16_t VOLANCHOR;
            int16_t VOLSCALE;
            int16_t BRIALPHA;
            int16_t BRIANCHOR;
            int16_t BRISCALE;
        } OVERLAY;
        struct {
            int16_t LOW_BATTERY;
            int16_t SHUTDOWN;
            int16_t SAVERTYPE;
            int16_t SAVERSPEED;
            struct {
                int16_t DISPLAY;
                int16_t SLEEP;
                int16_t MUTE;
            } IDLE;
            struct {
                char DEFAULT[MAX_BUFFER_SIZE];
                char IDLE[MAX_BUFFER_SIZE];
            } GOV;
        } POWER;
        struct {
            int16_t MODE;
            int16_t BRIGHT;
            int16_t BREATH_SPEED;
            int16_t COLOURL;
            int16_t COLOURR;
            int16_t COLOURM;
            int16_t COLOURF1;
            int16_t COLOURF2;
            int16_t COMBO;
            int16_t BACKEND;
        } RGB;
        struct {
            char NAME[MAX_BUFFER_SIZE];
            int16_t LIST_SIZE;
            int16_t HEADER_SIZE;
            int16_t FOOTER_SIZE;
            int16_t PANEL_SIZE;
        } FONT;
        struct {
            int16_t HEADER_HEIGHT;
            int16_t FOOTER_HEIGHT;
            int16_t CONTENT_ITEM_COUNT;
            int16_t GLYPH_SIZE_LIST;
            int16_t GLYPH_SIZE_FOOTER;
            int16_t GLYPH_SIZE_HEADER;
            int16_t GLYPH_SIZE_GRID;
            int16_t LABEL_WIDTH;
        } THEMEOPT;
        struct {
            int16_t LAYOUT;
        } REMAP;
    } SETTINGS;

    struct {
        int16_t DEFAULT;
        int16_t COLLECTION;
        int16_t HISTORY;
    } SORT;

    struct {
        int16_t SORT;
        int16_t BATTERY;
        int16_t NETWORK;
        int16_t HEADERTITLE;
        int16_t DIALOGUETRANSITION;
        int16_t BLUETOOTH;
        int16_t CLOCK;
        int16_t OVERLAYIMAGE;
        int16_t OVERLAYTRANSPARENCY;
        int16_t GRID_MODE_CONTENT;
        int16_t BOX_ART;
        int16_t BOX_ART_ALIGN;
        int16_t BOX_ART_HIDE;
        int16_t BOX_ART_SCALE;
        int16_t BOX_ART_TRANSITION;
        int16_t VIDEO_PREVIEW;
        int16_t CONTENT_WIDTH;
        int16_t NAME;
        int16_t DASH;
        int16_t LAUNCH_SWAP;
        int16_t SHUFFLE;
        int16_t HIDDEN;
        int16_t FRIENDLYFOLDER;
        int16_t THETITLEFORMAT;
        int16_t TITLEINCLUDEROOTDRIVE;
        int16_t FOLDERITEMCOUNT;
        int16_t DISPLAYEMPTYFOLDER;
        int16_t MENUCOUNTERFOLDER;
        int16_t MENUCOUNTERFILE;
        int16_t VIDEO_WALLPAPER;
        int16_t BACKGROUND_SCALE;
        int16_t LAUNCHSPLASH;
        int16_t BLACKFADE;
        int16_t CONTENTCOLLECT;
        int16_t CONTENTHISTORY;
        int16_t MIXEDCONTENT;
        int16_t FORWARDHISTORY;
        int16_t NAMESCROLL;
        int16_t LABELSCROLLSPEED;
        int16_t LISTGLYPH;
        int16_t SELECTIONANIMATION;
        int16_t SELECTIONSTYLE;
        int16_t RENDERSHADOWS;
    } VISUAL;

    struct {
        int16_t AUTOCONNECT;
    } BLUETOOTH;

    struct {
        int16_t SSHD;
        int16_t SFTPGO;
        int16_t TTYD;
        int16_t SYNCTHING;
        int16_t TAILSCALED;
    } WEB;

    struct {
        int16_t VMSWAP;
        int16_t DIRTYRATIO;
        int16_t DIRTYBACK;
        int16_t CACHE;
        int16_t MERGE;
        int16_t REQUESTS;
        int16_t IOSTATS;
        int16_t READAHEAD;
        int16_t IDLEFLUSH;
        int16_t PAGECLUSTER;
        int16_t CHILDFIRST;
        int16_t TIMESLICE;
        int16_t TUNESCALE;
        char CARDMODE[MAX_BUFFER_SIZE];
        char STATE[MAX_BUFFER_SIZE];
    } DANGER;
};

void load_config(struct mux_config *config);
