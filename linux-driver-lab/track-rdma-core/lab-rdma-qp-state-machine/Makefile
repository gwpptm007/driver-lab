APP := build/rdma-qp-state
SRC := src/main.c src/rdma_env.c src/qp_state.c
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
PC := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PL := $(shell pkg-config --libs libibverbs 2>/dev/null)
CPPFLAGS += -Iinclude $(PC)
CFLAGS += -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PL)),$(PL),-libverbs)
.PHONY: all test clean
all: $(APP)
$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LDLIBS)
build/%.o: src/%.c include/qp_lab.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
build:
	mkdir -p $@
test: all
	bash tests/qp_state_test.sh
clean:
	rm -rf build
