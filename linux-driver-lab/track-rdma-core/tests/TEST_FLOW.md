# RDMA Fundamentals 测试流程

## 目标

验证新的 RDMA 知识入口满足四个条件：文档齐全、图文结构有效、入口一致、原有 RDMA 功能没有因路线改造而回归。

## 测试层次

```mermaid
flowchart LR
    A[本地文档审计] --> B[Linux shell/py_compile]
    B --> C[六个基础 verbs 实验]
    C --> D[RC/perf/KV 扩展回归]
    D --> E[grep 最终 marker]
```

## 1. 本地快速审计

Windows 工作区不要求安装 RDMA 设备，只运行纯文档检查：

```powershell
py linux-driver-lab/track-rdma-core/tests/check_fundamentals.py
```

预期：

```text
RDMA_FUNDAMENTALS_DOC_AUDIT_PASS files=14 lines>=1500 mermaid>=50 links=pass
RDMA_FUNDAMENTALS_COMPLETE
```

## 2. 135 基础回归

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core
bash -n tests/*.sh
python3 -m py_compile tests/check_fundamentals.py
SUDO_PASSWORD='<sudo-password>' PREPARE_RXE=1 RDMA_GID_INDEX=1 \
  bash tests/software_regression.sh
```

这一步会重新编译并运行六个基础实验，不修改 PCI 绑定、hugepage 或物理 RNIC；`PREPARE_RXE=1` 会显式添加测试 link-local 地址并重建软件 `rxe0`，因此应在没有其他 RXE 业务的测试窗口运行。

## 3. 阶段扩展回归

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core
SUDO_PASSWORD='<sudo-password>' PREPARE_RXE=1 RDMA_GID_INDEX=1 \
  EXTENDED_REGRESSION=1 \
  bash tests/software_regression.sh
```

扩展模式额外运行：

- RC client/server 正向路径和 wrong-rkey/RNR/断连边界。
- performance tuning 的 SEND、batch、inline/selective/polling smoke。
- one-sided KV 的 batch、Atomic/CAS、动态目录和 rkey rotation。

## 4. 日志收敛

若完整输出较长，只读取 marker：

```bash
bash tests/software_regression.sh 2>&1 | tee /tmp/rdma-fundamentals-regression.log
grep -E 'DOC_AUDIT|RDMA_REGRESSION_(BEGIN|PASS)|CURRENT_ENV_COMPLETE|SOFTWARE_REGRESSION_PASS|FAIL|ERROR' \
  /tmp/rdma-fundamentals-regression.log
```

最终 marker：

```text
RDMA_FUNDAMENTALS_AND_SOFTWARE_REGRESSION_PASS extended=1 prepare_rxe=1
```

## 失败处理

1. 文档审计失败：只修复报告的缺失文件、链接或围栏。
2. 某个实验失败：先 grep 第一个 `ERROR/FAIL/wc_status/vendor_err`，再读取前后局部日志。
3. RXE 配置失败：记录环境阻塞，不把它包装成代码回归。
4. 扩展测试失败：保留基础六实验结果，并明确失败项目和首个 marker。
