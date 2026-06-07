# 05_VETH_PAIR_DEEP_DIVE — veth pair 原理与实践

## 1. 什么是 veth pair

veth（Virtual Ethernet）pair 是 Linux 内核提供的一对**背靠背连接的虚拟网络设备**。

```text
┌──────────────┐         ┌──────────────┐
│   veth0      │◄═══════►│   veth1      │
│  (peer)      │         │  (peer)      │
└──────────────┘         └──────────────┘
      ↑ 发送                   ↑ 接收
      包从 veth0 进入           包从 veth1 出来
      等同于发到了 veth1        等同于从 veth0 收到的
```

**核心语义：** 向 pair 的一端发送数据包，等价于从另一端接收数据包。就像一根网线的两端。

## 2. 为什么需要 veth pair

### 2.1 物理网卡的问题

在单台测试机上用物理网卡做 XDP/eBPF 实验会遇到几个问题：

**问题 1：本地 IP 短路**

```text
同一台机器上：
  ens33 (192.168.100.77)  → 发 UDP 到 192.168.100.1 (ens192 的 IP)
      
内核路由查找：192.168.100.1 在本地
  → 走 local delivery 路径
  → 数据包通过 loopback 提交给上层协议栈
  → 不经过 ens192 的 RX 路径
  → XDP hook 不触发
```

这是 Linux 协议栈的设计行为——目标是本机 IP 的包不会"发出去再收回来"，而是直接在内部交付。内核代码路径：

```c
// net/ipv4/ip_input.c
ip_local_deliver_finish()  // 直接交付本地 socket
    → 不经过网卡 RX
    → XDP 在网卡 RX 之前，更不可能触发
```

**问题 2：需要外部机器**

要物理网卡的 XDP hook 触发，需要从**另一台机器**发送数据包到这台机器的网卡：

```text
外部机器 (192.168.100.100) → 交换机/虚拟交换机 → ens192 RX → XDP hook
```

在 VMware 实验环境中，没有第二台机器，难以构造外部流量。

**问题 3：硬件干扰**

物理网卡有 RSS（多队列）、offload（校验和、TSO/LRO）、中断合并等特性，这些会干扰 XDP 实验的可预测性。

### 2.2 veth 如何解决

```text
               同一台 Linux 主机
   ┌─────────────────────────────────────┐
   │                                     │
   │  veth-peer          veth-xdp        │
   │  (10.99.0.2)        (无 IP)         │
   │     │                  │            │
   │     │    ping 发出     │  XDP 程序   │
   │     │ ═══════════════►│  attached   │
   │     │                  │            │
   │     │  包从 veth-peer   │  包到达    │
   │     │  TX 出去         │  veth-xdp  │
   │     │                  │  RX        │
   │     │                  │  → XDP hook│
   │     │                  │  被触发!   │
   └─────────────────────────────────────┘
```

veth 在**设备层面**模拟了"从另一台机器收包"的行为：
- veth-peer 的 TX 操作 → veth-xdp 的 RX 路径
- 在 veth-xdp 端看来，这就是"外部到达的包"
- XDP hook 一定会触发（kern 5.4+ 的 veth 驱动支持 XDP）

## 3. veth pair 的内核实现

### 3.1 数据结构

```c
// drivers/net/veth.c (简化)
struct veth_priv {
    struct net_device __rcu *peer;      // 指向对端设备
    struct bpf_prog __rcu *_xdp_prog;   // XDP 程序（本端）
    u64 dropped_tx;                      // TX 丢包计数
    u64 peer_tq_xdp_xmit;               // XDP TX 统计
    // ...
};

struct veth_rq {
    struct veth_rq_stats stats;          // RX 队列统计
    struct napi_struct napi;            // NAPI 实例（支持 NAPI mode）
};
```

veth 驱动在 5.4+ 内核中支持：
- **Generic XDP** (skb mode)：始终支持
- **Native XDP** (driver mode)：需要 veth 驱动注册 NAPI，内核 5.12+ 完全支持
- **XDP Redirect**：可以把包从 veth 的一端重定向到另一端

### 3.2 数据包流转路径

