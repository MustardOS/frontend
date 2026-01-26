include ../common.mk

CFLAGS = $(BASE_CFLAGS) \
         -I./ui -I../font -I../lookup -I../common \
         -I../common/img -I../common/input -I../common/json \
         -I../common/mini -I../common/miniz

LDFLAGS = $(COMMON_LIBS) $(LIB_LDFLAGS) $(SHARED_PIC) \
          -L../bin/lib -lui -llookup -lmuxcom \
          -lnotosans_big -lnotosans_big_hd \
          -lnotosans_medium -lnotosans_sc_medium -lnotosans_tc_medium \
          -lnotosans_jp_medium -lnotosans_kr_medium -lnotosans_ar_medium

MODULES = muxactivity muxapp muxappcon muxarchive muxassign muxbackup muxchrony \
          muxcollect muxconfig muxconnect muxcontrol muxcustom muxdanger muxdevice \
          muxdownload muxgov muxhdmi muxhistory muxinfo muxinstall muxkiosk muxlanguage \
          muxlaunch muxnetadv muxnetinfo muxnetprofile muxnetscan muxnetwork \
          muxnews muxoption muxoverlay muxpass muxpicker muxplore muxpower muxraopt \
          muxrtc muxsearch muxshot muxspace muxsplash muxstorage muxsysinfo muxtag \
          muxtask muxtheme muxthemedown muxthemefilter muxtester muxtext \
          muxtimezone muxtweakadv muxtweakgen muxvisual muxwebserv muxshare

MODULE_DIR = .

SRCS := $(foreach mod,$(MODULES), \
    $(wildcard $(MODULE_DIR)/$(mod).c) \
    $(wildcard $(MODULE_DIR)/ui/ui_$(mod).c) \
)

OBJS = $(SRCS:.c=.o)
LIB_DIR = ${shell pwd}/../bin/lib
LIB_FILE = libmuxmod.so

all: $(LIB_DIR)/$(LIB_FILE)

$(LIB_DIR)/$(LIB_FILE): $(OBJS)
	@printf "Linking objects into %s\n" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(OBJS) $(LDFLAGS) -o $@
	@find ${shell pwd} -name "*.o" -exec rm -f {} +

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS)
