# app/ - fastpath-lite

`fastpath-lite` 是 `project-user-space-fastpath` 的核心 C 数据面程序，继承自 `l2fwd-lite` 并增加协议分类和 rewrite 功能。

## 1. 与 l2fwd-lite 的区别

| 特性 | l2fwd-lite | fastpath-lite |
|------|-------------|---------------|
| L2 MAC swap | ✅ | ✅ |
| 协议分类 | ❌ | ✅ ARP/IPv4/UDP/non_UDP |
| UDP-only 过滤 | ❌ | ✅ |
| MAC/IPv4/UDP rewrite | ❌ | ✅ |
| 精细化统计 | 基础 | arp/ipv4/udp/rewrite/drop |

## 2. 核心原理

### 2.1 协议分类流程

```
收到 mbuf
    ↓
解析 Ethernet type
    ↓
├─ ARP     → 交换 MAC → 转发
├─ IPv4/UDP → 按配置 rewrite 或交换 MAC → 转发
├─ IPv4/non-UDP → udp_only=1 时丢弃，否则交换 MAC → 转发
└─ 其他    → 丢弃或交换 MAC
```

### 2.2 rewrite 规则

当 `--rewrite 1` 启用时，可以指定：

```bash
--rewrite-src-mac 02:00:00:00:00:11   # 替换 src MAC
--rewrite-dst-mac 02:00:00:00:00:22   # 替换 dst MAC
--rewrite-src-ip 10.10.1.10           # 替换 src IP
--rewrite-dst-ip 10.10.2.20           # 替换 dst IP
--rewrite-src-port 5000              # 替换 src UDP port
--rewrite-dst-port 6000               # 替换 dst UDP port
```

rewrite 会自动重新计算 IPv4 checksum。

### 2.3 UDP-only 模式

```bash
--udp-only 1
```

启用后，只允许 UDP 包通过，非 UDP 包（包括 IPv4 non-UDP、ARP 等）会被丢弃。

## 3. 关键代码结构

```c
int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);           // 1. EAL 初始化
    parse_app_args(argc, argv);           // 2. 解析参数（支持 rewrite 等）
    mbuf_pool = rte_pktmbuf_pool_create(); // 3. 创建 mbuf 池
    init_all_ports(mbuf_pool);           // 4. 初始化端口
    forwarding_loop();                    // 5. 转发循环（调用 classify_and_rewrite）
    print_sw_stats();                     // 6. 打印统计
    print_ethdev_stats();                // 7. 打印硬件统计
    stop_all_ports();                     // 8. 停止端口
    rte_eal_cleanup();                    // 9. 清理
}
```

### 核心分类函数

```c
classify_and_rewrite(src_portid, mbuf)
    ├─ 解析 Ethernet type
    ├─ ARP:     swap_mac → true
    ├─ IPv4:    handle_ipv4_udp() → 处理 UDP rewrite
    └─ Other:   udp_only? drop : swap_mac
```

## 4. 软件统计详解

```text
rx_packets       - 收到的包总数
tx_packets       - 发送的包总数
tx_failed        - 发送失败（TX 队列满）
arp_packets      - ARP 包数
ipv4_packets     - IPv4 包数
udp_packets      - UDP 包数
non_udp_packets  - 非 UDP 包数
rewrite_packets  - 被 rewrite 的包数
drop_short       - 包太短被丢弃
drop_non_udp    - udp_only 模式下非 UDP 被丢弃
drop_no_peer     - 无配对端口被丢弃
```

## 5. 构建

```bash
cd app
meson setup build
ninja -C build
```

编译产物：`app/build/fastpath-lite`

## 6. 运行示例

### 单端口 smoke

```bash
sudo ./build/fastpath-lite -l 0-1 -n 4 \
  --file-prefix fastpath_lite \
  -a 0000:0b:00.0 \
  -- \
  --run-seconds 20 --stats-period 2
```

### UDP-only 过滤

```bash
sudo ./build/fastpath-lite -l 0-1 -n 4 \
  -a 0000:0b:00.0 \
  -- \
  --udp-only 1 --run-seconds 20
```

### rewrite 示例

```bash
sudo ./build/fastpath-lite -l 0-1 -n 4 \
  -a 0000:0b:00.0 \
  -- \
  --rewrite 1 \
  --rewrite-src-ip 10.10.1.10 \
  --rewrite-dst-ip 10.10.2.20 \
  --rewrite-src-port 5000 \
  --rewrite-dst-port 6000 \
  --run-seconds 20
```

## 7. 下一步演进方向

```
当前: fastpath-lite (分类 + rewrite)
  ↓
未来: user-space-fastpath (生产级)
  ├─ per-flow stats (流级统计)
  ├─ control-plane config (控制面配置)
  ├─ records/replay/report (流量回放)
  ├─ flow table / ACL
  └─ multi-lcore scaling (多核扩展)
```