include ../common.mk

CFLAGS = $(BASE_CFLAGS) \
         -I./ui -I../font -I../lookup -I../common \
         -I../common/img -I../common/input -I../common/json \
         -I../common/mini -I../common/miniz

LDFLAGS = $(COMMON_LIBS) $(LIB_LDFLAGS) $(SHARED_PIC) \
          -L../bin/lib -lui -llookup -lmuxcom -lplutosvg

MODULES = muxactivity muxapp muxappcon muxarchive muxassign muxbackup muxbatinfo muxbtall \
          muxbtcon muxbtdev muxchrony muxcoladjust muxcolfilter muxcollect muxconfig \
          muxconnect muxcontent muxcontrol muxcustom muxdanger muxdevice muxdownload \
          muxfont muxgov muxhdmi muxhistory muxinfo muxinstall muxkiosk \
          muxlanguage muxlaunch muxnetadv muxnetinfo muxnetprofile muxnetscan \
          muxnetwork muxnews muxoption muxoverlay muxpass muxpasscfg muxpicker muxplore \
          muxpower muxraopt muxrgb muxrtc muxsearch muxshader muxshot muxsort \
          muxspace muxsplash muxstorage muxsysinfo muxtag muxtask muxtheme \
          muxthemedown muxthemefilter muxthemeopt muxtester muxtext muxtimezone \
          muxtweakadv muxtweakgen muxvisual muxwebserv muxshare muxremap

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
