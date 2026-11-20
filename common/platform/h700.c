#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <common/platform/h700.h>

#define H700_COMPATIBLE "arm,sun50iw9p1"
#define COMPATIBLE_PATH "/proc/device-tree/compatible"

int platform_is_h700(void) {
    static int supported = -1;
    if (supported >= 0) return supported;

    supported = 0;
    const int fd = open(COMPATIBLE_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return supported;

    char compatible[1024];
    const ssize_t length = read(fd, compatible, sizeof(compatible));
    close(fd);
    if (length <= 0) return supported;

    size_t offset = 0;
    while (offset < (size_t) length) {
        const size_t remaining = (size_t) length - offset;
        const size_t entry_length = strnlen(compatible + offset, remaining);
        if (entry_length == sizeof(H700_COMPATIBLE) - 1
            && memcmp(compatible + offset, H700_COMPATIBLE, sizeof(H700_COMPATIBLE) - 1) == 0) {
            supported = 1;
            break;
        }
        offset += entry_length + 1;
    }

    return supported;
}
