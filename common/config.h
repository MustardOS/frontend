#pragma once

#include "options.h"

extern struct mux_config config;

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
        int16_t NOTATION;
        char POOL[MAX_BUFFER_SIZE];
    } CLOCK;

    struct {
        char INTERFACE[MAX_BUFFER_SIZE];
        int16_t TYPE;
        char SSID[MAX_BUFFER_SIZE];
        char PASS[MAX_BUFFER_SIZE];
        int16_t SCAN;
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
        char DEFAULT_HASH[MAX_BUFFER_SIZE];

        struct {
            int16_t ALL_THEMES;
            int16_t RESOLUTION_640x480;
            int16_t RESOLUTION_720x480;
            int16_t RESOLUTION_720x720;
            int16_t RESOLUTION_1024x768;
            int16_t RESOLUTION_1280x720;
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
            int16_t REPEAT_DELAY;
            int16_t SWAP;
            int16_t THERMAL;
            int16_t FONT;
            char VOLUME[MAX_BUFFER_SIZE];
            char BRIGHTNESS[MAX_BUFFER_SIZE];
            int16_t OFFSET;
            int16_t LOCK;
            int16_t LED;
            int16_t THEME;
            int16_t RETROWAIT;
            int16_t RETROFREE;
            char USBFUNCTION[MAX_BUFFER_SIZE];
            int16_t VERBOSE;
            int16_t RUMBLE;
            int16_t USERINIT;
            int16_t DPADSWAP;
            int16_t OVERDRIVE;
            int16_t SWAPFILE;
            int16_t ZRAMFILE;
            int16_t LIDSWITCH;
            int16_t DISPSUSPEND;
        } ADVANCED;
        struct {
            int16_t HIDDEN;
            int16_t SOUND;
            int16_t CHIME;
            int16_t BGM;
            char STARTUP[MAX_BUFFER_SIZE];
            int16_t COLOUR;
            int16_t BRIGHTNESS;
            int16_t VOLUME;
            int16_t RGB;
            char LANGUAGE[MAX_BUFFER_SIZE];
            int16_t THEME_RESOLUTION;
            int16_t THEME_RESOLUTION_WIDTH;
            int16_t THEME_RESOLUTION_HEIGHT;
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
            int16_t COMPAT;
            int16_t ASYNCLOAD;
            int16_t WAIT;
            int16_t RETRY;
        } NETWORK;
        struct {
            int16_t LOW_BATTERY;
            int16_t SHUTDOWN;
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
    } SETTINGS;

    struct {
        int16_t BATTERY;
        int16_t NETWORK;
        int16_t BLUETOOTH;
        int16_t CLOCK;
        int16_t OVERLAY_IMAGE;
        int16_t OVERLAY_TRANSPARENCY;
        int16_t BOX_ART;
        int16_t BOX_ART_ALIGN;
        int16_t BOX_ART_HIDE;
        int16_t NAME;
        int16_t DASH;
        int16_t LAUNCH_SWAP;
        int16_t SHUFFLE;
        int16_t FRIENDLYFOLDER;
        int16_t THETITLEFORMAT;
        int16_t TITLEINCLUDEROOTDRIVE;
        int16_t FOLDERITEMCOUNT;
        int16_t FOLDEREMPTY;
        int16_t COUNTERFOLDER;
        int16_t COUNTERFILE;
        int16_t BACKGROUNDANIMATION;
        int16_t LAUNCHSPLASH;
        int16_t BLACKFADE;
    } VISUAL;

    struct {
        int16_t SSHD;
        int16_t SFTPGO;
        int16_t TTYD;
        int16_t SYNCTHING;
        int16_t NTP;
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
