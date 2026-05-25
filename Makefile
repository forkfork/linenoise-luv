OS := $(shell uname)
LINENOISE_DIR := deps

LUA_INCDIR ?= $(shell pkg-config --cflags lua5.4 2>/dev/null || echo "-I/opt/homebrew/include/lua")

CFLAGS := -O2 -Wall $(LUA_INCDIR) -I$(LINENOISE_DIR)
OBJECTS := linenoise.o linenoise_upstream.o

ifeq ($(OS),Darwin)
TARGET := linenoise_luv.so
LDFLAGS := -bundle -undefined dynamic_lookup
else
CFLAGS += -fPIC
TARGET := linenoise_luv.so
LDFLAGS := -shared
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(LDFLAGS) $^

linenoise.o: linenoise.c $(LINENOISE_DIR)/linenoise.h
	$(CC) $(CFLAGS) -c -o $@ $<

linenoise_upstream.o: $(LINENOISE_DIR)/linenoise.c $(LINENOISE_DIR)/linenoise.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET)

.PHONY: all clean
