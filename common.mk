SHELL   = /bin/sh
DEVICE := $(strip $(DEVICE))

A53_FIX = -mfix-cortex-a53-835769 -mfix-cortex-a53-843419

BUILD ?= test

ifeq ($(BUILD),release)
    BUILD_FLAGS = -DTEST_IMAGE=0 -DMUOS_RELEASE
else
    BUILD_FLAGS = -DTEST_IMAGE=1
endif

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

# generic 64 bit x86, safe baseline rather than anything modern
else ifeq ($(DEVICE), X86_64)
    ARCH = -march=x86-64-v2 -mtune=generic

# generic 64 bit risc-v with the usual double float abi
else ifeq ($(DEVICE), RISCV64)
    ARCH = -march=rv64gc -mabi=lp64d

# whatever the build host happens to be
else ifeq ($(DEVICE), NATIVE)
    ARCH = -march=native

# escape hatch for an architecture we have not met, pass ARCH_FLAGS in yourself!
else ifeq ($(DEVICE), GENERIC)
    ifeq ($(strip $(ARCH_FLAGS)),)
        $(error DEVICE=GENERIC needs ARCH_FLAGS, e.g. ARCH_FLAGS="-march=armv8.2-a")
    endif
    ARCH = $(ARCH_FLAGS)

# unsupported or not specified
else
    $(error Unsupported Device: $(DEVICE))
endif

ifeq ($(DEVICE), NATIVE)
    CC = ccache gcc
    NM = nm
else
    CC = ccache $(CROSS_COMPILE)gcc
    NM = $(CROSS_COMPILE)nm
endif

DEBUG  ?= 0
VERBOSE = $(if $(filter 2,$(DEBUG)),, @)
QUIET   = $(if $(filter 1,$(DEBUG)),,>/dev/null 2>&1)

OPT_LEVEL ?= 2

DEBUGSYM ?= 0

REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
DEP_ROOT  := $(REPO_ROOT)/.deps

# Flattened so nothing escapes its own dep directory, ./ and ../ normalised first
DEP_NAME = $(DEP_ROOT)/$(1)/$(subst /,_,$(subst ../,up_,$(patsubst ./%,%,$(2)))).d

include $(dir $(lastword $(MAKEFILE_LIST)))external/external.mk

BASE_CFLAGS = $(ARCH) -std=c11 -O$(OPT_LEVEL) -pipe -flto=auto -MMD -MP \
              -ffunction-sections -fdata-sections \
              -Wall -Wpedantic -Wno-format-zero-length \
              -Wno-unused-function -fno-plt \
              -fstack-protector-strong -fstack-clash-protection \
              -D_FORTIFY_SOURCE=3 -D_GNU_SOURCE -fPIE -fno-ident \
              $(if $(filter 1,$(DEBUGSYM)),-g) \
              $(BUILD_FLAGS) $(EXTERNAL_CFLAGS)

STRICT_CFLAGS = -Werror=implicit-function-declaration -Werror=implicit-int \
                -Werror=incompatible-pointer-types -Werror=return-type \
                -Wformat-truncation=2 -Werror=format-truncation \
                -Werror=unused-function

COMMON_LIBS = -lcurl -lSDL2 -lSDL2_mixer -lSDL2_ttf -lSDL2_image -lpthread -lpng -lm

BIN_LDFLAGS  = -Wl,--gc-sections -pie -Wl,-z,relro,-z,now \
               -Wl,--enable-new-dtags,-rpath,'$$ORIGIN/lib' \
               $(if $(filter 1,$(DEBUGSYM)),,-s)
LIB_LDFLAGS  = -Wl,-z,relro,-z,now \
               -Wl,--enable-new-dtags,-rpath,'$$ORIGIN'
CLOSED_LIB_LDFLAGS = $(LIB_LDFLAGS) -Wl,-z,defs
STAGE_LDFLAGS = -Wl,-z,relro,-z,now \
                -Wl,--enable-new-dtags,-rpath,'$$ORIGIN'

SHARED_PIC = -shared -fPIC
