#include <unistd.h>
#include <stdio.h>
#include <sys/reboot.h>

int main() {
    sync();
    sleep(1);

    if (reboot(RB_POWER_OFF) < 0) {
        perror("Shutdown failure");
        return 1;
    }

    return 0;
}