```text
用户态: ping -I veth-peer 10.99.0.1
         │
         ▼
    raw_sendmsg()           ← 创建 ICMP 包
         │
         ▼
    ip_output()             ← 路由查找，确定出口设备 = veth-peer
         │
         ▼
    dev_queue_xmit()        ← 进入 veth-peer 的 TX 路径
         │
         ▼
    veth_xmit()             ← veth 驱动的 ndo_start_xmit
         │
         │  关键：把 skb 从 veth-peer 的 TX
         │  转发到 veth-xdp 的 RX
         │
         ▼
    __dev_forward_skb()     ← 或 veth_forward_skb()
         │
         ▼
    netif_rx()              ← 进入 veth-xdp 的 RX 路径
         │
         ▼
    bpf_prog_run_xdp()      ← ★ XDP hook 在这里触发！
         │
         │  XDP 程序返回:
         │  - XDP_PASS    → 继续走协议栈
         │  - XDP_DROP    → 丢弃
         │  - XDP_REDIRECT → 重定向到其他网卡/CPU
         │
         ▼
    netif_receive_skb()     ← XDP_PASS 后进入内核协议栈
         │
         ▼
    ip_rcv() → ip_local_deliver() → icmp_rcv()
         │
         ▼
    icmp_echo()             ← 如果目标 IP 存在，回复 ICMP reply
```

### 3.3 veth 对 XDP 的支持演变

| 内核版本 | veth XDP 支持 |
|----------|--------------|
| < 4.17 | 无 XDP 支持 |
| 4.17 - 5.3 | Generic XDP (skb mode)，每个包都经过 XDP |
| 5.4 - 5.11 | 加入 per-queue NAPI，支持 native XDP 的基础 |
| 5.12+ | 完整的 native XDP + XDP redirect + XDP TX |

本次测试机内核 6.8，veth 完全支持 native XDP。

## 4. 命令详解

### 4.1 创建 veth pair

```bash
ip link add <name1> type veth peer name <name2>
```

一条命令创建两个设备，互相对端。

```bash
# 示例
ip link add veth-xdp type veth peer name veth-peer
```

创建后的设备状态：

```bash
$ ip link show type veth
6: veth-peer@veth-xdp: <BROADCAST,MULTICAST,M-DOWN> ...
7: veth-xdp@veth-peer: <BROADCAST,MULTICAST,M-DOWN> ...
```

`@veth-xdp` 表示 veth-peer 的对端是 veth-xdp。M-DOWN 表示需要 `ip link set up`。

### 4.2 启动设备

```bash
ip link set veth-xdp up
ip link set veth-peer up
```

启动后状态变为 `UP,LOWER_UP`（两端都启动后，对端的 LOWER_UP 才出现，表示"链路层已连接"）。

### 4.3 配置 IP 地址

```bash
ip addr add 10.99.0.2/24 dev veth-peer
```

给 veth-peer 配 IP，veth-xdp 可以配也可以不配：

- **给 veth-xdp 配 IP**：发送端有目标地址 → 内核回复 ARP → ICMP reply 正常
- **不给 veth-xdp 配 IP**：发送端 ARP 无回复 → ICMP 发不出去 → 但 XDP 程序仍能看到初始的 ARP 包

本次测试中 veth-xdp **没配 IP**。ping 的 ARP 请求被 XDP_PASS 放过但无人回复，后续 ICMP 发不出去。但这不影响 XDP hook 统计那些 ARP 包。

### 4.4 查看统计

```bash
ip -s link show veth-xdp
```

输出示例：

```
RX:  bytes packets errors dropped missed mcast
      5875    56      0       0       0      0
TX:  bytes packets errors dropped carrier collsns
      4309    36      0       0       0      0
```

- RX = veth-xdp 收到的包（来自 veth-peer 的发送）
- TX = veth-xdp 发出的包（经过协议栈后发出的，XDP_PASS 的包可能走到这里）

### 4.5 查看 ethtool 统计（XDP 专用）

```bash
ethtool -S veth-xdp
```

```
rx_queue_0_xdp_packets: 0       ← XDP 程序处理的包数（native mode 才有）
rx_queue_0_xdp_bytes: 0         ← XDP 程序处理的字节数
rx_queue_0_drops: 0             ← XDP_DROP 丢弃数
rx_queue_0_xdp_redirect: 0      ← XDP_REDIRECT 重定向数
rx_queue_0_xdp_drops: 0         ← XDP 处理中丢弃数
rx_queue_0_xdp_tx: 0            ← XDP_TX 发送数
```

注意：这些 ethtool 统计只在 **native XDP mode** 下更新。skb mode 下都是 0——但我们用 BPF map 统计（per-CPU array），所以不受此限制。

### 4.6 删除 veth pair

```bash
ip link delete veth-xdp
```

删除一端即可，内核自动清理对端。

## 5. 在本次测试中的完整执行过程

### 5.1 创建拓扑

```bash
sudo ip link add veth-xdp type veth peer name veth-peer
sudo ip link set veth-xdp up
sudo ip link set veth-peer up
sudo ip addr add 10.99.0.2/24 dev veth-peer
```

