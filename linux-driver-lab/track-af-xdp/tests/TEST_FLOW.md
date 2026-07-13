# AF_XDP Fundamentals 测试流程

## 测试分层

```mermaid
flowchart LR
    A[本地文档审计] --> B[Linux clean build]
    B --> C[veth + COPY runtime]
    C --> D[native COPY + ZC probe]
    D --> E[drop/reflect + completion]
```

## 1. 本地快速审计

```powershell
py linux-driver-lab/track-af-xdp/tests/check_fundamentals.py
```

预期：

```text
AF_XDP_FUNDAMENTALS_DOC_AUDIT_PASS files=14 lines>=1400 mermaid>=55 links=pass
AF_XDP_FUNDAMENTALS_COMPLETE
```

## 2. Linux clean build

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp
bash -n tests/*.sh
python3 -m py_compile tests/check_fundamentals.py
bash tests/software_regression.sh
```

该步骤只编译四个 Phase，不 attach XDP、不创建 veth。

## 3. veth 运行回归

仅在测试窗口运行；脚本只创建并清理 `veth-xdp/veth-peer`，不操作管理网口和物理 NIC：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp
printf '%s\n' '<sudo-password>' | sudo -S env \
  AF_XDP_TEST_DURATION=4 bash tests/veth_runtime_regression.sh \
  > /tmp/af-xdp-fundamentals-runtime.log 2>&1
```

日志收敛：

```bash
grep -E 'DOC_AUDIT|BUILD_PASS|RUNTIME_CASE_PASS|FINAL_STATS|ZERO_COPY|VETH_RUNTIME_REGRESSION_PASS|FAIL|ERROR' \
  /tmp/af-xdp-fundamentals-runtime.log
```

最终 marker：

```text
AF_XDP_VETH_RUNTIME_REGRESSION_PASS
```

## 边界

- veth COPY 验证 XDP/XSKMAP/UMEM/四环功能。
- native COPY 是否可用取决于当前 veth/kernel。
- veth 没有 NIC DMA，ZC unsupported 是预期能力边界。
- 真实 NIC ZC、RSS 多队列、PCIe/NUMA 性能需要硬件复验。
