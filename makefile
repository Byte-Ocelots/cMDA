# cMDA Makefile
# Supports: Linux, macOS, Windows (MinGW/MSYS2), AVR, ESP32, ESP8266, STM32

CC      ?= gcc
# Note: -march=native removed. SIMD is detected at runtime.
# __attribute__((target(...))) in funcs.c handles per-function ISA compilation.
CFLAGS  = -Iinclude -Wall -Wextra -std=c99 -O2
_ARCH   =

ifeq ($(arch),32)
  CFLAGS += -m32
  _ARCH   = -m32
endif
ifeq ($(arch),64)
  CFLAGS += -m64
  _ARCH   = -m64
endif

# ── platform detection ──────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
  SHARED_EXT   = dll
  SHARED_FILE  = cMDA.dll
  SHARED_FLAGS = -shared
  INSTALL_DIR ?= $(shell echo %windir:~0,2%)/Byte-Ocelots
  ifeq ($(findstring bash,$(shell echo $$SHELL)),bash)
    RM=rm -f
    CP=cp -r
    MV=mv
    MKDIR=mkdir -p
    SEP=/
  else
    RM=del /Q /F
    CP=xcopy /Y /E /I
    MV=move
    MKDIR=if not exist $@ mkdir $@
    SEP=\\
  endif
else
  RM    = rm -f
  CP    = cp
  MV    = mv
  SEP   = /
  ifeq ($(shell uname),Darwin)
    SHARED_EXT  = dylib
    SHARED_FILE = cMDA.dylib
    SHARED_FLAGS = -dynamiclib
    INSTALL_DIR ?= /usr/local/Byte-Ocelots
    MKDIR = mkdir -p
  else
    SHARED_EXT  = so
    SHARED_FILE = cMDA.so
    SHARED_FLAGS = -shared
    PREFIX      ?= /usr/local
    LIN_BIN_DIR  = $(PREFIX)/bin
    LIN_LIB_DIR  = $(PREFIX)/lib
    LIN_INC_DIR  = $(PREFIX)/include/cMDA
    INSTALL_DIR ?= $(PREFIX)
    MKDIR = mkdir -p
  endif
endif

INCLUDE_DEPS = 1

# ── directories ─────────────────────────────────────────────────────
SRC_MD_DIR   = src/md
SRC_CMDA_DIR = src/cmda
TEST_DIR     = tests
BIN_DIR      = bin
BUILD_DIR    = build
LIB_DIR      = lib

