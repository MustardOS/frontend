#pragma once

extern struct mux_config config;

struct mux_config {
    struct {
        int16_t FACTORY_RESET;
    } BOOT;

    struct {
        int16_t NOTATION;
        const char *POOL;
    } CLOCK;

    struct {
        int16_t ENABLED;
        const char *INTERFACE;
        int16_t TYPE;
        const char *SSID;
        const char *ADDRESS;
        const char *GATEWAY;
        const char *SUBNET;
        const char *DNS;
    } NETWORK;

    struct {
        struct {
            int16_t HIDDEN;
            int16_t SOUND;
            const char *STARTUP;
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
        const char *NAME;
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
