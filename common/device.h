#pragma once

#include "options.h"

extern struct mux_device device;

struct mux_device {
    struct {
        char NAME[MAX_BUFFER_SIZE];
        int16_t HAS_NETWORK;
        int16_t HAS_BLUETOOTH;
        int16_t HAS_PORTMASTER;
        int16_t HAS_LID;
        int16_t HAS_HDMI;
        int16_t EVENT;
        int16_t DEBUGFS;
        char RTC[MAX_BUFFER_SIZE];
        char LED[MAX_BUFFER_SIZE];
    } DEVICE;

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
        int16_t WAIT;
        int16_t WIDTH;
        int16_t HEIGHT;
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
    } BATTERY;

    struct {
        char EV0[MAX_BUFFER_SIZE];
        char EV1[MAX_BUFFER_SIZE];
        int32_t AXIS;
    } INPUT;

    struct {
        struct {
            int16_t UP;
            int16_t DOWN;
            int16_t LEFT;
            int16_t RIGHT;
        } DPAD;
        struct {
            struct {
                int16_t UP;
                int16_t DOWN;
                int16_t LEFT;
                int16_t RIGHT;
                int16_t CLICK;
            } LEFT;
            struct {
                int16_t UP;
                int16_t DOWN;
                int16_t LEFT;
                int16_t RIGHT;
                int16_t CLICK;
            } RIGHT;
        } ANALOG;
        struct {
            int16_t A;
            int16_t B;
            int16_t C;
            int16_t X;
            int16_t Y;
            int16_t Z;
            int16_t L1;
            int16_t L2;
            int16_t L3;
            int16_t R1;
            int16_t R2;
            int16_t R3;
            int16_t MENU_SHORT;
            int16_t MENU_LONG;
            int16_t SELECT;
            int16_t START;
            int16_t POWER_SHORT;
            int16_t POWER_LONG;
            int16_t VOLUME_UP;
            int16_t VOLUME_DOWN;
        } BUTTON;
    } RAW_INPUT;

};

void load_device(struct mux_device *device);
