#include <stdio.h>
#include <stdlib.h>

#include <common/config/config.h>
#include <common/content/content.h>
#include <common/platform/device.h>
#include <common/runtime/init.h>
#include <module/muxshare.h>

int main(const int argc, char **argv) {
    if (argc != 2 || !argv[1][0]) {
        fprintf(stderr, "Usage: %s CONTENT\n", argv[0]);
        return EXIT_FAILURE;
    }

    load_device(&device);
    load_config(&config);
    init_module("muswitch");
    return load_content(0, argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
