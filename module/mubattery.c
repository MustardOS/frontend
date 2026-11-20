#include <signal.h>
#include <time.h>

#include <common/platform/battery.h>

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t resumed = 0;

static void handle_signal(const int sig) {
    (void) sig;
    running = 0;
}

static void handle_resume(const int sig) {
    (void) sig;
    resumed = 1;
}

int main(void) {
    const struct timespec ts = {5, 0};

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGCONT, handle_resume);

    battery_set_daemon_mode(1);
    battery_init();

    while (running) {
        if (resumed) {
            resumed = 0;
            battery_reset();
        }

        battery_update();
        nanosleep(&ts, NULL);
    }

    return 0;
}
