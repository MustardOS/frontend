DEVICE := $(strip $(DEVICE))

ifeq ($(DEVICE), ARM64)          # standard aarch64
    ARCH = -march=armv8-a
else ifeq ($(DEVICE), ARM64_A53) # currently for h700 and a133p
    ARCH = -mcpu=cortex-a53+crc+crypto -mtune=cortex-a53 -mfix-cortex-a53-835769 -mfix-cortex-a53-843419
else ifeq ($(DEVICE), ARM32)     # only if you still build armhf for og 35x
    ARCH = -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard
else ifeq ($(DEVICE), ARM32_A9)  # armhf w/thumb for cortex a9 with neon
    ARCH = -mcpu=cortex-a9 -mthumb -mfpu=neon-vfpv3 -mfloat-abi=softfp
else ifeq ($(DEVICE), NATIVE)    # everything else, like maybe x86?
    ARCH = -march=native
else
    $(error Unsupported Device: $(DEVICE))
endif

CC = ccache $(CROSS_COMPILE)gcc

CFLAGS = $(ARCH) -O3 -pipe -flto=auto \
         -ffunction-sections -fdata-sections \
         -finline-functions -fmerge-all-constants \
         -Wall -Wno-format-zero-length -Wno-unused-function\
         -fno-stack-protector -fno-ident -fPIC

LDFLAGS = -shared $(CFLAGS) -lpthread -Wl,--gc-sections -s

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
