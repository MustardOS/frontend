MODULES := muxapp muxarchive muxassign muxcharge muxconfig muxcredits muxgov muxinfo \
           muxlanguage muxlaunch muxnetprofile muxnetscan muxnetwork muxoption muxpass \
           muxpower muxplore muxrtc muxsplash muxstart muxstorage muxsysinfo muxtask \
           muxtester muxtheme muxtimezone muxtweakadv muxtweakgen muxvisual muxwebserv \
           muhotkey muplay

BUILD_TOTAL := $(words $(MODULES))
BUILD_FILE := .build_count
DEVICE ?= $(error DEVICE not specified)

.PHONY: all $(MODULES) clean notify build

MAKEFLAGS += --no-print-directory

all: info clean $(MODULES) notify

info:
	$(info ======== muOS Frontend Builder ========)
	$(info Targeting: $(DEVICE))
	$(info Modules: $(MODULES))

clean:
	@rm -rf bin/mu* .build_count

$(MODULES):
	@if [ -f "$@/Makefile" ]; then \
		MAKE_JOBS=$$(nproc); \
		$(MAKE) -j$$MAKE_JOBS -C $@ DEVICE=$(DEVICE) && \
		BUILD_COUNT=$$(cat $(BUILD_FILE) 2>/dev/null || echo 0); \
		echo $$((BUILD_COUNT + 1)) > $(BUILD_FILE); \
	fi

notify:
	@BUILD_COUNT=$$(cat $(BUILD_FILE) 2>/dev/null || echo 0); \
	printf "Compiled $$BUILD_COUNT of $(BUILD_TOTAL) Modules\n============== Complete! ==============\a\n"; \
	notify-send "muOS Frontend Builder" "Compiled $$BUILD_COUNT of $(BUILD_TOTAL) Modules" 2>/dev/null || true
	@rm -rf $(BUILD_FILE)
