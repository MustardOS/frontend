DEVICE := $(strip $(DEVICE))

ifeq ($(DEVICE), ARM64)          # standard aarch64
    ARCH = -march=armv8-a
else ifeq ($(DEVICE), ARM64_A53) # currently for h700 and a133p
    ARCH = -mtune=cortex-a53 -mcpu=cortex-a53+crc+crypto
else ifeq ($(DEVICE), ARM32)     # only if you still build armhf for og 35x
    ARCH = -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard
else ifeq ($(DEVICE), ARM32_A9)  # armhf w/thumb for cortex a9 with neon
    ARCH = -mcpu=cortex-a9 -mthumb -mfpu=neon-vfpv3 -mfloat-abi=softfp
else ifeq ($(DEVICE), NATIVE)    # everything else, like maybe x86?
    ARCH = -march=native
else
    $(error Unsupported Device: $(DEVICE))
endif

CC = ccache $(CROSS_COMPILE)gcc -O3 -pipe

CFLAGS = $(ARCH) -flto=auto -ffunction-sections -fdata-sections \
	-flto -finline-functions -Wall -Wno-format-zero-length -fPIC

LDFLAGS = -shared $(CFLAGS) -lpthread -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fmerge-all-constants -fno-ident -ffast-math -funroll-loops -falign-functions

SRC_DIRS = .
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

LIB_DIR = ${shell pwd}/../bin/lib
LIB_FILES = $(patsubst %.c,$(LIB_DIR)/lib%.so,$(notdir $(SRCS)))

all: $(LIB_FILES)

$(LIB_DIR)/lib%.so: $(SRC_DIRS)/%.c
	@printf "Compiling %s into %s...\n" "$<" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $< $(LDFLAGS) -o $@
	@find ${shell pwd} -name "*.o" -exec rm -f {} +
