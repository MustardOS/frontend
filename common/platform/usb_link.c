#include "usb_link.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/if_tun.h>
#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <linux/usbdevice_fs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LINK_GADGET       "/sys/kernel/config/usb_gadget/mulink"
#define LINK_MUOS_GADGET  "/sys/kernel/config/usb_gadget/muos"
#define LINK_USB_FUNCTION "/opt/muos/config/settings/advanced/usb_function"
#define LINK_USB_SCRIPT   "/opt/muos/script/system/usb_gadget.sh"
#define LINK_SCRIPT_MS    8000
#define LINK_FUNCTION     "ffs.mulink"
#define LINK_FFS_ROOT     "/dev/usb-ffs"
#define LINK_FFS          LINK_FFS_ROOT "/mulink"
#define LINK_UDC_CLASS    "/sys/class/udc"
#define LINK_USB_DEVICES  "/sys/bus/usb/devices"
#define LINK_TUN          "/dev/net/tun"
#define LINK_VENDOR       0x1d6b
#define LINK_PRODUCT      0x0110
#define LINK_SUBCLASS     0x4d
#define LINK_PROTOCOL     0x4c
#define LINK_IFACE        "usblink0"
#define LINK_RECORD_MAX   2048
#define LINK_FRAME_MAX    1518
#define LINK_PACKET_ALIGN 64
#define LINK_POLL_MS      250
#define LINK_SCAN_MS      1000
#define LINK_RELEASE_MS   5000
#define LINK_LINGER_MS    15000

static struct {
    int notify_fd;
    volatile int running;
    volatile usb_link_state device_state;
    volatile usb_link_state host_state;
    pthread_mutex_t lock;

    int gadget_built;
    int ffs_mounted;
    int ep0;
    int ep_in;
    int ep_out;
    char udc[64];
    pthread_t control;
    int control_started;

    uint8_t mac[6];
    int tap;
    int tap_owner;
    uint64_t tap_idle_since;
    pthread_mutex_t tap_lock;

    volatile int device_enabled;
    volatile int bridge_active;
    pthread_mutex_t bridge_lock;
    pthread_t device_rx;
    pthread_t device_tx;
    int bridge_started;

    pthread_t host;
    int host_started;
} cable = {
    .notify_fd = -1,
    .ep0 = -1,
    .ep_in = -1,
    .ep_out = -1,
    .tap = -1,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .tap_lock = PTHREAD_MUTEX_INITIALIZER,
    .bridge_lock = PTHREAD_MUTEX_INITIALIZER
};

enum { tap_owner_none = 0, tap_owner_device, tap_owner_host };

static const struct {
    struct usb_functionfs_descs_head_v2 header;
    __le32 fs_count;
    __le32 hs_count;
    struct {
        struct usb_interface_descriptor interface;
        struct usb_endpoint_descriptor_no_audio in;
        struct usb_endpoint_descriptor_no_audio out;
    } __attribute__((packed)) fs, hs;
} __attribute__((packed)) link_descriptors = {
    .header =
        {
            .magic = FUNCTIONFS_DESCRIPTORS_MAGIC_V2,
            .flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC,
            .length = sizeof(link_descriptors),
        },
    .fs_count = 3,
    .hs_count = 3,
    .fs =
        {
            .interface =
                {
                    .bLength = sizeof(struct usb_interface_descriptor),
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = LINK_SUBCLASS,
                    .bInterfaceProtocol = LINK_PROTOCOL,
                    .iInterface = 1,
                },
            .in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 1 | USB_DIR_IN,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = 64,
                },
            .out =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 2 | USB_DIR_OUT,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = 64,
                },
        },
    .hs =
        {
            .interface =
                {
                    .bLength = sizeof(struct usb_interface_descriptor),
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = LINK_SUBCLASS,
                    .bInterfaceProtocol = LINK_PROTOCOL,
                    .iInterface = 1,
                },
            .in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 1 | USB_DIR_IN,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = 512,
                },
            .out =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 2 | USB_DIR_OUT,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = 512,
                },
        },
};

