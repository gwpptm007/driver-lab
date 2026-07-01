APP := build/rdma-mr-lab
SRC := src/main.c src/rdma_device.c src/mr_experiments.c
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
PKG_CFLAGS := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs libibverbs 2>/dev/null)

CPPFLAGS += -Iinclude $(PKG_CFLAGS)
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PKG_LIBS)),$(PKG_LIBS),-libverbs)

.PHONY: all test clean
all: $(APP)

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c include/mr_lab.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p $@

test: all
	bash tests/mr_test.sh

clean:
	rm -rf build