结果：

```text
veth-peer@veth-xdp    UP  10.99.0.2/24    (发送端)
veth-xdp@veth-peer    UP  无 IP           (接收端，XDP 程序依附点)
```

### 5.2 附加 XDP 程序到 veth-xdp

```bash
cd app/build
sudo ./xdp_loader run \
    --ifname veth-xdp \
    --mode skb \
    --action pass \
    --duration 10 \
    --interval 1 \
    --obj ./xdp_redirect_basics.bpf.o
```

内部发生的事情：

```text
1. bpf_object__open  → 解析 BPF ELF 文件
2. bpf_object__load  → 加载 BPF 程序到内核
3. bpf_program__attach → bpf_xdp_attach(veth-xdp, prog_fd, flags)
                      → 内核把 BPF 程序挂到 veth-xdp 的 XDP hook 上
4. 进入 poll 循环:
   - 每 1 秒从 BPF map 读取 stats
   - 打印 action=drop/pass/redirect 的 packets/bytes
   - 循环 10 次后退出
5. bpf_xdp_detach → 从 veth-xdp 卸下 BPF 程序
```

### 5.3 并发注入流量

在 XDP 程序运行的 10 秒窗口内，从 veth-peer 发送流量：

```bash
for i in $(seq 1 50); do
    ping -c 1 -W 0.1 10.99.0.1 -I veth-peer
done
```

每个 ping 的行为：

```text
1. ping 创建 ICMP echo request
2. 目标 10.99.0.1 → 路由查表 → 走 veth-peer 出口
3. Linux 需要知道 10.99.0.1 的 MAC 地址 → 先发 ARP request
4. ARP 包: veth-peer TX → veth-xdp RX → XDP hook →
   - PASS mode: ARP 包进入协议栈，但 10.99.0.1 未配置 → 无 ARP reply
   - DROP mode: ARP 包被 XDP 丢弃（统计为 drop）
   - REDIRECT mode: ARP 包被 XDP 重定向（统计为 redirect）
```

**为什么 ping 显示 100% packet loss 但 XDP stats 有计数？**

因为 ping 的 packet loss 只看 ICMP reply 是否收到。而 XDP hook 在 ICMP request 和 ARP 这些**入站包**到达时就触发了。我们的 XDP 统计计算的是"经过了 XDP hook 的包"，不是"ping 往返成功的包"。

### 5.4 三种 XDP action 的流量差异解释

| action | packets | 原因 |
|--------|---------|------|
| XDP_PASS | 12 | ARP 请求被放行到协议栈，持续重试。加上部分 ICMP echo request。|
| XDP_DROP | 3 | ARP 请求被丢弃。没有 ARP reply → 发送端很快就放弃发 ICMP。|
| XDP_REDIRECT | 3 | 同 DROP。redirect 到空 XSKMAP 等同于丢弃。|

**关键认识：** XDP_DROP 测试中，DROP 的包主要是 ARP。这验证了"XDP 能丢弃任意接收包"的能力，同时暴露了 ARP 缺失导致后续流量无法到达的连锁效应——这个认知在后续 AF_XDP socket 实验中很重要（AF_XDP socket 需要处理 ARP 吗？答案是：看场景）。

### 5.5 清理

```bash
sudo ip link delete veth-xdp
# veth-peer 自动被删除
```

## 6. 常用的 veth 测试拓扑

### 6.1 最简拓扑（本次使用）

```text
veth-peer ←──→ veth-xdp (XDP)
  (有IP)          (无IP)
```

流量方向：veth-peer → veth-xdp。适合测试 XDP 接收路径。

### 6.2 双向带 IP

```text
veth0 (10.0.0.1/24) ←──→ veth1 (10.0.0.2/24)
```

两端配同一子网的 IP，可以互相 ping。适合测试完整的数据通路。

### 6.3 network namespace 隔离

```text
default ns                    test ns
┌─────────┐              ┌─────────────┐
│ veth-a  │◄────────────►│ veth-b      │
│ (XDP)   │              │ 10.0.0.2/24 │
│         │              │ (独立协议栈)  │
└─────────┘              └─────────────┘
```

创建过程：

```bash
# 创建 namespace
ip netns add test-ns

# 创建 veth pair
ip link add veth-a type veth peer name veth-b

# 把 veth-b 移入 namespace
ip link set veth-b netns test-ns

# 在 namespace 内配置
ip netns exec test-ns ip link set veth-b up
ip netns exec test-ns ip addr add 10.0.0.2/24 dev veth-b

# 在 default namespace 配置
ip link set veth-a up
ip addr add 10.0.0.1/24 dev veth-a

# 现在两个 namespace 可以通过 veth 通信
ip netns exec test-ns ping 10.0.0.1
```

