TARGET ?= muinput

BUILDDIR ?= build
OBJDIR ?= $(BUILDDIR)/obj
BINDIR ?= $(BUILDDIR)/bin

SOURCE_DIRS := common devices drivers
SRCS := main.c runtime.c $(shell find $(SOURCE_DIRS) -type f -name '*.c' | sort)
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)
DEPS := $(OBJS:.o=.d)

BASE_CFLAGS := -std=c11 -O2 -pipe -flto=auto -ffunction-sections \
	-fdata-sections -fstack-protector-strong -D_FORTIFY_SOURCE=3 \
	-DNDEBUG -Wall -Wextra -Wpedantic
CFLAGS ?= $(BASE_CFLAGS)
CFLAGS += -MMD -MP
LDFLAGS ?= -Wl,--gc-sections -flto=auto
LDLIBS ?= -lm -lrt

.PHONY: all clean check

ifneq ($(filter clean,$(MAKECMDGOALS)),)
.NOTPARALLEL:
endif

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $@

check: $(BINDIR)/$(TARGET)
	$(BINDIR)/$(TARGET) --help >/dev/null
	$(BINDIR)/$(TARGET) --list-devices >/dev/null

clean:
	rm -rf $(BUILDDIR)

-include $(DEPS)
