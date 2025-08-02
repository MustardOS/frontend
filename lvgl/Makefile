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

LVGL_DIR_NAME 	?= .
LVGL_DIR 		?= .

CFLAGS = $(ARCH) -flto=auto -ffunction-sections -fdata-sections \
	-flto -finline-functions -Wall -Wno-format-zero-length -fPIC

LDFLAGS = $(CFLAGS) -lSDL2 -lSDL2_mixer -lpthread -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fmerge-all-constants -fno-ident -ffast-math -funroll-loops -falign-functions

BUILD_DIR 		= ./build
BUILD_OBJ_DIR 	= $(BUILD_DIR)/obj
BUILD_BIN_DIR 	= ../bin/lib

include $(LVGL_DIR)/$(LVGL_DIR_NAME)/lvgl.mk

OBJEXT 			?= .o

AOBJS 			= $(ASRCS:.S=$(OBJEXT))
COBJS 			= $(CSRCS:.c=$(OBJEXT))

SRCS 			= $(ASRCS) $(CSRCS)
OBJS 			= $(AOBJS) $(COBJS)
TARGET 			= $(addprefix $(BUILD_OBJ_DIR)/, $(patsubst ./%, %, $(OBJS)))

LIBRARY         = $(BUILD_BIN_DIR)/libui.so

all: $(LIBRARY)

$(BUILD_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(BUILD_OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(LIBRARY): $(TARGET)
	@mkdir -p $(dir $(LIBRARY))
	$(CC) -shared -o $@ $(TARGET) $(EXTRA)
	@echo "Built shared library: $@"
	@rm -rf $(BUILD_DIR)
