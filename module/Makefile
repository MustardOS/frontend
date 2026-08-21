include ../common.mk

CFLAGS = $(BASE_CFLAGS) $(STRICT_CFLAGS) \
         -I./ui -I../font -I../common \
         -I../common/img -I../common/input -I../common/json \
         -I../common/mini -I../common/miniz

LDFLAGS = $(COMMON_LIBS) $(LIB_LDFLAGS) $(SHARED_PIC) \
          -L../bin/lib -lui -lmuxcom -lplutosvg

MODULES = muxactivity muxapp muxappcon muxaccess muxdetail muxarchive muxassign muxbackup muxbtall \
          muxbtcon muxbtdev muxcolfilter muxcollect muxconfig muxconnect \
          muxcontrol muxcustom muxdanger muxdevice muxdistemp muxdownload \
          muxgov muxhdmi muxhistory muxinfo muxinstall muxkiosk \
          muxlanguage muxlaunch muxlink muxlogo muxnetadv muxnetprofile muxnetproxy muxnetscan \
          muxnetwork muxnews muxoption muxorder muxoverlay muxpass muxpasscfg muxpicker muxplore \
          muxpower muxraopt muxrgb muxrgbzone muxrtc muxsearch muxshader muxshot muxsort \
          muxspace muxsplash muxstorage muxtag muxtask muxtheme \
          muxthemedown muxthemefilter muxtester muxtext muxtimezone \
          muxtweakadv muxtweakgen muxwebserv muxshare muxremap muxthird

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

DEPDIR := $(DEP_ROOT)/module
DEPS   := $(foreach O,$(OBJS),$(call DEP_NAME,module,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS) -MF $(call DEP_NAME,module,$*)
