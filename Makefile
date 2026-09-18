CC := clang
CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic
OPT ?= -O2
DBG ?= -g

CFLAGS ?= $(CSTD) $(WARN) $(OPT) $(DBG)
CPPFLAGS ?=
LDFLAGS ?=

TARGET := 6502test
SRC_DIR := src
INCLUDE_DIR := include
LIB_DIR := lib

SRCS := $(SRC_DIR)/6502test.c $(SRC_DIR)/6502cpu.c $(SRC_DIR)/6502memory.c $(SRC_DIR)/6502framebuffer.c $(SRC_DIR)/6502machine.c $(SRC_DIR)/6502debugger.c $(SRC_DIR)/6502io.c
OBJS := $(SRCS:.c=.o)

ifeq ($(OS),Windows_NT)
TARGET := $(TARGET).exe
CPPFLAGS += -I$(SRC_DIR) -I$(INCLUDE_DIR)/curses
LDFLAGS += -L$(LIB_DIR)
CURSES_LIBS ?= $(LIB_DIR)/pdcurses.a
CLEAN_CMD := powershell -NoProfile -Command "$$items='$(OBJS) $(TARGET)'.Split(' '); foreach ($$i in $$items) { if ($$i) { Remove-Item -Force -ErrorAction SilentlyContinue $$i } }"
else
CPPFLAGS += -I$(SRC_DIR)
CURSES_LIBS ?= -lncurses
CLEAN_CMD := rm -f $(OBJS) $(TARGET)
endif

LDLIBS ?= $(CURSES_LIBS)

.PHONY: all clean run debug release conway headless

# CPU validation/benchmark build without a curses dependency.
headless:
	$(CC) -I$(SRC_DIR) $(CFLAGS) -o 6502headless$(if $(filter Windows_NT,$(OS)),.exe,) $(SRC_DIR)/6502test.c $(SRC_DIR)/6502cpu.c $(SRC_DIR)/6502memory.c $(SRC_DIR)/6502framebuffer.c $(SRC_DIR)/6502machine.c $(SRC_DIR)/6502debugger.c tools/headless_io.c

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

programs/%.bin: programs/%.asm tools/asm6502.py
	python tools/asm6502.py $< $@

conway: programs/conway_life_6502.bin
	./$(TARGET) $< 0xD000

# Keep symbols and disable optimizations to ease stepping in debuggers.
debug: CFLAGS := $(CSTD) $(WARN) -O0 -g

debug: clean all

# Optimized build profile.
release: CFLAGS := $(CSTD) $(WARN) -O3

release: clean all

clean:
	$(CLEAN_CMD)


