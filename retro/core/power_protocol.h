#pragma once

typedef struct {
    int sleep;
    int wake;
    int exit_signal;
} power_protocol_events;

int power_protocol_init(void);
void power_protocol_take(power_protocol_events *events);
