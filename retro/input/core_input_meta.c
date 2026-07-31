#include <stdio.h>
#include <string.h>
#include "../../common/input.h"
#include "core_input_meta.h"

#define CORE_INPUT_META_PORT_COUNT      (1 + MUX_INPUT_MAX_EXTRA_PLAYERS)
#define CORE_INPUT_META_MAX_TYPES       16
#define CORE_INPUT_META_MAX_DESCRIPTORS 256

typedef struct {
    char desc[64];
    unsigned id;
} core_input_device_type;

typedef struct {
    core_input_device_type types[CORE_INPUT_META_MAX_TYPES];
    int type_count;
} core_input_port_info;

static core_input_port_info port_info[CORE_INPUT_META_PORT_COUNT];

typedef struct {
    unsigned port;
    unsigned device;
    unsigned index;
    unsigned id;
    char description[128];
} core_input_descriptor_entry;

static core_input_descriptor_entry descriptors[CORE_INPUT_META_MAX_DESCRIPTORS];
static int descriptor_count = 0;

static const char *const joypad_fallback_labels[16] = {
    "B", "Y", "Select", "Start", "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
    "A", "X", "L1",     "R1",    "L2",       "R2",         "L3",         "R3",
};

void core_input_meta_set_controller_info(const struct retro_controller_info *info) {
    for (int port = 0; port < CORE_INPUT_META_PORT_COUNT; port++)
        port_info[port].type_count = 0;

    if (!info) return;

    for (int port = 0; port < CORE_INPUT_META_PORT_COUNT && info[port].types; port++) {
        unsigned count = info[port].num_types;
        if (count > CORE_INPUT_META_MAX_TYPES) count = CORE_INPUT_META_MAX_TYPES;

        for (unsigned t = 0; t < count; t++) {
            snprintf(
                port_info[port].types[t].desc, sizeof(port_info[port].types[t].desc), "%s",
                info[port].types[t].desc ? info[port].types[t].desc : ""
            );
            port_info[port].types[t].id = info[port].types[t].id;
        }

        port_info[port].type_count = (int) count;
    }
}

void core_input_meta_set_input_descriptors(const struct retro_input_descriptor *desc) {
    descriptor_count = 0;
    if (!desc) return;

    int i;
    for (i = 0; i < CORE_INPUT_META_MAX_DESCRIPTORS && desc[i].description; i++) {
        descriptors[i].port = desc[i].port;
        descriptors[i].device = desc[i].device;
        descriptors[i].index = desc[i].index;
        descriptors[i].id = desc[i].id;
        snprintf(descriptors[i].description, sizeof(descriptors[i].description), "%s", desc[i].description);
    }
    descriptor_count = i;
}

void core_input_meta_clear(void) {
    for (int port = 0; port < CORE_INPUT_META_PORT_COUNT; port++)
        port_info[port].type_count = 0;
    descriptor_count = 0;
}

int core_input_meta_port_type_count(const int port) {
    if (port < 0 || port >= CORE_INPUT_META_PORT_COUNT) return 0;
    return port_info[port].type_count;
}

int core_input_meta_port_type_get(
    const int port, const int index, char *desc_out, const size_t desc_len, unsigned *id_out
) {
    if (port < 0 || port >= CORE_INPUT_META_PORT_COUNT) return 0;
    if (index < 0 || index >= port_info[port].type_count) return 0;

    if (desc_out) snprintf(desc_out, desc_len, "%s", port_info[port].types[index].desc);
    if (id_out) *id_out = port_info[port].types[index].id;
    return 1;
}

unsigned core_input_meta_preferred_device(const int port) {
    if (port < 0 || port >= CORE_INPUT_META_PORT_COUNT) return 0;

    for (int i = 0; i < port_info[port].type_count; i++) {
        const core_input_device_type *type = &port_info[port].types[i];
        if ((type->id & RETRO_DEVICE_MASK) != RETRO_DEVICE_ANALOG) continue;
        if (strcasestr(type->desc, "dualshock") || strcasestr(type->desc, "dual shock")) return type->id;
    }

    return 0;
}

const char *core_input_meta_label(const unsigned port, const unsigned device, const unsigned index, const unsigned id) {
    for (int i = 0; i < descriptor_count; i++) {
        if (descriptors[i].port == port && descriptors[i].device == device && descriptors[i].index == index
            && descriptors[i].id == id) {
            return descriptors[i].description;
        }
    }

    if (device == RETRO_DEVICE_JOYPAD && index == 0 && id < 16) return joypad_fallback_labels[id];

    return NULL;
}
