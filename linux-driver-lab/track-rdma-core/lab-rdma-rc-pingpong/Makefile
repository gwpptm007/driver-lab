APP := build/rdma-rc-pingpong
SRC := src/main.c
PC := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PL := $(shell pkg-config --libs libibverbs 2>/dev/null)
CPPFLAGS += -Iinclude $(PC)
CFLAGS += -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PL)),$(PL),-libverbs)
.PHONY: all test clean
all: $(APP)
$(APP): $(SRC) include/pingpong.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)
build:
	mkdir -p $@
test: all
	bash tests/pingpong_test.sh
clean:
	rm -rf build
