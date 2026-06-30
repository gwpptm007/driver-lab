APP := rdma-object-lifecycle
SRC := main.c

PKG_CFLAGS := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs libibverbs 2>/dev/null)

CFLAGS ?= -O2 -g -Wall -Wextra
LDFLAGS ?=
LDLIBS ?=

ifeq ($(strip $(PKG_LIBS)),)
LDLIBS += -libverbs
else
CFLAGS += $(PKG_CFLAGS)
LDLIBS += $(PKG_LIBS)
endif

.PHONY: all clean

all: $(APP)

$(APP): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(APP)