#define LINK_INTERFACE_NAME "MustardOS Direct Link"

static const struct {
    struct usb_functionfs_strings_head header;
    struct {
        __le16 code;
        const char name[sizeof(LINK_INTERFACE_NAME)];
    } __attribute__((packed)) lang;
} __attribute__((packed)) link_strings = {
    .header =
        {
            .magic = FUNCTIONFS_STRINGS_MAGIC,
            .length = sizeof(link_strings),
            .str_count = 1,
            .lang_count = 1,
        },
    .lang = {0x0409, LINK_INTERFACE_NAME},
};

static void notify(void) {
    if (cable.notify_fd < 0) return;
    const char byte = 1;
    if (write(cable.notify_fd, &byte, 1) < 0 && errno != EAGAIN) return;
}

static void set_device_state(const usb_link_state state) {
    if (cable.device_state == state) return;
    cable.device_state = state;
    notify();
}

static void set_host_state(const usb_link_state state) {
    if (cable.host_state == state) return;
    cable.host_state = state;
    notify();
}

static void wake_thread(const int sig) {
    (void) sig;
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) ts.tv_nsec / 1000000u;
}

static void sleep_ms(const int ms) {
    const struct timespec delay = {.tv_sec = ms / 1000, .tv_nsec = (long) (ms % 1000) * 1000000L};
    nanosleep(&delay, NULL);
}

static int path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

static int write_text(const char *path, const char *text) {
    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    const size_t length = strlen(text);
    const ssize_t written = write(fd, text, length);
    const int saved = errno;
    close(fd);
    errno = saved;

    return written == (ssize_t) length ? 0 : -1;
}

static int read_text(const char *path, char *out, const size_t size) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    const ssize_t count = read(fd, out, size - 1);
    close(fd);
    if (count < 0) return -1;

    out[count] = '\0';
    out[strcspn(out, "\r\n")] = '\0';
    return 0;
}

static int read_hex(const char *path, unsigned *value) {
    char text[32];
    if (read_text(path, text, sizeof(text)) != 0) return -1;

    char *end = NULL;
    const unsigned long parsed = strtoul(text, &end, 16);
    if (end == text) return -1;

    *value = (unsigned) parsed;
    return 0;
}

static int read_decimal(const char *path, int *value) {
    char text[32];
    if (read_text(path, text, sizeof(text)) != 0) return -1;

    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (end == text) return -1;

    *value = (int) parsed;
    return 0;
}

static int first_udc(char *out, const size_t size) {
    DIR *dir = opendir(LINK_UDC_CLASS);
    if (!dir) return 0;

    const struct dirent *entry;
    int found = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        snprintf(out, size, "%.63s", entry->d_name);
        found = 1;
        break;
    }

    closedir(dir);
    return found;
}

int usb_link_supported(void) {
    char udc[64];
    return first_udc(udc, sizeof(udc)) && access(LINK_TUN, R_OK | W_OK) == 0
           && path_exists("/sys/kernel/config/usb_gadget");
}

static void make_mac(uint8_t *mac) {
    if (getrandom(mac, 6, 0) != 6) {
        const uint64_t seed = now_ms() ^ (uint64_t) getpid() << 20;
        for (int i = 0; i < 6; i++)
            mac[i] = (uint8_t) (seed >> (i * 8));
    }

    mac[0] = (uint8_t) ((mac[0] & 0xfe) | 0x02);
}

static int tap_open(const char *name, const uint8_t *mac) {
    const int fd = open(LINK_TUN, O_RDWR | O_CLOEXEC);
    if (fd < 0) return -1;

    struct ifreq request = {0};
    request.ifr_flags = IFF_TAP | IFF_NO_PI;
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name);

    if (ioctl(fd, TUNSETIFF, &request) != 0) {
        close(fd);
        return -1;
    }

    const int control = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (control >= 0) {
        struct ifreq address = {0};
        snprintf(address.ifr_name, sizeof(address.ifr_name), "%s", name);
        address.ifr_hwaddr.sa_family = ARPHRD_ETHER;
        memcpy(address.ifr_hwaddr.sa_data, mac, 6);
        ioctl(control, SIOCSIFHWADDR, &address);
        close(control);
    }

    return fd;
}

