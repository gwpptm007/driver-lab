# track-real-driver 测试流程

## 1. 测试分层

```mermaid
flowchart LR
    D[文档审计] --> S[shell/Python 静态检查]
    S --> K[kernel source symbol 回归]
    K --> C[runtime capability]
    C --> P[专项 patch/trace 实验]
```

前三层可自动执行；专项 patch 部署、unbind/reset 和高频 trace 必须使用各项目测试流程，不能由本知识层脚本自动改变系统。

## 2. Windows 本地审计

```powershell
cd linux-driver-lab/track-real-driver
py tests/check_fundamentals.py
py -m py_compile tests/check_fundamentals.py
```

检查 16 个知识层文件、最小篇幅、Mermaid 数、相对链接、代码围栏和三个入口 marker。

## 3. Linux 软件回归

```bash
cd linux-driver-lab/track-real-driver
chmod +x tests/*.sh
bash tests/software_regression.sh
```

该脚本会运行文档审计，并对 track 内全部 `*.sh` 执行 `bash -n`。它不会执行会修改网络、tracefs 或 kernel source 的历史实验脚本。

## 4. kernel source 回归

```bash
KERNEL_SRC=/path/to/linux bash tests/source_regression.sh
```

若未指定 `KERNEL_SRC`，脚本尝试仓库约定的 `kernel-src/linux-5.15.10`。找到源码后检查 virtio_net/e1000e 的核心符号；找不到则输出明确 `SKIP`。

## 5. runtime capability

```bash
bash tests/runtime_capability.sh
REAL_DRIVER_IFACE=eth1 bash tests/runtime_capability.sh
```

脚本只读取接口身份、features 和 stats。仅当 driver 是 `virtio_net`、`e1000` 或 `e1000e` 时输出 PASS；其他驱动输出 capability SKIP。

## 6. 最小运行回归

```bash
bash tests/runtime_regression.sh
REAL_DRIVER_IFACE=eth1 REAL_DRIVER_PING_TARGET=192.0.2.1 \
  bash tests/runtime_regression.sh
```

脚本先执行软件与源码回归，再对目标驱动接口发送 5 个 ICMP 包。它要求 RX/TX packet counter 前进且 RX/TX error counter 不增长，不执行 down/up、feature 修改、unbind 或 module reload。

## 7. marker

```text
REAL_DRIVER_DOC_AUDIT_PASS
REAL_DRIVER_FUNDAMENTALS_COMPLETE
REAL_DRIVER_SHELL_SYNTAX_PASS
REAL_DRIVER_SOFTWARE_REGRESSION_PASS
REAL_DRIVER_SOURCE_REGRESSION_PASS or SKIP
REAL_DRIVER_RUNTIME_CAPABILITY_PASS or SKIP
REAL_DRIVER_RUNTIME_REGRESSION_PASS or SKIP
```

## 8. 专项验证边界

- module/kernel build：按目标内核和项目 patch 文档执行；
- driver reload：先确认管理连接不经过目标接口；
- runtime trace：限定 hook、interface、CPU 和 duration；
- before/after：固定 kernel、driver、features、queue、workload；
- 任何 kernel warning、hang、packet loss 都先保存证据，再恢复环境。
