SHELL = /bin/sh

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

MUXLIB = -shared $(CFLAGS) -I./ui -I../font -I../lookup -I../common \
         -I../common/img -I../common/input -I../common/json \
         -I../common/mini -I../common/miniz

LDFLAGS = $(MUXLIB) -L../bin/lib -lui -llookup -lmux \
          -lnotosans_big -lnotosans_big_hd \
          -lnotosans_medium -lnotosans_sc_medium -lnotosans_tc_medium \
          -lnotosans_jp_medium -lnotosans_kr_medium -lnotosans_ar_medium \
          -lSDL2 -lSDL2_mixer -Wl,--gc-sections -s -Wl,-rpath,'./lib'

MODULES = muxactivity muxapp muxappcon muxarchive muxassign muxbackup muxcollect \
          muxconfig muxconnect muxcontrol muxcustom muxdanger muxdevice muxdownload \
          muxgov muxhdmi muxhistory muxinfo muxinstall muxkiosk muxlanguage \
          muxlaunch muxnetadv muxnetinfo muxnetprofile muxnetscan muxnetwork \
          muxoption muxpass muxpicker muxpower muxplore muxrtc muxsearch \
          muxshot muxspace muxsplash muxstorage muxsysinfo muxtag muxtask \
          muxtheme muxthemedown muxthemefilter muxtester muxtext muxtimezone \
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
	@$(CC) $(OBJS) $(LDFLAGS) -o $@

	@find ${shell pwd} -name "*.o" -exec rm -f {} +

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS)
