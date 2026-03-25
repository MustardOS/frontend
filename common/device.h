#pragma once

#include "options.h"

extern struct mux_device device;

struct mux_device {
    struct {
        char NAME[MAX_BUFFER_SIZE];
        int16_t HASNETWORK;
        int16_t HASBLUETOOTH;
        int16_t HASPORTMASTER;
        int16_t HASLID;
        int16_t HASHDMI;
        int16_t HASEVENT;
        int16_t HASDEBUGFS;
        int16_t HASRGB;
        int16_t HASSTICK;
        char SDL_MAP[MAX_BUFFER_SIZE];
        char JOY_HALL[MAX_BUFFER_SIZE];
        char LED[MAX_BUFFER_SIZE];
        char RTC_CLOCK[MAX_BUFFER_SIZE];
        char RTC_WAKE[MAX_BUFFER_SIZE];
    } BOARD;

    struct {
        int16_t MIN;
        int16_t MAX;
    } AUDIO;

    struct {
        int16_t WIDTH;
        int16_t HEIGHT;
    } MUX;

    struct {
        struct {
            char DEVICE[MAX_BUFFER_SIZE];
            char SEPARATOR[MAX_BUFFER_SIZE];
            int16_t PARTITION;
            char MOUNT[MAX_BUFFER_SIZE];
            char TYPE[MAX_BUFFER_SIZE];
            char LABEL[MAX_BUFFER_SIZE];
        } BOOT;
        struct {
            char DEVICE[MAX_BUFFER_SIZE];
            char SEPARATOR[MAX_BUFFER_SIZE];
            int16_t PARTITION;
            char MOUNT[MAX_BUFFER_SIZE];
            char TYPE[MAX_BUFFER_SIZE];
            char LABEL[MAX_BUFFER_SIZE];
        } ROM;
        struct {
            char DEVICE[MAX_BUFFER_SIZE];
            char SEPARATOR[MAX_BUFFER_SIZE];
            int16_t PARTITION;
            char MOUNT[MAX_BUFFER_SIZE];
            char TYPE[MAX_BUFFER_SIZE];
            char LABEL[MAX_BUFFER_SIZE];
        } ROOT;
        struct {
            char DEVICE[MAX_BUFFER_SIZE];
            char SEPARATOR[MAX_BUFFER_SIZE];
            int16_t PARTITION;
            char MOUNT[MAX_BUFFER_SIZE];
            char TYPE[MAX_BUFFER_SIZE];
            char LABEL[MAX_BUFFER_SIZE];
        } SDCARD;
        struct {
            char DEVICE[MAX_BUFFER_SIZE];
            char SEPARATOR[MAX_BUFFER_SIZE];
            int16_t PARTITION;
            char MOUNT[MAX_BUFFER_SIZE];
            char TYPE[MAX_BUFFER_SIZE];
            char LABEL[MAX_BUFFER_SIZE];
        } USB;
    } STORAGE;

    struct {
        char DEFAULT[MAX_BUFFER_SIZE];
        char AVAILABLE[MAX_BUFFER_SIZE];
        char GOVERNOR[MAX_BUFFER_SIZE];
        char SCALER[MAX_BUFFER_SIZE];
    } CPU;

    struct {
        char MODULE[MAX_BUFFER_SIZE];
        char NAME[MAX_BUFFER_SIZE];
        char TYPE[MAX_BUFFER_SIZE];
        char INTERFACE[MAX_BUFFER_SIZE];
        char STATE[MAX_BUFFER_SIZE];
    } NETWORK;

    struct {
        char DEVICE[MAX_BUFFER_SIZE];
        char HDMI[MAX_BUFFER_SIZE];
        int16_t BRIGHT;
        int16_t ROTATE;
        int16_t ROTATE_PIVOT_X;
        int16_t ROTATE_PIVOT_Y;
        int16_t RENDER_OFFSET_X;
        int16_t RENDER_OFFSET_Y;
        int16_t WAIT;
        int16_t WIDTH;
        int16_t HEIGHT;
        float ZOOM;
        float ZOOM_WIDTH;
        float ZOOM_HEIGHT;
        struct {
            int16_t WIDTH;
            int16_t HEIGHT;
        } INTERNAL;
        struct {
            int16_t WIDTH;
            int16_t HEIGHT;
        } EXTERNAL;
    } SCREEN;

    struct {
        int16_t SCALER;
        int16_t ROTATE;
    } SDL;

    struct {
        char CAPACITY[MAX_BUFFER_SIZE];
        char HEALTH[MAX_BUFFER_SIZE];
        char VOLTAGE[MAX_BUFFER_SIZE];
        char CHARGER[MAX_BUFFER_SIZE];
        int16_t VOLT_MIN;
        int16_t VOLT_MAX;
        int16_t SIZE;
        struct {
            char CHARGE[MAX_BUFFER_SIZE];
            char DISCHARGE[MAX_BUFFER_SIZE];
        } CURVE;
    } BATTERY;
};

void load_device(struct mux_device *device);
