SERVER := build/rdma-rc-server
CLIENT := build/rdma-rc-client
COMMON_SRC := src/common.c src/control_plane.c src/rdma_context.c
SERVER_SRC := src/server.c $(COMMON_SRC)
CLIENT_SRC := src/client.c $(COMMON_SRC)
PC := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PL := $(shell pkg-config --libs libibverbs 2>/dev/null)

CPPFLAGS += -Iinclude $(PC)
CFLAGS += -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PL)),$(PL),-libverbs)

.PHONY: all test envcheck dual-server dual-client clean

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC) include/rdma_cs.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDLIBS)

$(CLIENT): $(CLIENT_SRC) include/rdma_cs.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDLIBS)

build:
	mkdir -p $@

test: all
	bash tests/client_server_test.sh

envcheck: all
	bash tests/check_env.sh

dual-server: all
	bash tests/dual_server_capture.sh

dual-client: all
	bash tests/dual_client_run.sh

clean:
	rm -rf build tests/*.log
