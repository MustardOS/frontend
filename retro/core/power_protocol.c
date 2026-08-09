#include <signal.h>
#include <string.h>
#include "power_protocol.h"

static volatile sig_atomic_t pending_sleep = 0;
static volatile sig_atomic_t pending_wake = 0;
static volatile sig_atomic_t pending_exit = 0;

static void receive_sleep(const int signal_number) {
    (void) signal_number;
    pending_sleep = 1;
}

static void receive_wake(const int signal_number) {
    (void) signal_number;
    pending_wake = 1;
}

static void receive_exit(const int signal_number) {
    pending_exit = signal_number;
}

int power_protocol_init(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);

    action.sa_handler = receive_sleep;
    if (sigaction(SIGUSR1, &action, NULL) != 0) return -1;

    action.sa_handler = receive_wake;
    if (sigaction(SIGUSR2, &action, NULL) != 0) return -1;

    action.sa_handler = receive_exit;
    if (sigaction(SIGTERM, &action, NULL) != 0) return -1;
    if (sigaction(SIGINT, &action, NULL) != 0) return -1;

    pending_sleep = 0;
    pending_wake = 0;
    pending_exit = 0;
    return 0;
}

void power_protocol_take(power_protocol_events *events) {
    if (!events) return;

    sigset_t blocked;
    sigset_t previous;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    sigaddset(&blocked, SIGUSR2);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    sigprocmask(SIG_BLOCK, &blocked, &previous);

    events->sleep = pending_sleep != 0;
    events->wake = pending_wake != 0;
    events->exit_signal = (int) pending_exit;
    pending_sleep = 0;
    pending_wake = 0;
    pending_exit = 0;

    sigprocmask(SIG_SETMASK, &previous, NULL);
}
