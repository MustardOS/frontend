SHELL   = /bin/sh
DEVICE := $(strip $(DEVICE))

A53_FIX = -mfix-cortex-a53-835769 -mfix-cortex-a53-843419

# standard aarch64
ifeq ($(DEVICE), ARM64)
    ARCH = -march=armv8-a

# currently for h700 and a133p
else ifeq ($(DEVICE), ARM64_A53)
    ARCH = -mcpu=cortex-a53 -mtune=cortex-a53 $(A53_FIX)

# speciality for hardware crypto if supported
else ifeq ($(DEVICE), ARM64_A53_CRYPTO)
    ARCH = -mcpu=cortex-a53+crc+crypto -mtune=cortex-a53 $(A53_FIX)

# only for builds of armhf for og 35x
else ifeq ($(DEVICE), ARM32)
    ARCH = -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard

# armhf w/thumb for cortex a9 with neon
else ifeq ($(DEVICE), ARM32_A9)
    ARCH = -mcpu=cortex-a9 -mthumb -mfpu=neon-vfpv3 -mfloat-abi=softfp

# everything else, like maybe x86?
else ifeq ($(DEVICE), NATIVE)
    ARCH = -march=native

# unsupported or not specified
else
    $(error Unsupported Device: $(DEVICE))
endif

CC = ccache $(CROSS_COMPILE)gcc

DEBUG  ?= 0
VERBOSE = $(if $(filter 2,$(DEBUG)),, @)
QUIET   = $(if $(filter 1,$(DEBUG)),,>/dev/null 2>&1)

OPT_LEVEL ?= 2

BASE_CFLAGS = $(ARCH) -O$(OPT_LEVEL) -pipe -flto=auto \
              -ffunction-sections -fdata-sections \
              -Wall -Wno-format-zero-length \
              -Wno-unused-function -fno-plt \
              -fno-stack-protector -fno-ident

COMMON_LIBS = -lcurl -lSDL2 -lSDL2_mixer -lSDL2_ttf -lSDL2_image -lpthread

BIN_LDFLAGS  = -Wl,--gc-sections -s
LIB_LDFLAGS  = -Wl,-rpath,'./lib'

SHARED_PIC = -shared -fPIC