static void tap_close(int *tap) {
    if (*tap >= 0) close(*tap);
    *tap = -1;
}

static size_t record_build(uint8_t *record, const uint8_t *frame, const size_t length) {
    record[0] = (uint8_t) (length >> 8);
    record[1] = (uint8_t) (length & 0xff);
    memcpy(record + 2, frame, length);

    size_t total = length + 2;
    if (total % LINK_PACKET_ALIGN == 0) record[total++] = 0;

    return total;
}

static int record_frame(const uint8_t *record, const size_t count, size_t *length) {
    if (count < 2) return 0;

    const size_t value = (size_t) record[0] << 8 | record[1];
    if (!value || value > LINK_FRAME_MAX || value + 2 > count) return 0;

    *length = value;
    return 1;
}

static int tap_acquire(const int owner) {
    pthread_mutex_lock(&cable.tap_lock);

    int granted = cable.tap_owner == tap_owner_none || cable.tap_owner == owner;
    if (granted && cable.tap < 0) cable.tap = tap_open(LINK_IFACE, cable.mac);
    if (granted && cable.tap < 0) granted = 0;
    if (granted) cable.tap_owner = owner;

    pthread_mutex_unlock(&cable.tap_lock);
    return granted;
}

static void tap_release(const int owner) {
    pthread_mutex_lock(&cable.tap_lock);
    if (cable.tap_owner == owner) {
        cable.tap_owner = tap_owner_none;
        cable.tap_idle_since = now_ms();
    }
    pthread_mutex_unlock(&cable.tap_lock);
}

static void tap_linger(void) {
    pthread_mutex_lock(&cable.tap_lock);
    if (cable.tap_owner == tap_owner_none && cable.tap >= 0 && now_ms() - cable.tap_idle_since > LINK_LINGER_MS)
        tap_close(&cable.tap);
    pthread_mutex_unlock(&cable.tap_lock);
}

static void tap_deliver(const int tap, const uint8_t *record, const size_t count) {
    size_t length = 0;
    if (!record_frame(record, count, &length)) return;
    if (write(tap, record + 2, length) < 0) return;
}

static void *device_rx_thread(void *unused) {
    (void) unused;
    uint8_t record[LINK_RECORD_MAX];

    while (cable.bridge_active) {
        const ssize_t count = read(cable.ep_out, record, sizeof(record));
        if (count < 0) {
            if (errno == EINTR) continue;
            break;
        }

        tap_deliver(cable.tap, record, (size_t) count);
    }

    return NULL;
}

static void *device_tx_thread(void *unused) {
    (void) unused;
    uint8_t frame[LINK_FRAME_MAX];
    uint8_t record[LINK_RECORD_MAX];

    while (cable.bridge_active) {
        struct pollfd watch = {.fd = cable.tap, .events = POLLIN};
        const int ready = poll(&watch, 1, LINK_POLL_MS);
        if (ready < 0 && errno != EINTR) break;
        if (ready <= 0 || !(watch.revents & POLLIN)) continue;

        const ssize_t length = read(cable.tap, frame, sizeof(frame));
        if (length <= 0) continue;

        const size_t total = record_build(record, frame, (size_t) length);
        if (write(cable.ep_in, record, total) < 0 && errno != EINTR && errno != EAGAIN) break;
    }

    return NULL;
}

static void device_bridge_stop(void) {
    pthread_mutex_lock(&cable.bridge_lock);

    if (cable.bridge_started) {
        cable.bridge_active = 0;
        pthread_kill(cable.device_rx, SIGUSR1);
        pthread_kill(cable.device_tx, SIGUSR1);
        pthread_join(cable.device_rx, NULL);
        pthread_join(cable.device_tx, NULL);
        cable.bridge_started = 0;
        tap_release(tap_owner_device);
    }

    pthread_mutex_unlock(&cable.bridge_lock);
}