# ── source / object / binary lists ──────────────────────────────────
SRC_MD_MD_FILES    = $(wildcard $(SRC_MD_DIR)/md*.c)
MD_MD_OBJ_FILES    = $(patsubst $(SRC_MD_DIR)/%.c,$(BUILD_DIR)/bin_%.o,$(SRC_MD_MD_FILES))
SRC_MD_OTHER_FILES = $(filter-out $(SRC_MD_DIR)/md%.c,$(wildcard $(SRC_MD_DIR)/*.c))
MD_OTHER_OBJ_FILES = $(patsubst $(SRC_MD_DIR)/%.c,$(BUILD_DIR)/bin_%.o,$(SRC_MD_OTHER_FILES))
MD_BIN_FILES       = $(patsubst $(SRC_MD_DIR)/%.c,$(BIN_DIR)/%,$(SRC_MD_MD_FILES))
SRC_CMDA_FILES     = $(wildcard $(SRC_CMDA_DIR)/*.c)
CMDA_OBJ_FILES     = $(patsubst $(SRC_CMDA_DIR)/%.c,$(BUILD_DIR)/lib_%.o,$(SRC_CMDA_FILES))
TEST_FILES         = $(wildcard $(TEST_DIR)/*.c)
TEST_BIN_FILES     = $(patsubst $(TEST_DIR)/%.c,$(TEST_DIR)/bin/%,$(TEST_FILES))
STATIC_LIB         = $(LIB_DIR)/libcMDA.a
SHARED_LIB         = $(LIB_DIR)/$(SHARED_FILE)

# ── default target ───────────────────────────────────────────────────
all: static shared bins tests
all-c: all clean-o

# ── directory setup ──────────────────────────────────────────────────
$(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) $(TEST_DIR)/bin:
ifeq ($(OS),Windows_NT)
ifeq ($(findstring bash,$(shell echo $$SHELL)),bash)
	mkdir -p $@
else
	if not exist $@ mkdir $@
endif
else
	mkdir -p $@
endif

# ── object rules ─────────────────────────────────────────────────────
$(BUILD_DIR)/lib_%.o: $(SRC_CMDA_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(BUILD_DIR)/bin_%.o: $(SRC_MD_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── static library ───────────────────────────────────────────────────
static: $(STATIC_LIB)
$(STATIC_LIB): $(CMDA_OBJ_FILES) | $(LIB_DIR)
	ar rcs $@ $(CMDA_OBJ_FILES)

# Fragment static libs (single-algo)
lib-md2: $(LIB_DIR)/libcMD2.a
lib-md4: $(LIB_DIR)/libcMD4.a
lib-md5: $(LIB_DIR)/libcMD5.a
$(LIB_DIR)/libcMD2.a: $(BUILD_DIR)/lib_rfc1319.o | $(LIB_DIR)
	ar rcs $@ $^
$(LIB_DIR)/libcMD4.a: $(BUILD_DIR)/lib_rfc1320.o $(BUILD_DIR)/lib_funcs.o | $(LIB_DIR)
	ar rcs $@ $^
$(LIB_DIR)/libcMD5.a: $(BUILD_DIR)/lib_rfc1321.o $(BUILD_DIR)/lib_funcs.o | $(LIB_DIR)
	ar rcs $@ $^

# ── shared library ───────────────────────────────────────────────────
shared: $(SHARED_LIB)
$(SHARED_LIB): $(CMDA_OBJ_FILES) | $(LIB_DIR)
	$(CC) $(SHARED_FLAGS) $(_ARCH) -o $@ $(CMDA_OBJ_FILES) -lm

# Fragment shared libs (single-algo)
shared-md2: $(LIB_DIR)/libcMD2.$(SHARED_EXT)
shared-md4: $(LIB_DIR)/libcMD4.$(SHARED_EXT)
shared-md5: $(LIB_DIR)/libcMD5.$(SHARED_EXT)
$(LIB_DIR)/libcMD2.$(SHARED_EXT): $(BUILD_DIR)/lib_rfc1319.o | $(LIB_DIR)
	$(CC) $(SHARED_FLAGS) $(_ARCH) -o $@ $^ -lm
$(LIB_DIR)/libcMD4.$(SHARED_EXT): $(BUILD_DIR)/lib_rfc1320.o $(BUILD_DIR)/lib_funcs.o | $(LIB_DIR)
	$(CC) $(SHARED_FLAGS) $(_ARCH) -o $@ $^ -lm
$(LIB_DIR)/libcMD5.$(SHARED_EXT): $(BUILD_DIR)/lib_rfc1321.o $(BUILD_DIR)/lib_funcs.o | $(LIB_DIR)
	$(CC) $(SHARED_FLAGS) $(_ARCH) -o $@ $^ -lm

# ── CLI binaries ─────────────────────────────────────────────────────
bins: $(MD_BIN_FILES)
$(BIN_DIR)/%: $(BUILD_DIR)/bin_%.o $(MD_OTHER_OBJ_FILES) $(STATIC_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -L$(LIB_DIR) -o $@ $< $(MD_OTHER_OBJ_FILES) -lcMDA -lm

# ── tests ────────────────────────────────────────────────────────────
tests: static $(TEST_BIN_FILES)
$(TEST_DIR)/bin/%: $(TEST_DIR)/%.c | $(TEST_DIR)/bin
	$(CC) $(CFLAGS) -o $@ $< -L$(LIB_DIR) -lcMDA -lm

# ── embedded cross-compile targets ───────────────────────────────────
# Usage: make avr          (produces lib/libcMDA_avr.a)
#        make esp32        (produces lib/libcMDA_esp32.a)
#        make esp8266      (produces lib/libcMDA_esp8266.a)
#        make stm32        (produces lib/libcMDA_stm32.a, Cortex-M4)
# Override MCU with e.g.: make avr AVR_MCU=attiny85

AVR_CC       ?= avr-gcc
AVR_MCU      ?= atmega328p
AVR_CFLAGS    = -Iinclude -mmcu=$(AVR_MCU) -Os -DCMDA_NO_SIMD -std=c99 -Wall

ESP32_CC     ?= xtensa-esp32-elf-gcc
ESP32_CFLAGS  = -Iinclude -Os -DCMDA_NO_SIMD -std=c99 -mlongcalls -Wall

ESP8266_CC   ?= xtensa-lx106-elf-gcc
ESP8266_CFLAGS= -Iinclude -Os -DCMDA_NO_SIMD -std=c99 -mlongcalls -Wall

STM32_CC     ?= arm-none-eabi-gcc
STM32_MCU    ?= cortex-m4
STM32_CFLAGS  = -Iinclude -mcpu=$(STM32_MCU) -mthumb -Os -DCMDA_NO_SIMD -std=c99 -Wall

EMBED_SRCS = $(SRC_CMDA_DIR)/rfc1319.c $(SRC_CMDA_DIR)/rfc1320.c \
             $(SRC_CMDA_DIR)/rfc1321.c $(SRC_CMDA_DIR)/funcs.c

avr: | $(BUILD_DIR) $(LIB_DIR)
	$(AVR_CC) $(AVR_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1319.c -o $(BUILD_DIR)/avr_rfc1319.o
	$(AVR_CC) $(AVR_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1320.c -o $(BUILD_DIR)/avr_rfc1320.o
	$(AVR_CC) $(AVR_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1321.c -o $(BUILD_DIR)/avr_rfc1321.o
	$(AVR_CC) $(AVR_CFLAGS) -c $(SRC_CMDA_DIR)/funcs.c   -o $(BUILD_DIR)/avr_funcs.o
	ar rcs $(LIB_DIR)/libcMDA_avr.a $(BUILD_DIR)/avr_rfc1319.o $(BUILD_DIR)/avr_rfc1320.o \
	       $(BUILD_DIR)/avr_rfc1321.o $(BUILD_DIR)/avr_funcs.o
	@echo "Built: $(LIB_DIR)/libcMDA_avr.a"

esp32: | $(BUILD_DIR) $(LIB_DIR)
	$(ESP32_CC) $(ESP32_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1319.c -o $(BUILD_DIR)/esp32_rfc1319.o
	$(ESP32_CC) $(ESP32_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1320.c -o $(BUILD_DIR)/esp32_rfc1320.o
	$(ESP32_CC) $(ESP32_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1321.c -o $(BUILD_DIR)/esp32_rfc1321.o
	$(ESP32_CC) $(ESP32_CFLAGS) -c $(SRC_CMDA_DIR)/funcs.c   -o $(BUILD_DIR)/esp32_funcs.o
	ar rcs $(LIB_DIR)/libcMDA_esp32.a $(BUILD_DIR)/esp32_rfc1319.o $(BUILD_DIR)/esp32_rfc1320.o \
	       $(BUILD_DIR)/esp32_rfc1321.o $(BUILD_DIR)/esp32_funcs.o
	@echo "Built: $(LIB_DIR)/libcMDA_esp32.a"

esp8266: | $(BUILD_DIR) $(LIB_DIR)
	$(ESP8266_CC) $(ESP8266_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1319.c -o $(BUILD_DIR)/esp8266_rfc1319.o
	$(ESP8266_CC) $(ESP8266_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1320.c -o $(BUILD_DIR)/esp8266_rfc1320.o
	$(ESP8266_CC) $(ESP8266_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1321.c -o $(BUILD_DIR)/esp8266_rfc1321.o
	$(ESP8266_CC) $(ESP8266_CFLAGS) -c $(SRC_CMDA_DIR)/funcs.c   -o $(BUILD_DIR)/esp8266_funcs.o
	ar rcs $(LIB_DIR)/libcMDA_esp8266.a $(BUILD_DIR)/esp8266_rfc1319.o $(BUILD_DIR)/esp8266_rfc1320.o \
	       $(BUILD_DIR)/esp8266_rfc1321.o $(BUILD_DIR)/esp8266_funcs.o
	@echo "Built: $(LIB_DIR)/libcMDA_esp8266.a"

stm32: | $(BUILD_DIR) $(LIB_DIR)
	$(STM32_CC) $(STM32_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1319.c -o $(BUILD_DIR)/stm32_rfc1319.o
	$(STM32_CC) $(STM32_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1320.c -o $(BUILD_DIR)/stm32_rfc1320.o
	$(STM32_CC) $(STM32_CFLAGS) -c $(SRC_CMDA_DIR)/rfc1321.c -o $(BUILD_DIR)/stm32_rfc1321.o
	$(STM32_CC) $(STM32_CFLAGS) -c $(SRC_CMDA_DIR)/funcs.c   -o $(BUILD_DIR)/stm32_funcs.o
	ar rcs $(LIB_DIR)/libcMDA_stm32.a $(BUILD_DIR)/stm32_rfc1319.o $(BUILD_DIR)/stm32_rfc1320.o \
	       $(BUILD_DIR)/stm32_rfc1321.o $(BUILD_DIR)/stm32_funcs.o
	@echo "Built: $(LIB_DIR)/libcMDA_stm32.a"

# ── clean ────────────────────────────────────────────────────────────
clean-bin:
	$(RM) $(subst /,$(SEP),$(BIN_DIR)/*)
clean-tests:
	$(RM) $(subst /,$(SEP),$(TEST_DIR)/bin/*)
clean-o:
	$(RM) $(subst /,$(SEP),$(BUILD_DIR)/*.o)
clean-static:
	$(RM) $(subst /,$(SEP),$(LIB_DIR)/libcMDA.a $(LIB_DIR)/libcMD2.a $(LIB_DIR)/libcMD4.a $(LIB_DIR)/libcMD5.a)
clean-shared:
	$(RM) $(subst /,$(SEP),$(SHARED_LIB))
clean: clean-bin clean-tests clean-o clean-static clean-shared

# ── install variants ─────────────────────────────────────────────────
_do_install_libs: static shared
ifeq ($(OS),Windows_NT)
	$(MKDIR) "$(INSTALL_DIR)"
	$(CP) lib $(subst /,$(SEP),$(INSTALL_DIR)/lib)
else
  ifeq ($(shell uname),Darwin)
	mkdir -p $(INSTALL_DIR)/lib
	install -m 644 lib/* $(INSTALL_DIR)/lib
  else
	mkdir -p $(LIN_LIB_DIR)
	install -m 644 lib/* $(LIN_LIB_DIR)
  endif
endif

_do_install_headers:
ifeq ($(OS),Windows_NT)
	$(MKDIR) "$(INSTALL_DIR)"
	$(CP) include $(subst /,$(SEP),$(INSTALL_DIR)/include)
else
  ifeq ($(shell uname),Darwin)
	mkdir -p $(INSTALL_DIR)/include/cMDA
	install -m 644 include/cMDA/* $(INSTALL_DIR)/include/cMDA
  else
	mkdir -p $(LIN_INC_DIR)
	install -m 644 include/cMDA/* $(LIN_INC_DIR)
  endif
endif

_do_install_bin: bins
ifeq ($(OS),Windows_NT)
	$(MKDIR) "$(INSTALL_DIR)"
	$(CP) bin $(subst /,$(SEP),$(INSTALL_DIR)/bin)
else
  ifeq ($(shell uname),Darwin)
	mkdir -p $(INSTALL_DIR)/bin
	install -m 755 bin/* $(INSTALL_DIR)/bin
  else
	mkdir -p $(LIN_BIN_DIR)
	install -m 755 bin/* $(LIN_BIN_DIR)
  endif
endif

# Install only libraries + headers (no CLI)
install-libs-only: _do_install_libs _do_install_headers
	@echo "Install (libs only) complete."

# Install only CLI binaries
install-cli-only: _do_install_bin
	@echo "Install (cli only) complete."

# Install everything
install-all: _do_install_libs _do_install_headers _do_install_bin
	@echo "Install (all) complete."

# Alias: default install = all
install: install-all

# ── uninstall ────────────────────────────────────────────────────────
uninstall:
ifeq ($(OS),Windows_NT)
ifeq ($(findstring bash,$(shell echo $$SHELL)),bash)
	rm -rf $(INSTALL_DIR)/bin $(INSTALL_DIR)/lib $(INSTALL_DIR)/include/cMDA
else
	if exist $(subst /,\,$(INSTALL_DIR)/bin) rmdir /s /q $(subst /,\,$(INSTALL_DIR)/bin)
	if exist $(subst /,\,$(INSTALL_DIR)/lib) rmdir /s /q $(subst /,\,$(INSTALL_DIR)/lib)
	if exist $(subst /,\,$(INSTALL_DIR)/include/cMDA) rmdir /s /q $(subst /,\,$(INSTALL_DIR)/include/cMDA)
endif
else
	rm -rf $(INSTALL_DIR)/bin $(INSTALL_DIR)/lib $(INSTALL_DIR)/include/cMDA
endif
	@echo "Uninstall complete."

# ── phony / special ──────────────────────────────────────────────────
.PHONY: all shared bins static tests clean avr esp32 esp8266 stm32 \
        lib-md2 lib-md4 lib-md5 shared-md2 shared-md4 shared-md5 \
        install install-all install-libs-only install-cli-only uninstall
.SECONDARY: $(MD_MD_OBJ_FILES) $(MD_OTHER_OBJ_FILES) $(CMDA_OBJ_FILES) $(STATIC_LIB) $(SHARED_LIB)
.NOTPARALLEL: static shared clean-bin clean-tests clean-o clean-static clean-shared
.IGNORE: clean clean-static clean-shared clean-o clean-bin clean-tests