**namespace 隔离的好处：**
- 独立的协议栈、路由表、ARP 表
- 可以模拟完全独立的"外部机器"
- ARP/路由等问题不会和主 namespace 互相干扰
- 是后续 AF_XDP 多程序测试的标准拓扑

### 6.4 多 veth + bridge

```text
ns1 (veth1) ──┐
              ├── bridge (br0) ── veth-host (XDP) ── host
ns2 (veth2) ──┘
```

多个 namespace 的 veth 接入同一个 bridge，模拟多台机器在同一交换机下。XDP 程序可以挂在 bridge 或 veth-host 上。

## 7. XDP mode 选择：skb vs native

veth 同时支持两种 XDP attach mode：

| mode | 标志 | 位置 | 性能 | 何时用 |
|------|------|------|------|--------|
| skb (generic) | XDP_FLAGS_SKB_MODE | 协议栈 skb 层 | 较低，每个包走完整的 skb 分配 | 开发调试、驱动不支持 native 时 |
| native (driver) | XDP_FLAGS_DRV_MODE | 驱动层，NAPI poll 内 | 较高，在 skb 分配之前 | veth 5.12+ 内核，生产环境 |

```bash
# skb mode
sudo AF_XDP_MODE=skb bash scripts/03_run_xdp_pass.sh

# native mode (veth 5.12+ 支持)
sudo AF_XDP_MODE=drv bash scripts/03_run_xdp_pass.sh
```

本次测试全部使用 `skb` mode，原因：
1. 和 ens192 保持一致（vmxnet3 也只支持 skb mode 在当前内核）
2. 概念简单，不会引入 native mode 的额外变量
3. XDP 程序对两种 mode 是完全相同的

## 8. 常见问题

### Q1: 创建 veth 后 `ip link show` 看不到？

需要 `ip link set up` 才会在默认输出中显示。`ip -a link show type veth` 可以看到所有 veth（包括未启动的）。

### Q2: ping 不通对端？

检查：
1. 两端是否 `ip link set up`
2. 两端 IP 是否在同一子网
3. 如果有 XDP_DROP 程序挂着，检查是否误丢包

### Q3: veth 能测试 XDP_TX 吗？

可以。XDP_TX 把包从原接口反射回去：

```text
veth-peer → veth-xdp (XDP_TX) → 包从 veth-xdp TX 发出 → veth-peer RX
```

在 veth-peer 上 `tcpdump` 能看到反射回来的包。这是 AF_XDP 反射测试的基础。

### Q4: 删除 namespace 后 veth 还在吗？

把 veth 一端移入 namespace 后，删除 namespace 会自动删除该端，**对端也会被自动删除**。这是 namespace 隔离的一个便利特性。

### Q5: veth pair perf 能到多少？

veth 的吞吐量取决于 CPU，因为没有硬件 offload。在 XDP native mode 下，veth 可以处理 ~2-5 Mpps（单核）。对于功能验证和学习实验完全足够。

## 9. 从零开始：完整使用过程（无需编译任何代码）

以下整个过程可以在任何 Linux 机器上独立完成，只依赖内核自带的 veth 和系统自带的 `ping` / `tcpdump`，不需要 XDP、不需要编译器。

### 9.1 创建并启动

```bash
# 创建一对 veth
sudo ip link add veth-a type veth peer name veth-b

# 启动
sudo ip link set veth-a up
sudo ip link set veth-b up

# 验证：两个设备都在，状态 UP,LOWER_UP
ip link show type veth
```

输出：

```
8: veth-b@veth-a: <BROADCAST,MULTICAST,UP,LOWER_UP> ...
9: veth-a@veth-b: <BROADCAST,MULTICAST,UP,LOWER_UP> ...
```

`@veth-a` 表示 veth-b 的对端是 veth-a。`LOWER_UP` 表示链路层已连接——等同于"网线已插好"。

### 9.2 验证：不用 XDP，先看 veth 本身能不能通

给两端配同一子网的 IP，测试双向 ping：

```bash
# 配 IP
sudo ip addr add 10.99.0.1/24 dev veth-a
sudo ip addr add 10.99.0.2/24 dev veth-b

# 从 veth-a ping veth-b
ping -c 3 -I veth-a 10.99.0.2
```

输出：

