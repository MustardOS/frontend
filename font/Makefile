ifeq ($(DEVICE), RG35XX)
    ARCH = -march=armv7-a -mtune=cortex-a9 -mfpu=neon-vfpv3 -mfloat-abi=softfp
else ifeq ($(DEVICE), RG35XXPLUS)
    ARCH = -march=armv8-a+simd -mtune=cortex-a53
else ifeq ($(DEVICE), ARM64)
    ARCH = -march=armv8-a
else ifeq ($(DEVICE), NATIVE)
    ARCH = -march=native
else
    $(error Unsupported Device: $(DEVICE))
endif

CC = ccache $(CROSS_COMPILE)gcc -O3

CFLAGS = $(ARCH) -flto=auto -ffunction-sections -fdata-sections \
	-flto -finline-functions -Wall -Wno-format-zero-length -fPIC

LDFLAGS = -shared $(CFLAGS) -lpthread -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fno-unroll-loops -fmerge-all-constants -fno-ident \
	-ffast-math -funroll-loops -falign-functions

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
