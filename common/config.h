#pragma once

#include "options.h"

extern struct mux_config config;

struct mux_config {
    struct {
        int16_t FACTORY_RESET;
    } BOOT;

    struct {
        int16_t NOTATION;
        char POOL[MAX_BUFFER_SIZE];
    } CLOCK;

    struct {
        int16_t ENABLED;
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
            int16_t ACCELERATE;
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
            char USBFUNCTION[MAX_BUFFER_SIZE];
            char STATE[MAX_BUFFER_SIZE];
            int16_t VERBOSE;
            int16_t RUMBLE;
            int16_t USERINIT;
            int16_t DPADSWAP;
            int16_t OVERDRIVE;
        } ADVANCED;
        struct {
            int16_t HIDDEN;
            int16_t SOUND;
            int16_t BGM;
            char STARTUP[MAX_BUFFER_SIZE];
            int16_t COLOUR;
            int16_t BRIGHTNESS;
            char LANGUAGE[MAX_BUFFER_SIZE];
        } GENERAL;
        struct {
            int16_t ENABLED;
            int16_t RESOLUTION;
            int16_t SPACE;
            int16_t DEPTH;
            int16_t RANGE;
            int16_t SCAN;
            int16_t AUDIO;
        } HDMI;
        struct {
            int16_t LOW_BATTERY;
            int16_t SHUTDOWN;
            int16_t IDLE_DISPLAY;
            int16_t IDLE_SLEEP;
        } POWER;
    } SETTINGS;

    struct {
        int16_t BATTERY;
        int16_t NETWORK;
        int16_t BLUETOOTH;
        int16_t CLOCK;
        int16_t BOX_ART;
        int16_t BOX_ART_ALIGN;
        int16_t NAME;
        int16_t DASH;
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
        int16_t RSLSYNC;
        int16_t NTP;
        int16_t TAILSCALED;
    } WEB;
};

void load_config(struct mux_config *config);
