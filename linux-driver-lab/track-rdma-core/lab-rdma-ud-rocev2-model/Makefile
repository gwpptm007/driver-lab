APP := build/rdma-ud-demo
PC := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PL := $(shell pkg-config --libs libibverbs 2>/dev/null)
CPPFLAGS += -Iinclude $(PC)
CFLAGS += -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PL)),$(PL),-libverbs)
.PHONY: all test clean
all: $(APP)
$(APP): src/main.c include/ud_lab.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/main.c $(LDLIBS)
build:
	mkdir -p $@
test: all
	bash tests/ud_test.sh
clean:
	rm -rf build
