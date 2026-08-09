#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "address.h"

int netplay_parse_address(const char *specification, char *address, const size_t address_size, uint16_t *port) {
    if (!specification || !specification[0] || !address || address_size < 2 || !port) return -1;
    *port = NETPLAY_DEFAULT_PORT;

    if (specification[0] == '[') {
        const char *closing = strchr(specification, ']');
        if (!closing || closing == specification + 1 || (closing[1] && closing[1] != ':')) return -1;
        if ((size_t) (closing - specification) > address_size) return -1;
        snprintf(address, address_size, "%.*s", (int) (closing - specification - 1), specification + 1);
        if (closing[1] == ':') {
            char *end = NULL;
            const unsigned long parsed = strtoul(closing + 2, &end, 10);
            if (!end || *end || !parsed || parsed > UINT16_MAX) return -1;
            *port = (uint16_t) parsed;
        }
    } else {
        const char *first = strchr(specification, ':');
        const char *last = strrchr(specification, ':');
        if (first && first == last) {
            if (first == specification || (size_t) (first - specification) >= address_size) return -1;
            snprintf(address, address_size, "%.*s", (int) (first - specification), specification);
            char *end = NULL;
            const unsigned long parsed = strtoul(first + 1, &end, 10);
            if (!end || *end || !parsed || parsed > UINT16_MAX) return -1;
            *port = (uint16_t) parsed;
        } else {
            if (strlen(specification) >= address_size) return -1;
            snprintf(address, address_size, "%s", specification);
        }
    }
    return address[0] ? 0 : -1;
}
