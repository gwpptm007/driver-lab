# 07_EXECUTION_RECORD

## project-user-space-fastpath 执行记录

### Step 1: 00_check_env.sh

```
$ cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/project-user-space-fastpath
$ ./scripts/00_check_env.sh
[OK] env check saved: records/20260505_202300-user-space-fastpath/ENV_CHECK.txt
```

### Step 2: 01_build_app.sh

```
$ ./scripts/01_build_app.sh
[OK] build saved: records/.../BUILD.log
[OK] binary: .../app/build/fastpath-lite
```

### Step 3: 02_prepare_vmxnet3.sh (sudo)

```
$ sudo ./scripts/02_prepare_vmxnet3.sh
[OK] prepare saved: records/.../PREPARE_VMXNET3.txt
```

### Step 4: 03_run_fastpath_single_port.sh (sudo)

```
$ sudo ./scripts/03_run_fastpath_single_port.sh
[OK] single-port run saved: records/.../FASTPATH_SINGLE_PORT.log
```

关键日志：
```
fastpath-lite config: nb_mbuf=8192 mbuf_cache=250 rx_desc=1024 tx_desc=1024 burst=32
policy: promisc=1 udp_only=1 swap_mac=1 rewrite_enable=0
initializing port 0
port 0 started: rx_desc=1024 tx_desc=1024 socket=0 driver=net_vmxnet3
port 0 MAC: 00:0C:29:F8:F6:82
available/initialized ports: 1
notice: only one port is available; running RX/classify/free smoke mode
enter fastpath loop: run_seconds=20 stats_period=2 burst=32 lcore=0

==== fastpath-lite software stats ====
port 0: rx=0 rx_bytes=0 tx=0 tx_bytes=0 tx_failed=0 arp=0 ipv4=0 udp=0 non_udp=0 rewrite=0 drop_short=0 drop_non_udp=0 drop_no_peer=0
```

### Step 5: 07_collect_stats.sh

```
$ ./scripts/07_collect_stats.sh
[OK] stats saved: records/.../COLLECT_STATS.txt
```

### Step 6: 08_make_review_bundle.sh

```
$ ./scripts/08_make_review_bundle.sh
[OK] review bundle saved: records/.../REVIEW_BUNDLE.md
```

### 关键结果

| 项目 | 结果 |
|------|------|
| meson | 0.61.2 |
| ninja | 1.10.1 |
| fastpath-lite | 220KB, 编译成功 |
| EAL IOVA | PA |
| policy | promisc=1, udp_only=1, swap_mac=1 |
| Port 0 | net_vmxnet3, MAC: 00:0C:29:F8:F6:82 |
| 转发模式 | RX/classify/free smoke |
| Stats | arp=0, ipv4=0, udp=0, non_udp=0 |

### 分析

1. **编译成功**: meson + ninja 构建正常
2. **EAL 正常**: IOVA PA 模式，PCI 探测成功
3. **Policy 生效**: promisc/udp_only/swap_mac 配置正确传递
4. **分类计数**: arp/ipv4/udp/non_udp 计数器初始化正常
5. **RX/TX=0**: 单端口 smoke，无外部流量，正常现象

### 结论

**PASS_SMOKE** - fastpath-lite 数据面骨架验证通过

与 l2fwd-lite 相比，fastpath-lite 新增：
- UDP-only 分类过滤
- MAC/IPv4/UDP rewrite 规则框架
- 精细化统计 (arp/ipv4/udp/non_udp/rewrite)

下一步：接入第二个 DPDK 口或 vhost/virtio-user 验证真正转发