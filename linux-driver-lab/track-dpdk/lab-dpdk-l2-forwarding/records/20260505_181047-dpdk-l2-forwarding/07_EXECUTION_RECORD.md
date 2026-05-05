# 07_EXECUTION_RECORD

## lab-dpdk-l2-forwarding 执行记录

### Step 1: 00_check_env.sh

```
$ cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-dpdk-l2-forwarding
$ ./scripts/00_check_env.sh
[OK] env check saved: records/20260505_181047-dpdk-l2-forwarding/ENV_CHECK.txt
```

### Step 2: 01_build_app.sh (首次需安装 meson/ninja)

```
$ sudo apt install -y meson ninja-build  # 首次安装
$ ./scripts/01_build_app.sh
[OK] build saved: records/.../BUILD.log
[OK] binary: .../app/build/l2fwd-lite
```

问题：meson/ninja 未安装
- DPDK 21.11+ 使用 meson+ninja 构建系统
- meson 0.61.2, ninja 1.10.1

### Step 3: 02_prepare_vmxnet3.sh (sudo)

```
$ sudo ./scripts/02_prepare_vmxnet3.sh
[OK] prepare saved: records/.../PREPARE_VMXNET3.txt
```

### Step 4: 03_run_l2fwd_single_port.sh (sudo)

```
$ sudo ./scripts/03_run_l2fwd_single_port.sh
# 无输出（后台运行）
```

检查日志：
```
$ cat records/.../L2FWD_SINGLE_PORT.log
EAL: Detected CPU lcores: 8
EAL: Selected IOVA mode 'PA'
EAL: Probe PCI driver: net_vmxnet3 (15ad:07b0) device: 0000:0b:00.0
l2fwd-lite config: nb_mbuf=8192 mbuf_cache=250 rx_desc=1024 tx_desc=1024 burst=32 promisc=1
port 0 started: rx_desc=1024 tx_desc=1024 socket=0 driver=net_vmxnet3
port 0 MAC: 00:0C:29:F8:F6:82
available/initialized ports: 1
notice: only one port is available; running RX/free smoke mode, no L2 peer forwarding.
enter forwarding loop: run_seconds=15 stats_period=2 burst=32 lcore=0
==== l2fwd-lite software stats ====
port 0: rx=0 rx_bytes=0 tx=0 tx_bytes=0 tx_failed=0 no_peer_drop=0
```

### Step 5: 06_collect_stats.sh

```
$ ./scripts/06_collect_stats.sh
[OK] collect saved: records/.../COLLECT_STATS.txt
```

### Step 6: 07_make_review_bundle.sh

```
$ ./scripts/07_make_review_bundle.sh
[OK] review bundle: records/.../REVIEW_BUNDLE.md
```

### 关键结果

| 项目 | 结果 |
|------|------|
| meson | 0.61.2 |
| ninja | 1.10.1 |
| l2fwd-lite | 编译成功, 210KB |
| EAL IOVA | PA |
| Port 0 | net_vmxnet3, MAC: 00:0C:29:F8:F6:82 |
| 转发模式 | RX/free smoke (单端口) |
| Stats | rx=0 tx=0 tx_failed=0 |

### 分析

1. **编译成功**: meson + ninja 组合正常工作
2. **EAL 正常**: IOVA PA 模式，PCI 探测成功
3. **Port 初始化成功**: 只有一个 VMXNET3 口
4. **进入转发循环**: smoke test 正常
5. **RX/TX=0**: 单端口无 peer，正常现象

### 结论

**PASS_SMOKE** - l2fwd-lite C 数据面骨架验证通过

下一步进入 project-user-space-fastpath