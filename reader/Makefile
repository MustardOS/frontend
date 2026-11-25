include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/module/muxreader

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -I. -I.. -I../module -I../module/ui -I../vendor
LDFLAGS := $(COMMON_LIBS) -lcurl -lcrypto $(MUPDF_LIBS) $(EXTERNAL_HIDE) $(BIN_LDFLAGS)
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

SRC := main.c menu.c settings.c view.c engine/document.c
OBJ := $(SRC:.c=.o) filter_assets.o filter_catalogue.o

FILTER_CFLAGS := $(CFLAGS) -DWASABI_SHARE_PATH='RUN_STORAGE_PATH "chutney/"'

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $(TARGET))
	$(CC) $(OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

DEPDIR := $(DEP_ROOT)/reader
DEPS   := $(foreach O,$(OBJ),$(call DEP_NAME,reader,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,reader,$*)

filter_assets.o: ../video/settings/assets.c
	$(CC) $(FILTER_CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,reader,filter_assets)

filter_catalogue.o: ../video/settings/catalogue.c
	$(CC) $(FILTER_CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,reader,filter_catalogue)

clean:
	rm -f $(OBJ)
	rm -rf $(DEPDIR)
