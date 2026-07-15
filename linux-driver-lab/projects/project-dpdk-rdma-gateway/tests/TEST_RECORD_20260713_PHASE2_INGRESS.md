# TEST_RECORD_20260713_PHASE2_INGRESS

## 1. 目标

在 Phase 1 contract 之上接入 DPDK pcap PMD，验证 Ethernet/IPv4/UDP parser、payload staging copy、mbuf 及时释放、request ring 和 mock completion 的组合路径。

## 2. 环境

- 主机：`192.168.65.135`
- DPDK：`21.11.9`
- PMD：`net_pcap`
- EAL：`-l 0 -n 4 --no-pci --no-huge`
- 编译：`-O2 -g -std=gnu11 -Wall -Wextra -Werror`
- 输入：64 packets，48 UDP + 16 ICMP

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/projects/project-dpdk-rdma-gateway
chmod +x tests/*.sh tools/*.py
bash -n tests/*.sh
python3 -m py_compile tools/*.py
make clean
make test-all
```

## 4. 首次构建修正

纯 contract 使用严格 `-std=c11`。DPDK 21.11 头文件需要 `ssize_t`、`strnlen` 等 GNU/POSIX 声明，首次复用 C11 参数时被 `-Werror` 拒绝。修正为两个 target 分离：contract 保持 C11，DPDK app 使用 `-std=gnu11`，不关闭任何 warning。

## 5. 实测结果

```text
GATEWAY_PCAP_GENERATED packets=64 udp=48 unsupported=16 output=tests/runtime/gateway_phase2.pcap
GATEWAY_INGRESS_CONFIG port=0 queue=0 expected=64 burst=32
GATEWAY_INGRESS_RESULT rx=64 udp=48 unsupported=16 malformed=0 staged=48 ring_full=0 slot_exhausted=0
GATEWAY_MOCK_RDMA_RESULT dequeued=48 completed=48 payload_bytes=1536
DPDK_RDMA_GATEWAY_PHASE2_INGRESS_PASS
cleanup=complete result=pass
PASS: DPDK-RDMA gateway Phase 2 pcap ingress and staging
script_summary name=phase2_ingress_test status=pass
```

## 6. 守恒关系

```text
rx = udp + unsupported + malformed
64 = 48 + 16 + 0

udp = staged = dequeued = completed
48 = 48 = 48 = 48

payload_bytes = completed * 32 = 1536
```

## 7. 结论与边界

Phase 2 pcap ingress PASS。该结果证明真实 DPDK RX mbuf 能被解析、复制到 staging、发布为 request 并完成 mock 回收；不证明 verbs MR、QP、RDMA WRITE 或 CQ 已接入。Phase 3 将独立实现 RXE backend，再与 ingress 集成。
