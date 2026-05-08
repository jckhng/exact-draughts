CC ?= gcc
PKG_CONFIG ?= pkg-config

CFLAGS += -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags gtk+-2.0 cairo gdk-pixbuf-2.0)
LDLIBS += $(shell $(PKG_CONFIG) --libs gtk+-2.0 cairo gdk-pixbuf-2.0)

OBJS = main.o draughts_engine.o
TEST_OBJS = smoke_test.o draughts_engine.o

.PHONY: all clean

all: exact-draughts

exact-draughts: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

smoke-test: $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(LDLIBS)

clean:
	rm -f $(OBJS) $(TEST_OBJS) exact-draughts smoke-test