static void device_bridge_start(void) {
    pthread_mutex_lock(&cable.bridge_lock);

    if (cable.bridge_started || !tap_acquire(tap_owner_device)) {
        pthread_mutex_unlock(&cable.bridge_lock);
        return;
    }

    cable.bridge_active = 1;
    if (pthread_create(&cable.device_rx, NULL, device_rx_thread, NULL) != 0) {
        cable.bridge_active = 0;
        tap_release(tap_owner_device);
        set_device_state(usb_link_failed);
        pthread_mutex_unlock(&cable.bridge_lock);
        return;
    }

    if (pthread_create(&cable.device_tx, NULL, device_tx_thread, NULL) != 0) {
        cable.bridge_active = 0;
        pthread_kill(cable.device_rx, SIGUSR1);
        pthread_join(cable.device_rx, NULL);
        tap_release(tap_owner_device);
        set_device_state(usb_link_failed);
        pthread_mutex_unlock(&cable.bridge_lock);
        return;
    }

    cable.bridge_started = 1;
    set_device_state(usb_link_device);
    pthread_mutex_unlock(&cable.bridge_lock);
}

static void answer_setup(const struct usb_ctrlrequest *request) {
    if (request->bRequestType & USB_DIR_IN) {
        if (write(cable.ep0, NULL, 0) < 0) return;
    } else {
        if (read(cable.ep0, NULL, 0) < 0) return;
    }
}

static void *control_thread(void *unused) {
    (void) unused;

    while (cable.running) {
        tap_linger();
        if (cable.device_enabled && !cable.bridge_started) device_bridge_start();

        struct pollfd watch = {.fd = cable.ep0, .events = POLLIN};
        const int ready = poll(&watch, 1, LINK_POLL_MS);
        if (ready < 0 && errno != EINTR) break;
        if (ready <= 0 || !(watch.revents & POLLIN)) continue;

        struct usb_functionfs_event events[4];
        const ssize_t count = read(cable.ep0, events, sizeof(events));
        if (count < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            break;
        }

        for (size_t i = 0; i < (size_t) count / sizeof(events[0]); i++) {
            switch (events[i].type) {
                case FUNCTIONFS_ENABLE:
                case FUNCTIONFS_RESUME:
                    cable.device_enabled = 1;
                    device_bridge_start();
                    break;
                case FUNCTIONFS_DISABLE:
                case FUNCTIONFS_UNBIND:
                case FUNCTIONFS_SUSPEND:
                    cable.device_enabled = 0;
                    device_bridge_stop();
                    if (cable.running) set_device_state(usb_link_waiting);
                    break;
                case FUNCTIONFS_SETUP:
                    answer_setup(&events[i].u.setup);
                    break;
                default:
                    break;
            }
        }
    }

    cable.device_enabled = 0;
    device_bridge_stop();
    return NULL;
}

static int gadget_write(const char *relative, const char *text) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), LINK_GADGET "/%s", relative);
    return write_text(path, text);
}

static int gadget_mkdir(const char *relative) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), LINK_GADGET "%s%s", relative[0] ? "/" : "", relative);
    return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

static void gadget_rmdir(const char *relative) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), LINK_GADGET "%s%s", relative[0] ? "/" : "", relative);
    rmdir(path);
}

static int open_endpoint(const char *name, const int flags) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), LINK_FFS "/%s", name);

    for (int attempt = 0; attempt < 30; attempt++) {
        const int fd = open(path, flags | O_CLOEXEC);
        if (fd >= 0) return fd;
        sleep_ms(100);
    }

    return -1;
}

static int release_udc(const char *gadget) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/UDC", gadget);

    for (int waited = 0; waited < LINK_RELEASE_MS; waited += 100) {
        char current[64] = "";
        if (read_text(path, current, sizeof(current)) != 0 || !current[0]) return 0;
        if (write_text(path, "\n") == 0) return 0;
        sleep_ms(100);
    }

    return -1;
}

