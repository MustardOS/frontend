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
        char ADDRESS[MAX_BUFFER_SIZE];
        char GATEWAY[MAX_BUFFER_SIZE];
        char SUBNET[MAX_BUFFER_SIZE];
        char DNS[MAX_BUFFER_SIZE];
    } NETWORK;

    struct {
        struct {
            int16_t HIDDEN;
            int16_t SOUND;
            int16_t BGM;
            char STARTUP[MAX_BUFFER_SIZE];
            int16_t POWER;
            int16_t LOW_BATTERY;
            int16_t COLOUR;
            int16_t BRIGHTNESS;
            int16_t HDMI;
            int16_t SHUTDOWN;
            char LANGUAGE[MAX_BUFFER_SIZE];
        } GENERAL;
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
            int16_t HDMIOUTPUT;
        } ADVANCED;
    } SETTINGS;

    struct {
        int16_t BATTERY;
        int16_t NETWORK;
        int16_t BLUETOOTH;
        int16_t CLOCK;
        int16_t BOX_ART;
        int16_t NAME;
        int16_t DASH;
        int16_t FRIENDLYFOLDER;
        int16_t THETITLEFORMAT;
        int16_t FOLDERITEMCOUNT;
        int16_t COUNTERFOLDER;
        int16_t COUNTERFILE;
        int16_t BACKGROUNDANIMATION;
    } VISUAL;

    struct {
        int16_t SHELL;
        int16_t BROWSER;
        int16_t TERMINAL;
        int16_t SYNCTHING;
        int16_t RESILIO;
        int16_t NTP;
    } WEB;

    struct {
        int16_t BIOS;
        int16_t CONFIG;
        int16_t CATALOGUE;
        int16_t CONTENT;
        int16_t MUSIC;
        int16_t SAVE;
        int16_t SCREENSHOT;
        int16_t THEME;
        int16_t LANGUAGE;
        int16_t NETWORK;
    } STORAGE;
};

void load_config(struct mux_config *config);
