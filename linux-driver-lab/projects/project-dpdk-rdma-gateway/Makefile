# Phase 1 只构建纯 C contract，自 Phase 2 起再分别链接 libdpdk 与 libibverbs。
CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror
CPPFLAGS ?= -Iinclude
BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/gateway-contract-test
SRCS := src/gateway_contract.c tests/contract_test.c
DPDK_BIN := $(BUILD_DIR)/gateway-dpdk-ingress
DPDK_SRCS := app/gateway_dpdk_ingress.c src/gateway_ingress.c src/gateway_contract.c
DPDK_CFLAGS := $(shell pkg-config --cflags libdpdk 2>/dev/null)
DPDK_LIBS := $(shell pkg-config --libs libdpdk 2>/dev/null)
DPDK_APP_CFLAGS := $(filter-out -std=c11,$(CFLAGS)) -std=gnu11
RDMA_CS_DIR := ../../track-rdma-core/project-rdma-one-sided-kv
RDMA_CS_SRCS := $(RDMA_CS_DIR)/src/common.c \
	$(RDMA_CS_DIR)/src/control_plane.c $(RDMA_CS_DIR)/src/rdma_context.c
RDMA_CFLAGS := $(shell pkg-config --cflags libibverbs 2>/dev/null)
RDMA_LIBS := $(shell pkg-config --libs libibverbs 2>/dev/null)
RDMA_CPPFLAGS := $(CPPFLAGS) -I$(RDMA_CS_DIR)/include $(RDMA_CFLAGS)
RDMA_SERVER := $(BUILD_DIR)/gateway-rdma-server
RDMA_CLIENT := $(BUILD_DIR)/gateway-rdma-client
RDMA_COMMON := src/gateway_rdma_backend.c src/gateway_contract.c $(RDMA_CS_SRCS)
INTEGRATED_SERVER := $(BUILD_DIR)/gateway-integrated-server
INTEGRATED_CLIENT := $(BUILD_DIR)/gateway-integrated-client
INTEGRATED_COMMON := src/gateway_ingress.c src/gateway_rdma_worker.c \
	src/gateway_rdma_backend.c src/gateway_contract.c $(RDMA_CS_SRCS)

.PHONY: all clean test dpdk test-phase2 rdma test-phase3 integrated test-phase4 test-all

all: $(TEST_BIN)

$(TEST_BIN): $(SRCS) include/gateway_contract.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRCS) -o $@

test:
	bash tests/contract_test.sh

dpdk: $(DPDK_BIN)

$(DPDK_BIN): $(DPDK_SRCS) include/gateway_ingress.h include/gateway_contract.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(DPDK_APP_CFLAGS) $(DPDK_CFLAGS) $(DPDK_SRCS) -o $@ $(DPDK_LIBS)

test-phase2:
	bash tests/phase2_ingress_test.sh

rdma: $(RDMA_SERVER) $(RDMA_CLIENT)

$(RDMA_SERVER): app/gateway_rdma_server.c $(RDMA_COMMON) include/gateway_rdma_backend.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(RDMA_CPPFLAGS) $(CFLAGS) app/gateway_rdma_server.c $(RDMA_COMMON) -o $@ $(if $(strip $(RDMA_LIBS)),$(RDMA_LIBS),-libverbs)

$(RDMA_CLIENT): app/gateway_rdma_client.c $(RDMA_COMMON) include/gateway_rdma_backend.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(RDMA_CPPFLAGS) $(CFLAGS) app/gateway_rdma_client.c $(RDMA_COMMON) -o $@ $(if $(strip $(RDMA_LIBS)),$(RDMA_LIBS),-libverbs)

test-phase3:
	bash tests/phase3_rdma_test.sh

integrated: $(INTEGRATED_SERVER) $(INTEGRATED_CLIENT)

$(INTEGRATED_SERVER): app/gateway_integrated_server.c $(RDMA_COMMON)
	mkdir -p $(BUILD_DIR)
	$(CC) $(RDMA_CPPFLAGS) $(CFLAGS) app/gateway_integrated_server.c $(RDMA_COMMON) -o $@ $(if $(strip $(RDMA_LIBS)),$(RDMA_LIBS),-libverbs)

$(INTEGRATED_CLIENT): app/gateway_integrated_client.c $(INTEGRATED_COMMON)
	mkdir -p $(BUILD_DIR)
	$(CC) $(RDMA_CPPFLAGS) $(DPDK_APP_CFLAGS) $(DPDK_CFLAGS) app/gateway_integrated_client.c $(INTEGRATED_COMMON) -o $@ $(DPDK_LIBS) $(if $(strip $(RDMA_LIBS)),$(RDMA_LIBS),-libverbs) -pthread

test-phase4:
	bash tests/phase4_e2e_test.sh

test-all: test test-phase2 test-phase3 test-phase4
	@echo DPDK_RDMA_GATEWAY_CURRENT_ENV_COMPLETE

clean:
	rm -rf $(BUILD_DIR) tests/contract_test.log tests/runtime