static void run_usb_script(const char *action) {
    if (access(LINK_USB_SCRIPT, X_OK) != 0) return;

    const pid_t child = fork();
    if (child < 0) return;

    if (child == 0) {
        signal(SIGUSR1, SIG_DFL);
        const int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
        }
        execl(LINK_USB_SCRIPT, LINK_USB_SCRIPT, action, (char *) NULL);
        _exit(127);
    }

    for (int waited = 0; waited < LINK_SCRIPT_MS; waited += 50) {
        if (waitpid(child, NULL, WNOHANG) == child) return;
        sleep_ms(50);
    }
}

static void gadget_teardown(void) {
    if (cable.gadget_built && release_udc(LINK_GADGET) != 0) {
        set_device_state(usb_link_failed);
        return;
    }

    if (cable.control_started) {
        pthread_kill(cable.control, SIGUSR1);
        pthread_join(cable.control, NULL);
        cable.control_started = 0;
    }

    if (cable.ep_in >= 0) close(cable.ep_in);
    if (cable.ep_out >= 0) close(cable.ep_out);
    if (cable.ep0 >= 0) close(cable.ep0);
    cable.ep_in = cable.ep_out = cable.ep0 = -1;

    if (cable.ffs_mounted) {
        if (umount(LINK_FFS) != 0) umount2(LINK_FFS, MNT_DETACH);
        rmdir(LINK_FFS);
        cable.ffs_mounted = 0;
    }

    if (cable.gadget_built) {
        unlink(LINK_GADGET "/configs/c.1/" LINK_FUNCTION);
        gadget_rmdir("configs/c.1/strings/0x409");
        gadget_rmdir("configs/c.1");
        gadget_rmdir("functions/" LINK_FUNCTION);
        gadget_rmdir("strings/0x409");
        gadget_rmdir("");
        cable.gadget_built = 0;
    }

    int usb_function = 0;
    if (!cable.gadget_built && read_decimal(LINK_USB_FUNCTION, &usb_function) == 0 && usb_function > 0)
        run_usb_script("start");
}

static int gadget_build(void) {
    if (!first_udc(cable.udc, sizeof(cable.udc))) return -1;

    run_usb_script("stop");
    run_usb_script("disable");

    char serial[128] = "mulink";
    read_text(LINK_MUOS_GADGET "/strings/0x409/serialnumber", serial, sizeof(serial));
    if (!serial[0]) snprintf(serial, sizeof(serial), "mulink");

    if (gadget_mkdir("") != 0) return -1;
    cable.gadget_built = 1;

    char value[16];
    snprintf(value, sizeof(value), "0x%04x", LINK_VENDOR);
    if (gadget_write("idVendor", value) != 0) return -1;
    snprintf(value, sizeof(value), "0x%04x", LINK_PRODUCT);
    if (gadget_write("idProduct", value) != 0) return -1;

    if (gadget_mkdir("strings/0x409") != 0 || gadget_write("strings/0x409/manufacturer", "MustardOS") != 0
        || gadget_write("strings/0x409/product", LINK_INTERFACE_NAME) != 0
        || gadget_write("strings/0x409/serialnumber", serial) != 0)
        return -1;

    if (gadget_mkdir("configs/c.1") != 0 || gadget_mkdir("configs/c.1/strings/0x409") != 0
        || gadget_write("configs/c.1/strings/0x409/configuration", LINK_INTERFACE_NAME) != 0
        || gadget_write("configs/c.1/MaxPower", "500") != 0)
        return -1;

    if (gadget_mkdir("functions/" LINK_FUNCTION) != 0) return -1;
    if (symlink(LINK_GADGET "/functions/" LINK_FUNCTION, LINK_GADGET "/configs/c.1/" LINK_FUNCTION) != 0
        && errno != EEXIST)
        return -1;

    mkdir(LINK_FFS_ROOT, 0755);
    if (mkdir(LINK_FFS, 0755) != 0 && errno != EEXIST) return -1;
    if (mount("mulink", LINK_FFS, "functionfs", 0, NULL) != 0) return -1;
    cable.ffs_mounted = 1;

    cable.ep0 = open(LINK_FFS "/ep0", O_RDWR | O_CLOEXEC);
    if (cable.ep0 < 0) return -1;
    if (write(cable.ep0, &link_descriptors, sizeof(link_descriptors)) != (ssize_t) sizeof(link_descriptors)) return -1;
    if (write(cable.ep0, &link_strings, sizeof(link_strings)) != (ssize_t) sizeof(link_strings)) return -1;

    cable.ep_in = open_endpoint("ep1", O_WRONLY);
    cable.ep_out = open_endpoint("ep2", O_RDONLY);
    if (cable.ep_in < 0 || cable.ep_out < 0) return -1;

    if (release_udc(LINK_MUOS_GADGET) != 0) return -1;
    if (gadget_write("UDC", cable.udc) != 0) return -1;

    if (pthread_create(&cable.control, NULL, control_thread, NULL) != 0) return -1;
    cable.control_started = 1;

    return 0;
}

