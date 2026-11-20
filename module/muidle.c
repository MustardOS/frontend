#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <common/platform/h700.h>

#define KALLSYMS    "/proc/kallsyms"
#define IOMEM       "/proc/iomem"
#define INPUT_CLASS "/sys/class/input"
#define STATE_FILE  "/run/muos/muidle"
#define PAGE_OFFSET 0xffffffc000000000ULL
#define MAX_SYMBOLS 4
#define MAP_SIZE    8192

#define POLL_MAX_MS   1000U
#define POLL_GAME_MS  10U
#define POLL_MENU_MS  16U
#define POLL_SLEEP_MS 500U

#define MOVZ(reg, imm) (0xd2800000U | ((uint32_t) (imm) << 5) | (uint32_t) (reg))

enum patch_state { patch_awake, patch_asleep };

typedef struct {
    const char *symbol;
    uint32_t original;
    uint32_t value[2];
} code_patch;

static const code_patch patches[] = {
    {"pktgen_thread_worker", MOVZ(20, 10), {MOVZ(20, 6000), MOVZ(20, 6000)}},
    {"allen_my_led_poll", MOVZ(3, 50), {MOVZ(3, 200), MOVZ(3, 500)}},
    {"axp2202_usb_power_monitor", MOVZ(3, 50), {MOVZ(3, 50), MOVZ(3, 500)}},
};

static int mem_fd = -1;
static uint64_t text_virt;
static uint64_t text_phys;
static uint64_t ram_phys;
static uint64_t ram_size;

static int symbol_addresses(const char *name, uint64_t *out, const int max) {
    FILE *file = fopen(KALLSYMS, "r");
    if (!file) return 0;

    int count = 0;
    char line[256];
    while (count < max && fgets(line, sizeof(line), file)) {
        unsigned long long address;
        char type;
        char symbol[200];
        if (sscanf(line, "%llx %c %199s", &address, &type, symbol) != 3) continue;
        if (strcmp(symbol, name) == 0 && address) out[count++] = address;
    }

    fclose(file);
    return count;
}

static uint64_t symbol_end(const uint64_t start) {
    FILE *file = fopen(KALLSYMS, "r");
    if (!file) return start;

    uint64_t end = UINT64_MAX;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        unsigned long long address;
        if (sscanf(line, "%llx", &address) == 1 && address > start && address < end) end = address;
    }

    fclose(file);
    return end == UINT64_MAX ? start : end;
}

static int load_layout(void) {
    uint64_t text;
    if (symbol_addresses("_text", &text, 1) != 1) return 0;
    text_virt = text;

    FILE *file = fopen(IOMEM, "r");
    if (!file) return 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        unsigned long long start, end;
        if (sscanf(line, " %llx-%llx", &start, &end) != 2) continue;
        if (!ram_phys && strstr(line, "System RAM")) {
            ram_phys = start;
            ram_size = end - start + 1;
        }
        if (!text_phys && strstr(line, "Kernel code")) text_phys = start;
    }

    fclose(file);
    return ram_phys && text_phys;
}

static uint64_t image_phys(const uint64_t virt) {
    return virt - text_virt + text_phys;
}

static int linear_virt(const uint64_t virt) {
    return virt >= PAGE_OFFSET && virt - PAGE_OFFSET < ram_size;
}

static uint64_t linear_phys(const uint64_t virt) {
    return virt - PAGE_OFFSET + ram_phys;
}

static volatile void *map_phys(const uint64_t phys, uint8_t **page_out) {
    const uint64_t page = phys & ~0xfffULL;
    uint8_t *map = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, (off_t) page);
    if (map == MAP_FAILED) return NULL;

    *page_out = map;
    return map + (phys - page);
}

static int read64(const uint64_t phys, uint64_t *value) {
    uint8_t *page;
    volatile uint64_t *word = map_phys(phys, &page);
    if (!word) return 0;

    *value = *word;
    munmap(page, MAP_SIZE);
    return 1;
}

static void sync_code(volatile void *address) {
    __asm__ volatile("dc cvau, %0\n\tdsb ish\n\tic ivau, %0\n\tdsb ish\n\tisb" : : "r"(address) : "memory");
}

static int patch_word(const code_patch *patch, const uint32_t word) {
    return word == patch->original || word == patch->value[patch_awake] || word == patch->value[patch_asleep];
}

static void apply_patches(const enum patch_state state) {
    for (size_t i = 0; i < sizeof(patches) / sizeof(patches[0]); i++) {
        const code_patch *patch = &patches[i];
        uint64_t function;
        if (symbol_addresses(patch->symbol, &function, 1) != 1) continue;

        uint8_t *page;
        volatile uint32_t *code = map_phys(image_phys(function), &page);
        if (!code) continue;

        const uint64_t limit = (MAP_SIZE - (image_phys(function) & 0xfffULL)) / 4;
        uint64_t words = (symbol_end(function) - function) / 4;
        if (words > limit) words = limit;

        volatile uint32_t *site = NULL;
        int sites = 0;
        for (uint64_t w = 0; w < words; w++) {
            if (!patch_word(patch, code[w])) continue;
            site = &code[w];
            sites++;
        }

        if (sites == 1) {
            const uint32_t want = patch->value[state];
            if (*site != want) {
                *site = want;
                sync_code(site);
            }
        } else {
            fprintf(stderr, "muidle: %s code does not match, left alone\n", patch->symbol);
        }

        munmap(page, MAP_SIZE);
    }
}

