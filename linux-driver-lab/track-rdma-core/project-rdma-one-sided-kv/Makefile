SERVER := build/rdma-kv-server
CLIENT := build/rdma-kv-client
COMMON_SRC := src/common.c src/control_plane.c src/rdma_context.c
SERVER_SRC := src/kv_server.c $(COMMON_SRC)
CLIENT_SRC := src/kv_client.c $(COMMON_SRC)
PC := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PL := $(shell pkg-config --libs libibverbs 2>/dev/null)

CPPFLAGS += -Iinclude $(PC)
CFLAGS += -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PL)),$(PL),-libverbs)

.PHONY: all test envcheck clean

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC) include/rdma_cs.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDLIBS)

$(CLIENT): $(CLIENT_SRC) include/rdma_cs.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDLIBS)

build:
	mkdir -p $@

test: all
	bash tests/kv_smoke_test.sh

envcheck: all
	bash tests/check_env.sh

clean:
	rm -rf build tests/*.log
