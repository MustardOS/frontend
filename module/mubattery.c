#include <signal.h>
#include <time.h>

#include "../common/battery.h"

static volatile int running = 1;

static void handle_signal(int sig) {
    (void) sig;
    running = 0;
}

int main(void) {
    struct timespec ts = {3, 0};

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    battery_set_daemon_mode(1);
    battery_init();

    while (running) {
        battery_update();
        nanosleep(&ts, NULL);
    }

    return 0;
}