static int polldev_matches(
    const uint64_t virt, const uint64_t *opens, const int open_count, const uint64_t *polls, const int poll_count
) {
    if (!linear_virt(virt)) return 0;

    uint64_t open_fn, poll_fn;
    if (!read64(linear_phys(virt) + 0x08, &open_fn) || !read64(linear_phys(virt) + 0x18, &poll_fn)) return 0;

    int open_ok = 0, poll_ok = 0;
    for (int i = 0; i < open_count; i++)
        open_ok |= open_fn == opens[i];
    for (int i = 0; i < poll_count; i++)
        poll_ok |= poll_fn == polls[i];
    return open_ok && poll_ok;
}

static uint64_t find_polldev(void) {
    uint64_t list, opens[MAX_SYMBOLS], polls[MAX_SYMBOLS];
    if (symbol_addresses("input_dev_list", &list, 1) != 1) return 0;

    const int open_count = symbol_addresses("gpio_keys_polled_open", opens, MAX_SYMBOLS);
    const int poll_count = symbol_addresses("gpio_keys_polled_poll", polls, MAX_SYMBOLS);
    if (!open_count || !poll_count) return 0;

    uint64_t node;
    if (!read64(image_phys(list), &node)) return 0;

    for (int guard = 0; guard < 64 && node != list && linear_virt(node); guard++) {
        for (uint64_t at = node - 0x600; at < node + 0x200; at += 8) {
            uint64_t candidate;
            if (read64(linear_phys(at), &candidate) && polldev_matches(candidate, opens, open_count, polls, poll_count))
                return linear_phys(candidate);
        }
        if (!read64(linear_phys(node), &node)) break;
    }

    return 0;
}

static int write_u32(const uint64_t phys, const uint32_t value) {
    uint8_t *page;
    volatile uint32_t *word = map_phys(phys, &page);
    if (!word) return 0;

    *word = value;
    munmap(page, MAP_SIZE);
    return 1;
}

static int read_u32(const uint64_t phys, uint32_t *value) {
    uint8_t *page;
    volatile uint32_t *word = map_phys(phys, &page);
    if (!word) return 0;

    *value = *word;
    munmap(page, MAP_SIZE);
    return 1;
}

static int poll_attribute(char *path, const size_t size) {
    DIR *dir = opendir(INPUT_CLASS);
    if (!dir) return 0;

    int found = 0;
    const struct dirent *entry;
    while (!found && (entry = readdir(dir))) {
        if (strncmp(entry->d_name, "input", 5) != 0) continue;

        char max_path[300];
        snprintf(max_path, sizeof(max_path), INPUT_CLASS "/%s/max", entry->d_name);
        FILE *file = fopen(max_path, "r");
        if (!file) continue;

        unsigned value = 0;
        found = fscanf(file, "%u", &value) == 1 && value == POLL_MAX_MS;
        fclose(file);
        if (found) snprintf(path, size, INPUT_CLASS "/%s/poll", entry->d_name);
    }

    closedir(dir);
    return found;
}

static unsigned read_text(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;

    unsigned value = 0;
    if (fscanf(file, "%u", &value) != 1) value = 0;
    fclose(file);
    return value;
}

static int write_text(const char *path, const unsigned value) {
    FILE *file = fopen(path, "w");
    if (!file) return 0;

    const int ok = fprintf(file, "%u", value) > 0;
    return fclose(file) == 0 && ok;
}

static void set_buttons(const unsigned interval) {
    char path[300];
    if (poll_attribute(path, sizeof(path))) write_text(path, interval);
}

static void raise_poll_limit(void) {
    const uint64_t polldev = find_polldev();
    uint32_t max;
    if (polldev && read_u32(polldev + 0x24, &max) && max < POLL_MAX_MS) write_u32(polldev + 0x24, POLL_MAX_MS);
}

static void sleep_buttons(void) {
    char path[300];
    if (!poll_attribute(path, sizeof(path))) return;

    const unsigned interval = read_text(path);
    if (!interval || interval == POLL_SLEEP_MS) return;

    FILE *state = fopen(STATE_FILE, "w");
    if (!state) return;
    fprintf(state, "%u\n", interval);
    fclose(state);

    write_text(path, POLL_SLEEP_MS);
}

static void wake_buttons(void) {
    const unsigned interval = read_text(STATE_FILE);
    unlink(STATE_FILE);
    if (interval) set_buttons(interval);
}

static int open_mem(void) {
    mem_fd = open("/dev/mem", O_RDWR | O_CLOEXEC);
    return mem_fd >= 0 && load_layout();
}

int main(const int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <boot|menu|game|sleep|wake>\n", argv[0]);
        return 2;
    }

    if (!platform_is_h700()) return 0;

    if (strcmp(argv[1], "menu") == 0) {
        set_buttons(POLL_MENU_MS);
    } else if (strcmp(argv[1], "game") == 0) {
        set_buttons(POLL_GAME_MS);
    } else if (strcmp(argv[1], "boot") == 0) {
        if (!open_mem()) return 1;
        apply_patches(patch_awake);
        raise_poll_limit();
        set_buttons(POLL_MENU_MS);
    } else if (strcmp(argv[1], "sleep") == 0) {
        if (!open_mem()) return 1;
        apply_patches(patch_asleep);
        sleep_buttons();
    } else if (strcmp(argv[1], "wake") == 0) {
        if (!open_mem()) return 1;
        wake_buttons();
        apply_patches(patch_awake);
    } else {
        return 2;
    }

    if (mem_fd >= 0) close(mem_fd);
    return 0;
}
