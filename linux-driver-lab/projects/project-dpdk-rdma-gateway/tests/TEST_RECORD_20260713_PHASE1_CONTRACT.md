# TEST_RECORD_20260713_PHASE1_CONTRACT

## 1. 目标

验证 DPDK producer 与 RDMA consumer 之间的固定 descriptor、wire 字节序、SPSC ring 和 staging slot generation 生命周期，为后续接入真实 pcap PMD 与 RXE 提供稳定契约。

## 2. 环境

- 主机：`192.168.65.135`
- 内核：`Linux 6.8.0-124-generic x86_64`
- 编译器：`cc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
- 在线 CPU：8
- 编译模式：`-O2 -g -std=c11 -Wall -Wextra -Werror`
- 外部运行时依赖：无

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/projects/project-dpdk-rdma-gateway
chmod +x tests/*.sh
bash -n tests/*.sh
make clean
make
make test
```

## 4. 编译结果

```text
cc -Iinclude -O2 -g -std=c11 -Wall -Wextra -Werror \
  src/gateway_contract.c tests/contract_test.c \
  -o build/gateway-contract-test
```

- 编译 warning/error：0
- 二进制大小：35456 bytes

## 5. 测试结果

```text
GATEWAY_CONTRACT_LAYOUT_PASS request_size=32 wire_size=40 align=64
GATEWAY_WIRE_ROUNDTRIP_PASS
GATEWAY_WIRE_BOUNDARY_PASS cases=6
GATEWAY_RING_SPSC_PASS requests=256 full_empty_wrap=pass
GATEWAY_SLOT_LIFECYCLE_PASS stale_completion=blocked generation=2
DPDK_RDMA_GATEWAY_PHASE1_CONTRACT_PASS
PASS: DPDK-RDMA gateway Phase 1 contract and lifecycle
script_summary name=contract_test status=pass
```

## 6. 覆盖说明

| 测试 | 覆盖内容 | 结果 |
|---|---|---|
| layout | descriptor 大小、staging 对齐 | PASS |
| wire roundtrip | 64/32/16 位大端字段 | PASS |
| wire boundaries | magic/version/header/opcode/payload/短 buffer | PASS |
| SPSC ring | empty/full/FIFO/256 次回绕 | PASS |
| slot lifecycle | 重复 prepare、错误 generation、迟到 completion | PASS |

## 7. 结论与边界

Phase 1 contract PASS。当前结果证明纯 C 数据契约与生命周期正确，不证明 DPDK 收包、MR 注册、RDMA WRITE 或 CQ polling 已完成。Phase 2 将接入 pcap PMD，但仍不会把 `rte_mbuf *` 直接传给 RDMA worker。
