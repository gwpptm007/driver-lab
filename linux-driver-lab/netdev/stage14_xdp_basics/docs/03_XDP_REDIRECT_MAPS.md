# 03_XDP_REDIRECT_MAPS — XDP 重定向与 BPF Map

## XDP_REDIRECT

`XDP_REDIRECT` 将数据包重定向到其他设备或 BPF map：

```c
case XDP_REDIRECT:
    xdp_do_redirect(priv->xdp_prog, &xdp, priv->redirect_ctx);
    atomic64_inc(&q->stats.xdp.xdp_redirect);
    return 0;
```

**软模型限制**：由于没有真实的 TX 硬件路径，软模型只能统计 `xdp_redirect` 次数，无法真正转发。

---

## BPF Map 基础

BPF Map 用于 XDP program 存储状态和统计数据：

| Map 类型 | 用途 |
|---------|------|
| `BPF_MAP_TYPE_PERCPU_ARRAY` | 每 CPU 计数统计 |
| `BPF_MAP_TYPE_HASH` | 键值对存储（如连接跟踪） |
| `BPF_MAP_TYPE_DEVMAP` | 设备重定向映射 |
| `BPF_MAP_TYPE_CPUMAP` | CPU 负载均衡 |

---

## statsmap 示例

```c
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 256);
    __type(key, u32);
    __type(value, u64);
} xdp_stats SEC(".maps");
```

使用：
```c
u32 key = 0;
u64 *count = bpf_map_lookup_elem(&xdp_stats, &key);
if (count)
    (*count)++;
```

---

## devmap 示例

```c
struct {
    __uint(type, BPF_MAP_TYPE_DEVMAP);
    __uint(max_entries, 64);
    __type(key, u32);
    __type(value, struct bpf_devmap_val);
} tx_port SEC(".maps");
```

---

## XDP 统计验证

```bash
# 查看 XDP 统计（ethtool）
ethtool -S nds14s | grep xdp_

# 查看 debugfs XDP 状态
cat /sys/kernel/debug/netdev_stage14_soft/xdp

# 示例输出：
# xdp_prog=00000000abc123
# q0: xdp_pass=100 xdp_drop=5 xdp_tx=0 xdp_redirect=0 xdp_err=0
# q1: xdp_pass=98 xdp_drop=3 xdp_tx=0 xdp_redirect=0 xdp_err=0
```

---

## XDP 与 GRO 的交互

```
有 XDP program 时：
  page → xdp_buff → XDP program → XDP_PASS → build_skb → GRO → protocol stack

无 XDP program 时：
  page → build_skb → GRO → protocol stack

XDP_DROP 时：
  page → xdp_buff → XDP program → XDP_DROP → page_pool_put_page → 归还 page
```

---

## AF_XDP 简介

AF_XDP 是 XDP 的用户空间版本，提供了更灵活的用户空间绕过：

1. **socket 绑定** — 用户程序通过 socket 接收 XDP 数据包
2. **零拷贝** — 理论上可以达到接近 XDP 的性能
3. **与 XDP 配合** — XDP_REDIRECT 到 AF_XDP socket

**软模型限制**：AF_XDP 需要真实的硬件支持，软模型不支持。