struct host_peer {
    char path[PATH_MAX];
    int interface;
    unsigned ep_in;
    unsigned ep_out;
};

static int host_endpoints(const char *interface_dir, struct host_peer *peer) {
    DIR *dir = opendir(interface_dir);
    if (!dir) return 0;

    peer->ep_in = peer->ep_out = 0;
    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "ep_", 3) != 0) continue;

        char path[PATH_MAX];
        char type[16] = "";
        snprintf(path, sizeof(path), "%.512s/%.64s/type", interface_dir, entry->d_name);
        if (read_text(path, type, sizeof(type)) != 0 || strcmp(type, "Bulk") != 0) continue;

        unsigned address = 0;
        snprintf(path, sizeof(path), "%.512s/%.64s/bEndpointAddress", interface_dir, entry->d_name);
        if (read_hex(path, &address) != 0) continue;

        if (address & USB_DIR_IN)
            peer->ep_in = address;
        else
            peer->ep_out = address;
    }

    closedir(dir);
    return peer->ep_in && peer->ep_out;
}

static int host_find_interface(const char *device, struct host_peer *peer) {
    DIR *dir = opendir(LINK_USB_DEVICES);
    if (!dir) return 0;

    const size_t prefix = strlen(device);
    const struct dirent *entry;
    int found = 0;
    while (!found && (entry = readdir(dir))) {
        if (strncmp(entry->d_name, device, prefix) != 0 || entry->d_name[prefix] != ':') continue;

        char base[512];
        char path[PATH_MAX];
        snprintf(base, sizeof(base), LINK_USB_DEVICES "/%.64s", entry->d_name);

        unsigned class_code = 0, subclass = 0, protocol = 0;
        snprintf(path, sizeof(path), "%s/bInterfaceClass", base);
        if (read_hex(path, &class_code) != 0 || class_code != USB_CLASS_VENDOR_SPEC) continue;
        snprintf(path, sizeof(path), "%s/bInterfaceSubClass", base);
        if (read_hex(path, &subclass) != 0 || subclass != LINK_SUBCLASS) continue;
        snprintf(path, sizeof(path), "%s/bInterfaceProtocol", base);
        if (read_hex(path, &protocol) != 0 || protocol != LINK_PROTOCOL) continue;

        unsigned number = 0;
        snprintf(path, sizeof(path), "%s/bInterfaceNumber", base);
        if (read_hex(path, &number) != 0) continue;

        peer->interface = (int) number;
        found = host_endpoints(base, peer);
    }

    closedir(dir);
    return found;
}

