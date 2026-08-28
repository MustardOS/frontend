#include "sunxi-gpio.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef O_SYNC
#define O_SYNC 0x101000
#endif

#define SUNXI_PIO_PHYS_BASE    0x0300B000u
#define SUNXI_PIO_LM_PHYS_BASE 0x07022000u

#define SUNXI_GPIO_BANK_STRIDE 0x24u

#define OFF_CFG0  0x00u
#define OFF_DAT   0x10u
#define OFF_PULL0 0x1Cu

#define GPIO_BANK(pin) ((uint32_t) ((pin) >> 5))
#define GPIO_NUM(pin)  ((uint32_t) ((pin) & 0x1Fu))

#define GPIO_CFG_INDEX(pin)  (((pin) & 0x1Fu) >> 3)
#define GPIO_CFG_OFFSET(pin) ((((pin) & 0x1Fu) & 0x7u) << 2)

static size_t g_map_len = 0;

static void *g_map0 = NULL;
static void *g_map1 = NULL;
static volatile uint8_t *g_pio = NULL;
static volatile uint8_t *g_pio_lm = NULL;

static int map_region(uint32_t phys, void **out_map, volatile uint8_t **out_base) {
    long pagesz = sysconf(_SC_PAGESIZE);
    if (pagesz <= 0) return -1;
    g_map_len = (size_t) pagesz * 2;

    uint32_t page_mask = (uint32_t) (~((uint32_t) pagesz - 1u));
    uint32_t addr_start = phys & page_mask;
    uint32_t addr_offset = phys & ~page_mask;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return -1;

    void *m = mmap(NULL, g_map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr_start);
    close(fd);

    if (m == MAP_FAILED) return -1;

    *out_map = m;
    *out_base = (volatile uint8_t *) m + addr_offset;
    return 0;
}

int sunxi_gpio_initialise(void) {
    if (g_pio || g_pio_lm) return -1;

    if (map_region(SUNXI_PIO_PHYS_BASE, &g_map0, &g_pio) < 0) {
        return -1;
    }

    if (map_region(SUNXI_PIO_LM_PHYS_BASE, &g_map1, &g_pio_lm) < 0) {
        sunxi_gpio_close();
        return -1;
    }

    return 0;
}

void sunxi_gpio_close(void) {
    if (g_map0 && g_map_len) munmap(g_map0, g_map_len);
    if (g_map1 && g_map_len) munmap(g_map1, g_map_len);

    g_map0 = g_map1 = NULL;
    g_pio = g_pio_lm = NULL;
    g_map_len = 0;
}

static inline volatile uint8_t *bank_base(uint32_t pin) {
    if (!g_pio) return NULL;
    uint32_t bank = GPIO_BANK(pin);
    if (bank == 11u) {
        return g_pio_lm;
    }
    return g_pio + (size_t) bank * SUNXI_GPIO_BANK_STRIDE;
}

int sunxi_gpio_set_cfgpin(uint32_t pin, uint32_t val) {
    volatile uint8_t *b = bank_base(pin);
    if (!b) return -1;

    uint32_t idx = GPIO_CFG_INDEX(pin);
    uint32_t off = GPIO_CFG_OFFSET(pin);

    volatile uint32_t *cfg_reg = (volatile uint32_t *) (b + OFF_CFG0 + idx * 4u);
    uint32_t cfg = *cfg_reg;
    cfg &= ~(0xFu << off);
    cfg |= ((val & 0xFu) << off);
    *cfg_reg = cfg;

    return 0;
}

int sunxi_gpio_input(uint32_t pin) {
    volatile uint8_t *b = bank_base(pin);
    if (!b) return -1;

    uint32_t num = GPIO_NUM(pin);
    volatile uint32_t *dat_reg = (volatile uint32_t *) (b + OFF_DAT);
    uint32_t dat = *dat_reg;

    return (int) ((dat >> num) & 0x1u);
}