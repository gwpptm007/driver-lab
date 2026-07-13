# TEST_RECORD_20260714_AF_XDP_FUNDAMENTALS

## 目标

收口 `track-af-xdp/docs/fundamentals`，验证：

1. 13 个主题及总入口具备足够篇幅、图示和有效链接。
2. `README.md`、`START_HERE.md`、`ROADMAP.md` 统一提供 Phase 0 前置入口。
3. 四个 Phase clean build 无回归。
4. veth COPY 环境重新验证 XDP PASS/DROP、XSKMAP、UMEM、FILL/RX、native COPY、TX/COMPLETION。
5. zero-copy unsupported 被记录为真实 capability boundary。

最终状态：`AF_XDP_FUNDAMENTALS_COMPLETE`。

## 环境

| 项目 | 值 |
| --- | --- |
| 日期 | 2026-07-14 |
| 本地 | Windows 工作区 |
| Linux 测试机 | `192.168.65.135` |
| 远端目录 | `/home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp` |
| 测试拓扑 | `veth-peer -> veth-xdp` |
| 测试地址 | peer `10.99.0.2/24`，target `10.99.0.1` |
| XDP | generic/COPY baseline、native/COPY probe |
| ZC 边界 | veth 无 NIC DMA，`Operation not supported` 为预期 |

## 文档规模

```text
files=14
topic_docs=13
lines=1526
mermaid=62
```

覆盖内核 RX/XDP、verifier/maps/loader、UMEM/frame、四环 ownership、XSKMAP/queue、COPY/ZC、TX/need-wakeup、多队列/RSS/shared UMEM、内存序、性能/NUMA、排障和项目速记。

## 本地审计

```powershell
cd E:\02_Learning\2026\gitcode\driver-lab\linux-driver-lab\track-af-xdp
py tests/check_fundamentals.py
py -m py_compile tests/check_fundamentals.py
```

结果：

```text
AF_XDP_FUNDAMENTALS_DOC_AUDIT_PASS files=14 lines=1526 mermaid=62 links=pass
AF_XDP_FUNDAMENTALS_COMPLETE
```

## Linux clean build

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp
chmod +x tests/*.sh
bash -n tests/*.sh
python3 -m py_compile tests/check_fundamentals.py
bash tests/software_regression.sh \
  > /tmp/af-xdp-fundamentals-build.log 2>&1
```

结果：

```text
AF_XDP_BUILD_PASS target=lab-xdp-redirect-basics
AF_XDP_BUILD_PASS target=lab-af-xdp-socket-rings
AF_XDP_BUILD_PASS target=lab-af-xdp-zero-copy-vs-copy
AF_XDP_BUILD_PASS target=project-af-xdp-mini-forwarder
AF_XDP_FUNDAMENTALS_AND_BUILD_REGRESSION_PASS
```

## 首轮发现的问题

### 1. 远端历史附件不完整

首次远端审计报告：

```text
ERROR: 相对链接不存在: docs/fundamentals/05_XSKMAP_REDIRECT_AND_QUEUE_BINDING.md
       -> ../../lab-af-xdp-socket-rings/docs/02_RING_MODEL.md
```

本地存在该旧附件，但 135 的旧副本没有同步。新知识层不应依赖非稳定历史附件，因此将链接改为稳定存在的 Phase 2 `README.md`，随后本地和远端审计均通过。

### 2. ZC 探针脚本存在无效 shell 文本

首轮 runtime 中，ZC probe 正确返回 `Operation not supported`，但脚本第 10-23 行使用 C 风格 ` *` 作为说明，shell 将通配符展开后的 `docs` 当作命令，产生：

```text
docs: command not found
```

修复 `lab-af-xdp-zero-copy-vs-copy/scripts/05_probe_zero_copy.sh`：将这些行改为标准 `#` 中文 shell 注释。`bash -n` 和完整 runtime 复验通过，最终日志不再出现该错误。

## 最终 veth 回归命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp
printf '%s\n' '<sudo-password>' | sudo -S env \
  AF_XDP_TEST_DURATION=4 \
  bash tests/veth_runtime_regression.sh \
  > /tmp/af-xdp-fundamentals-runtime-final.log 2>&1
```

脚本安全边界：

- 只创建固定测试设备 `veth-xdp/veth-peer`。
- 不操作管理网口、物理 NIC、PCI binding 或 hugepage。
- 使用 `trap` 在成功或失败退出时 detach 测试 XDP 并删除 veth。

## 最终运行结果

Phase 1：

```text
action=pass packets=36 bytes=5021
AF_XDP_RUNTIME_CASE_PASS name=phase1_pass
action=drop packets=8 bytes=754
AF_XDP_RUNTIME_CASE_PASS name=phase1_drop
```

Phase 2：

```text
AF_XDP_FINAL_STATS rx_packets=4 rx_bytes=196 fill_recycled=4
AF_XDP_RUNTIME_CASE_PASS name=phase2_rx
```

Phase 3：

```text
AF_XDP_FINAL_STATS rx_packets=5 rx_bytes=261 fill_recycled=5
AF_XDP_RUNTIME_CASE_PASS name=phase3_copy

AF_XDP_FINAL_STATS rx_packets=3 rx_bytes=126 fill_recycled=3
AF_XDP_RUNTIME_CASE_PASS name=phase3_native_copy

xsk_socket__create: Operation not supported
AF_XDP_RUNTIME_CASE_PASS name=phase3_zero_copy_probe
```

Phase 4：

```text
FORWARDER_FINAL_STATS rx_packets=4 tx_packets=0 dropped_packets=4 fill_recycled=4
AF_XDP_RUNTIME_CASE_PASS name=phase4_drop

FORWARDER_FINAL_STATS rx_packets=4 tx_packets=4 dropped_packets=0
fill_recycled=4 comp_packets=4 tx_full_drops=0
AF_XDP_RUNTIME_CASE_PASS name=phase4_reflect
```

最终 marker：

```text
AF_XDP_VETH_RUNTIME_REGRESSION_PASS
```

## 兼容性提示

运行日志中 libbpf 曾打印 BTF map create 错误后自动执行 `Retrying without BTF`，随后 map、program、XSK 和 runtime 均成功。这属于当前旧代码/libbpf 兼容 fallback，应保留观察，但不是本轮失败。

## 能力边界

- veth COPY 已验证 XDP/XSKMAP/UMEM/四环和 TX completion 的功能闭环。
- 当前 veth/kernel 的 native COPY 验证通过。
- veth 无物理 DMA，不能支持或证明 AF_XDP ZC。
- 真实 NIC ZC、RSS 多队列、PCIe/NUMA 和线速性能仍需要硬件环境复验。

## 结论

AF_XDP track 现在具备统一、详细的项目前知识层，入口、自动审计、clean build、隔离 veth runtime、错误诊断和完整测试证据均已收口。