```
PING 10.99.0.2 (10.99.0.2) from 10.99.0.1 veth-a: 56(84) bytes of data.
64 bytes from 10.99.0.2: icmp_seq=1 ttl=64 time=0.050 ms
64 bytes from 10.99.0.2: icmp_seq=2 ttl=64 time=0.038 ms
64 bytes from 10.99.0.2: icmp_seq=3 ttl=64 time=0.039 ms

--- 10.99.0.2 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss
```

双向都通，证明 veth pair 工作正常。

### 9.3 用 tcpdump 观察 veth 上的流量

开两个终端。

**终端 1：在 veth-a 上抓包**

```bash
sudo tcpdump -i veth-a -nn
```

**终端 2：从 veth-b 发 ping**

```bash
ping -c 3 -I veth-b 10.99.0.1
```

终端 1 输出：

```
12:00:01.123456 ARP, Request who-has 10.99.0.1 tell 10.99.0.2, length 28
12:00:01.123478 ARP, Reply 10.99.0.1 is-at xx:xx:xx:xx:xx:xx, length 28
12:00:01.123501 IP 10.99.0.2 > 10.99.0.1: ICMP echo request, id 1234, seq 1, length 64
12:00:01.123512 IP 10.99.0.1 > 10.99.0.2: ICMP echo reply, id 1234, seq 1, length 64
```

在 veth-a 上抓到了从 veth-b 发来的 ARP + ICMP，证明流量确实跨过了 veth 的"虚拟网线"。

### 9.4 验证 XDP hook 生效

如果已经编译了 XDP 程序（本 lab 的 `xdp_loader`），把它挂到其中一端：

```bash
# 先清掉两端的 IP，避免干扰（XDP hook 在 IP 层之前，不影响测试）
sudo ip addr del 10.99.0.1/24 dev veth-a 2>/dev/null
sudo ip addr del 10.99.0.2/24 dev veth-b 2>/dev/null

# 编译 XDP 程序（只需做一次）
cd lab-xdp-redirect-basics
bash scripts/01_build_app.sh

# 挂 XDP_PASS 到 veth-a（10 秒后自动卸载）
sudo AF_XDP_IFACE=veth-a bash scripts/03_run_xdp_pass.sh &
sleep 2

# 从 veth-b 发流量
ping -c 10 -I veth-b 10.99.0.1
```

XDP loader 输出中可以看到 `action=pass packets=...` 的数字在增长——证明包经过了 XDP hook。

### 9.5 换 DROP 模式验证

```bash
sudo AF_XDP_CONFIRM_DROP=YES AF_XDP_IFACE=veth-a bash scripts/04_run_xdp_drop.sh &
sleep 1
ping -c 5 -I veth-b 10.99.0.1
```

这次 XDP stats 显示 `action=drop packets=...` 在增长，tcpdump 在 veth-a 上看不到任何包（全被 XDP 在驱动层丢弃了）。

### 9.6 清理

```bash
# 卸载 XDP 程序（通常已自动卸载，这里确保干净）
sudo ip link set dev veth-a xdp off 2>/dev/null

# 删除 veth pair（删一端即可，对端自动消失）
sudo ip link delete veth-a

# 验证已清除
ip link show type veth
# 输出为空：没有 veth 设备了
```

### 9.7 完整流程总结

```text
                        无需编译        需要编译（一次）
                       ─────────        ──────────────
sudo ip link add ...    ← 创建 veth
sudo ip link set up     ← 启动
sudo ip addr add ...    ← 配 IP
ping -I veth-a ...      ← 验证互通        bash scripts/01_build_app.sh
tcpdump -i veth-a       ← 观察流量
                                          sudo .../03_run_xdp_pass.sh   ← 挂 XDP
                                          sudo .../04_run_xdp_drop.sh   ← 挂 XDP
sudo ip link delete     ← 清理
```

veth 的创建、配 IP、互通验证完全不需要编译任何代码——它是内核内置功能。XDP 程序是"附加"在 veth 上的，就像你可以给物理网卡附加 XDP 程序一样。二者独立，互不绑定。

## 10. 总结

veth pair 是 Linux 网络实验中最重要的基础设施之一。它的价值在于：

- **模拟而非仿真**：veth 经过完整的内核网络栈（包括 XDP hook），行为与物理网卡完全一致
- **隔离性**：配合 namespace 可以创造任意复杂的网络拓扑
- **零成本**：不需要额外硬件或 VM
- **可复现**：每次创建的环境完全一致

对于 XDP/AF_XDP 实验来说，veth pair 解决了最核心的问题：**如何在单台机器上可靠地触发 XDP hook**。
