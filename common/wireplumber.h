#pragma once

typedef struct
{
    int id;
    char description[256];
    char nick[256];
    char name[256];
} Sink;

int get_sinks(Sink** sinks, int* count);
int get_default_sink_id(int *sink_id);
int set_default_sink(int sink_id);