static int host_find_peer(struct host_peer *peer) {
    DIR *dir = opendir(LINK_USB_DEVICES);
    if (!dir) return 0;

    const struct dirent *entry;
    int found = 0;
    while (!found && (entry = readdir(dir))) {
        if (entry->d_name[0] == '.' || strchr(entry->d_name, ':')) continue;

        char path[PATH_MAX];
        unsigned vendor = 0, product = 0;
        snprintf(path, sizeof(path), LINK_USB_DEVICES "/%s/idVendor", entry->d_name);
        if (read_hex(path, &vendor) != 0 || vendor != LINK_VENDOR) continue;
        snprintf(path, sizeof(path), LINK_USB_DEVICES "/%s/idProduct", entry->d_name);
        if (read_hex(path, &product) != 0 || product != LINK_PRODUCT) continue;

        int bus = 0, device = 0;
        snprintf(path, sizeof(path), LINK_USB_DEVICES "/%s/busnum", entry->d_name);
        if (read_decimal(path, &bus) != 0) continue;
        snprintf(path, sizeof(path), LINK_USB_DEVICES "/%s/devnum", entry->d_name);
        if (read_decimal(path, &device) != 0) continue;

        if (!host_find_interface(entry->d_name, peer)) continue;

        snprintf(peer->path, sizeof(peer->path), "/dev/bus/usb/%03d/%03d", bus, device);
        found = 1;
    }

    closedir(dir);
    return found;
}

static int submit_urb(const int fd, struct usbdevfs_urb *urb, const unsigned endpoint, void *buffer, const int length) {
    memset(urb, 0, sizeof(*urb));
    urb->type = USBDEVFS_URB_TYPE_BULK;
    urb->endpoint = (unsigned char) endpoint;
    urb->buffer = buffer;
    urb->buffer_length = length;
    return ioctl(fd, USBDEVFS_SUBMITURB, urb);
}

static void host_session(const struct host_peer *peer) {
    const int fd = open(peer->path, O_RDWR | O_CLOEXEC);
    if (fd < 0) return;

    unsigned int interface = (unsigned int) peer->interface;
    if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &interface) != 0) {
        close(fd);
        return;
    }

    if (!tap_acquire(tap_owner_host)) {
        device_bridge_stop();
        set_device_state(usb_link_waiting);
    }
    if (!tap_acquire(tap_owner_host)) {
        ioctl(fd, USBDEVFS_RELEASEINTERFACE, &interface);
        close(fd);
        set_host_state(usb_link_failed);
        return;
    }
    const int tap = cable.tap;

    static uint8_t rx_buffer[LINK_RECORD_MAX];
    static uint8_t tx_buffer[LINK_RECORD_MAX];
    struct usbdevfs_urb rx_urb;
    struct usbdevfs_urb tx_urb;
    int rx_pending = submit_urb(fd, &rx_urb, peer->ep_in, rx_buffer, sizeof(rx_buffer)) == 0;
    int tx_pending = 0;

    if (rx_pending) set_host_state(usb_link_host);

    while (cable.running && rx_pending) {
        struct pollfd watch[2] = {{.fd = fd, .events = POLLOUT}, {.fd = tap, .events = tx_pending ? 0 : POLLIN}};
        const int ready = poll(watch, 2, LINK_POLL_MS);
        if (ready < 0 && errno != EINTR) break;
        if (ready <= 0) continue;

        if (watch[0].revents & (POLLERR | POLLHUP)) break;

        if (watch[0].revents & POLLOUT) {
            int lost = 0;
            for (;;) {
                struct usbdevfs_urb *done = NULL;
                if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &done) != 0) {
                    if (errno != EAGAIN) lost = 1;
                    break;
                }

                if (done == &rx_urb) {
                    if (rx_urb.status == 0) tap_deliver(tap, rx_buffer, (size_t) rx_urb.actual_length);
                    rx_pending = rx_urb.status != -ENODEV && rx_urb.status != -ESHUTDOWN
                                 && submit_urb(fd, &rx_urb, peer->ep_in, rx_buffer, sizeof(rx_buffer)) == 0;
                } else if (done == &tx_urb) {
                    tx_pending = 0;
                }
            }
            if (lost) break;
        }

        if (!tx_pending && watch[1].revents & POLLIN) {
            uint8_t frame[LINK_FRAME_MAX];
            const ssize_t length = read(tap, frame, sizeof(frame));
            if (length > 0) {
                const size_t total = record_build(tx_buffer, frame, (size_t) length);
                tx_pending = submit_urb(fd, &tx_urb, peer->ep_out, tx_buffer, (int) total) == 0;
            }
        }
    }

    struct usbdevfs_urb *done = NULL;
    if (rx_pending) ioctl(fd, USBDEVFS_DISCARDURB, &rx_urb);
    if (tx_pending) ioctl(fd, USBDEVFS_DISCARDURB, &tx_urb);
    for (int attempt = 0; attempt < 10 && (rx_pending || tx_pending); attempt++) {
        if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &done) != 0) {
            if (errno != EAGAIN) break;
            sleep_ms(20);
            continue;
        }
        if (done == &rx_urb) rx_pending = 0;
        if (done == &tx_urb) tx_pending = 0;
    }

    tap_release(tap_owner_host);
    ioctl(fd, USBDEVFS_RELEASEINTERFACE, &interface);
    close(fd);
}

