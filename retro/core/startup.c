#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../netplay/address.h"
#include "startup.h"

void startup_options_print_usage(FILE *stream, const char *program) {
    fprintf(
        stream,
        "Usage: %s <core.so> <content> [--fresh] [--restart] [--netplay-host[=port]] "
        "[--netplay-join=address[:port]]\n",
        program
    );
}

int startup_options_parse(const int argc, char *argv[], startup_options *options) {
    if (!options) return 0;
    memset(options, 0, sizeof(*options));
    options->netplay_port = NETPLAY_DEFAULT_PORT;
    if (argc < 3 || !argv) return 0;

    options->core_path = argv[1];
    options->content_path = argv[2];
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--fresh") == 0) {
            options->fresh = 1;
        } else if (strcmp(argv[i], "--restart") == 0) {
            options->fresh = 1;
            options->restarting = 1;
        } else if (strcmp(argv[i], "--netplay-host") == 0) {
            options->netplay_host = 1;
        } else if (strncmp(argv[i], "--netplay-host=", 15) == 0) {
            options->netplay_host = 1;
            char *end = NULL;
            const unsigned long port = strtoul(argv[i] + 15, &end, 10);
            if (end && !*end && port > 0 && port <= UINT16_MAX)
                options->netplay_port = (uint16_t) port;
            else
                options->netplay_invalid = 1;
        } else if (strncmp(argv[i], "--netplay-join=", 15) == 0) {
            if (netplay_parse_address(
                    argv[i] + 15, options->netplay_address, sizeof(options->netplay_address), &options->netplay_port
                )
                != 0) {
                options->netplay_address[0] = '\0';
                options->netplay_invalid = 1;
            }
        } else {
            options->unknown_option = argv[i];
            return 0;
        }
    }

    if (options->netplay_host && options->netplay_address[0]) options->netplay_invalid = 1;
    return 1;
}
