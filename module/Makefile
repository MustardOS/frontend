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

MUXLIB = $(CFLAGS) -I./ui -I../font -I../lookup -I../common \
         -I../common/img -I../common/input -I../common/json \
         -I../common/mini -I../common/miniz

LDFLAGS = $(MUXLIB) -L../bin/lib -lui -llookup -lmux \
          -lnotosans_big -lnotosans_big_hd \
          -lnotosans_medium -lnotosans_sc_medium -lnotosans_tc_medium \
          -lnotosans_jp_medium -lnotosans_kr_medium -lnotosans_ar_medium \
          -lSDL2 -lSDL2_mixer -Wl,--gc-sections -s -Wl,-rpath,'./lib'

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fmerge-all-constants -fno-ident -ffast-math -funroll-loops -falign-functions

MODULES = muxapp muxarchive muxassign muxbackup muxcollect muxconfig \
          muxconnect muxcustom muxdanger muxdevice muxgov muxhdmi \
          muxhistory muxinfo muxkiosk muxlanguage muxlaunch muxnetinfo \
          muxnetprofile muxnetscan muxnetwork muxoption muxpass muxpicker \
          muxpower muxplore muxrtc muxsearch muxshot muxspace muxsplash \
          muxstorage muxsysinfo muxtag muxtask muxtester muxtext muxtimezone \
          muxtweakadv muxtweakgen muxvisual muxwebserv muxshare
MODULE_DIR = .

SRCS := $(foreach mod,$(MODULES), \
    $(wildcard $(MODULE_DIR)/$(mod).c) \
    $(wildcard $(MODULE_DIR)/ui/ui_$(mod).c) \
)

OBJS = $(SRCS:.c=.o)
LIB_DIR = ${shell pwd}/../bin/lib
LIB_FILE = libmuxmodule.so

all: $(LIB_DIR)/$(LIB_FILE)

$(LIB_DIR)/$(LIB_FILE): $(OBJS)
	@printf "Linking objects into %s\n" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(OBJS) $(EXTRA) -shared -o $@

	@find ${shell pwd} -name "*.o" -exec rm -f {} +

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS)