static void *host_thread(void *unused) {
    (void) unused;

    while (cable.running) {
        struct host_peer peer;
        if (!host_find_peer(&peer)) {
            tap_linger();
            set_host_state(usb_link_waiting);
            for (int waited = 0; waited < LINK_SCAN_MS && cable.running; waited += LINK_POLL_MS)
                sleep_ms(LINK_POLL_MS);
            continue;
        }

        host_session(&peer);
        if (cable.running) {
            set_host_state(usb_link_waiting);
            sleep_ms(LINK_POLL_MS);
        }
    }

    set_host_state(usb_link_off);
    return NULL;
}

int usb_link_start(const int notify_fd) {
    pthread_mutex_lock(&cable.lock);
    if (cable.running) {
        pthread_mutex_unlock(&cable.lock);
        return 0;
    }

    struct sigaction action = {0};
    action.sa_handler = wake_thread;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR1, &action, NULL);

    if (!cable.mac[0]) make_mac(cable.mac);

    cable.notify_fd = notify_fd;
    cable.running = 1;
    cable.device_state = usb_link_waiting;
    cable.host_state = usb_link_waiting;

    if (gadget_build() != 0) {
        gadget_teardown();
        cable.device_state = usb_link_failed;
    }

    cable.host_started = pthread_create(&cable.host, NULL, host_thread, NULL) == 0;
    if (!cable.host_started) cable.host_state = usb_link_failed;

    pthread_mutex_unlock(&cable.lock);
    notify();
    return 0;
}

void usb_link_stop(void) {
    pthread_mutex_lock(&cable.lock);
    if (!cable.running) {
        pthread_mutex_unlock(&cable.lock);
        return;
    }

    cable.running = 0;

    if (cable.host_started) {
        pthread_kill(cable.host, SIGUSR1);
        pthread_join(cable.host, NULL);
        cable.host_started = 0;
    }

    gadget_teardown();

    pthread_mutex_lock(&cable.tap_lock);
    cable.tap_owner = tap_owner_none;
    tap_close(&cable.tap);
    pthread_mutex_unlock(&cable.tap_lock);

    cable.device_state = usb_link_off;
    cable.host_state = usb_link_off;
    pthread_mutex_unlock(&cable.lock);
    notify();
}

usb_link_state usb_link_current(void) {
    if (!cable.running) return usb_link_off;

    const usb_link_state device = cable.device_state;
    const usb_link_state host = cable.host_state;

    if (host == usb_link_host) return host;
    if (device == usb_link_device) return device;
    if (device == usb_link_failed && host == usb_link_failed) return usb_link_failed;

    return usb_link_waiting;
}

const char *usb_link_state_name(const usb_link_state state) {
    switch (state) {
        case usb_link_off:
            return "off";
        case usb_link_waiting:
            return "waiting";
        case usb_link_device:
            return "device";
        case usb_link_host:
            return "host";
        case usb_link_failed:
            return "failed";
        default:
            return "unsupported";
    }
}
