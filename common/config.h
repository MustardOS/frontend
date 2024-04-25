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
        char ADDRESS[MAX_BUFFER_SIZE];
        char GATEWAY[MAX_BUFFER_SIZE];
        char SUBNET[MAX_BUFFER_SIZE];
        char DNS[MAX_BUFFER_SIZE];
    } NETWORK;

    struct {
        struct {
            int16_t HIDDEN;
            int16_t SOUND;
            char STARTUP[MAX_BUFFER_SIZE];
            int16_t POWER;
            int16_t LOW_BATTERY;
            int16_t COLOUR;
            int16_t BRIGHTNESS;
            int16_t HDMI;
        } GENERAL;
        struct {
            int16_t SWAP;
            int16_t THERMAL;
            int16_t FONT;
            int16_t VERBOSE;
            int16_t VOLUME_LOW;
            int16_t OFFSET;
        } ADVANCED;
    } SETTINGS;

    struct {
        char NAME[MAX_BUFFER_SIZE];
    } THEME;

    struct {
        int16_t BATTERY;
        int16_t NETWORK;
        int16_t BLUETOOTH;
        int16_t CLOCK;
        int16_t BOX_ART;
    } VISUAL;

    struct {
        int16_t SHELL;
        int16_t BROWSER;
        int16_t TERMINAL;
        int16_t SYNCTHING;
        int16_t NTP;
    } WEB;
};

void load_config(struct mux_config *config);
