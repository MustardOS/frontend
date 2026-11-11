#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <common/platform/stickpoll.h>

#define STICK_CODE_BASE   0x405C2000UL
#define STICK_CODE_SIZE   0x2000UL
#define STICK_DATA_BASE   0x411BD000UL
#define STICK_POLL_FLAG   0x304UL
#define STICK_POLL_ENABLE 0x2A0UL

static const struct {
    unsigned long offset;
    uint32_t value;
} stick_poll_signature[] = {
    {0x048CUL, 0xF0005FC0U},
    {0x0490UL, 0xB942A000U},
    {0x11B4UL, 0x394092C0U},
    {0x11B8UL, 0x340007E0U},
};

static int code_matches(const int fd) {
    void *code = mmap(NULL, STICK_CODE_SIZE, PROT_READ, MAP_SHARED, fd, (off_t) STICK_CODE_BASE);
    if (code == MAP_FAILED) return 0;

    int matches = 1;
    for (size_t i = 0; i < sizeof(stick_poll_signature) / sizeof(stick_poll_signature[0]); i++) {
        const volatile uint32_t *word =
            (const volatile uint32_t *) ((const uint8_t *) code + stick_poll_signature[i].offset);
        if (*word != stick_poll_signature[i].value) matches = 0;
    }

    munmap(code, STICK_CODE_SIZE);
    return matches;
}

static int stick_poll_access(const int write, const int on) {
    const int fd = open("/dev/mem", (write ? O_RDWR : O_RDONLY) | O_CLOEXEC);
    if (fd < 0) return stick_poll_unsupported;

    int state = stick_poll_unsupported;
    const long page = sysconf(_SC_PAGESIZE);
    if (page > 0 && code_matches(fd)) {
        void *data = mmap(
            NULL, (size_t) page, write ? PROT_READ | PROT_WRITE : PROT_READ, MAP_SHARED, fd, (off_t) STICK_DATA_BASE
        );
        if (data != MAP_FAILED) {
            const volatile uint32_t *enabled = (const volatile uint32_t *) ((uint8_t *) data + STICK_POLL_ENABLE);
            volatile uint8_t *flag = (volatile uint8_t *) data + STICK_POLL_FLAG;

            if (*enabled == 1 && *flag <= 1) {
                if (write) *flag = on ? 1 : 0;
                state = *flag ? stick_poll_on : stick_poll_off;
            }

            munmap(data, (size_t) page);
        }
    }

    close(fd);
    return state;
}

int stick_poll_get(void) {
    return stick_poll_access(0, 0);
}

int stick_poll_set(const int on) {
    return stick_poll_access(1, on);
}
