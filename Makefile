ifneq ($(filter contract-check protocol-check cli-check schema-check runtime-check interface-check release-check,$(MAKECMDGOALS)),)
DEVICE ?= NATIVE
endif

include common.mk

HOSTCC ?= cc

BIN_DIR = ./bin
LIB_DIR = $(BIN_DIR)/lib

MODULE_DIR = module
MODULES = mubattery mucredits mufbset muhotkey mulog mulookup murgb musplash muwarn muxcharge muxfrontend muxmessage muremap

DEPENDENCIES = plutosvg common lvgl module

CFLAGS = $(BASE_CFLAGS) $(STRICT_CFLAGS)

INCLUDES = -I./module/ui -I./common \
           -I./common/input -I./common/json \
           -I./common/mini -I./common/miniz

LDLIBS = -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

LDFLAGS = $(COMMON_LIBS) $(BIN_LDFLAGS)

.PHONY: all $(MODULES) prebuild clean notify info contract-check release-check protocol-check cli-check schema-check runtime-check interface-check

all: info prebuild $(MODULES) clean notify

$(MODULES): | prebuild
clean notify: | $(MODULES)

info:
	@echo "======== muOS Frontend Builder ========"
	@echo "Targeting: $(DEVICE)"
	@echo "Modules: $(MODULES)"
	@echo "Dependencies: $(DEPENDENCIES)"

contract-check:
	$(VERBOSE)./gen_contract.sh --check

protocol-check:
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror -Icommon \
		tests/task_protocol_test.c common/task_protocol.c -o /tmp/muos-task-protocol-test
	$(VERBOSE)/tmp/muos-task-protocol-test
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/netplay_wire_test.c retro/netplay/wire.c -o /tmp/muos-netplay-wire-test
	$(VERBOSE)/tmp/muos-netplay-wire-test
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/cheevo_cache_key_test.c retro/cheevo/cache_key.c -o /tmp/muos-cheevo-cache-key-test
	$(VERBOSE)/tmp/muos-cheevo-cache-key-test
	$(VERBOSE)rm -f /tmp/muos-task-protocol-test
	$(VERBOSE)rm -f /tmp/muos-netplay-wire-test /tmp/muos-cheevo-cache-key-test

cli-check:
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/startup_options_test.c retro/core/startup.c retro/netplay/address.c \
		-o /tmp/muos-startup-options-test
	$(VERBOSE)/tmp/muos-startup-options-test
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/message_args_test.c common/message_args.c -o /tmp/muos-message-args-test
	$(VERBOSE)/tmp/muos-message-args-test
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/lookup_args_test.c common/lookup_args.c -o /tmp/muos-lookup-args-test
	$(VERBOSE)/tmp/muos-lookup-args-test
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/fbset_args_test.c common/fbset_args.c -o /tmp/muos-fbset-args-test
	$(VERBOSE)/tmp/muos-fbset-args-test
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/rgb_args_test.c common/rgb_args.c -o /tmp/muos-rgb-args-test
	$(VERBOSE)/tmp/muos-rgb-args-test
	$(VERBOSE)rm -f /tmp/muos-startup-options-test
	$(VERBOSE)rm -f /tmp/muos-message-args-test /tmp/muos-lookup-args-test
	$(VERBOSE)rm -f /tmp/muos-fbset-args-test /tmp/muos-rgb-args-test

schema-check:
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		tests/config_value_test.c common/config_value.c -o /tmp/muos-config-value-test
	$(VERBOSE)/tmp/muos-config-value-test
	$(VERBOSE)rm -f /tmp/muos-config-value-test

runtime-check:
	$(VERBOSE)$(HOSTCC) -std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L \
		tests/power_protocol_test.c retro/core/power_protocol.c -o /tmp/muos-power-protocol-test
	$(VERBOSE)/tmp/muos-power-protocol-test
	$(VERBOSE)rm -f /tmp/muos-power-protocol-test

interface-check: protocol-check cli-check schema-check runtime-check

release-check: contract-check
	$(VERBOSE)./check_release_contract.sh

prebuild: contract-check
	$(VERBOSE)rm -rf $(BIN_DIR)
	$(VERBOSE)find . -name "*.o" -not -path "./.git/*" -exec rm -f {} +
	@echo "Building Stage Overlay: libmustage.so"
	$(VERBOSE)$(MAKE) -C stage DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1
	$(VERBOSE)for DEP in $(DEPENDENCIES); do \
		echo "Building Dependency: $$DEP"; \
		$(MAKE) -C $$DEP DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1; \
	done
	@echo "Building Libretro Host: muxretro"
	$(VERBOSE)$(MAKE) -C retro DEVICE="$(DEVICE)" DEBUG="$(DEBUG)" $(QUIET) || exit 1

clean:
	$(VERBOSE)rm -rf .build_count
	$(VERBOSE)find ./$(MODULE_DIR) -name "*.o" -exec rm -f {} +


%.o: $(MODULE_DIR)/%.c
	@echo "Compiling $< to $@"
	$(VERBOSE)$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) -c $< -o $@ $(QUIET)

$(MODULES):
	@echo "Building Module: $@"
	$(VERBOSE)UI_FILE="$(MODULE_DIR)/ui/ui_$@.c"; \
	UI_OBJ="$(MODULE_DIR)/ui/ui_$@.o"; \
	if [ -f "$$UI_FILE" ]; then \
		rm -f "$$UI_OBJ"; \
		$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) -c "$$UI_FILE" -o "$$UI_OBJ" $(QUIET) || { echo "Error building UI object"; exit 1; }; \
	else \
		UI_OBJ=""; \
	fi; \
	$(CC) -D$(DEVICE) $(CFLAGS) $(INCLUDES) $(MODULE_DIR)/$@.c $$UI_OBJ -o $@ $(LDLIBS) $(LDFLAGS) $(QUIET) || { echo "Error building $@"; exit 1; }; \
	mkdir -p $(BIN_DIR); mv $@ $(BIN_DIR) || { echo "Error moving $@ to $(BIN_DIR)"; exit 1; }

notify:
	@printf "Compiled %d Modules\n============== Complete! ==============\n" "$(words $(MODULES))"
