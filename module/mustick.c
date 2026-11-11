#include <stdio.h>
#include <string.h>
#include <common/platform/stickpoll.h>

static int usage(void) {
    fprintf(stderr, "Usage: mustick on|off|status\n");
    return 2;
}

int main(const int argc, char **argv) {
    if (argc != 2) return usage();

    int state;
    if (strcmp(argv[1], "on") == 0) {
        state = stick_poll_set(1);
    } else if (strcmp(argv[1], "off") == 0) {
        state = stick_poll_set(0);
    } else if (strcmp(argv[1], "status") == 0) {
        state = stick_poll_get();
    } else {
        return usage();
    }

    puts(state == stick_poll_on ? "on" : state == stick_poll_off ? "off" : "unsupported");
    return state == stick_poll_unsupported ? 1 : 0;
}
