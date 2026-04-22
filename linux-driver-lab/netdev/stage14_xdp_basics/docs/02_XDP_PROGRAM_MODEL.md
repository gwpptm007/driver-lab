# 02_XDP_PROGRAM_MODEL — XDP 程序模型

## XDP 程序执行流程

```
Packet arrives
    ↓
驱动 RX 路径（page 层面）
    ↓
检查 priv->xdp_prog 是否存在
    ↓ (存在)
构建 xdp_buff
    ↓
bpf_prog_run(xdp_prog, &xdp)
    ↓
根据返回值处理：
  XDP_PASS → build_skb → GRO → netif_receive_skb
  XDP_DROP → page_pool_put_page → 归还 page
  XDP_TX   → 软模型仅统计（无真正 DMA TX）
  XDP_REDIRECT → 软模型仅统计（无真正 redirect）
```

---

## ndo_bpf 回调

驱动通过 `ndo_bpf` 回调注册 XDP program：

```c
static int stage14_xdp(struct net_device *ndev, struct netdev_bpf *bpf)
{
    struct stage14_priv *priv = netdev_priv(ndev);

    switch (bpf->command) {
    case XDP_SETUP_PROG:
        if (bpf->prog) {
            bpf_prog_inc(bpf->prog);
            priv->xdp_prog = bpf->prog;
        } else {
            priv->xdp_prog = NULL;
        }
        return 0;
    case XDP_QUERY_PROG:
        return 0;
    default:
        return -EINVAL;
    }
}
```

---

## XDP 处理路径

在 `stage14_consume_rx_one()` 中：

```c
/* XDP 处理路径：在 build_skb 之前先检查 XDP */
if (priv->xdp_prog) {
    struct xdp_buff xdp;
    u32 act;

    xdp.data = buf;
    xdp.data_end = buf + len;
    xdp.data_meta = buf;

    act = bpf_prog_run(priv->xdp_prog, &xdp);

    switch (act) {
    case XDP_PASS:
        atomic64_inc(&q->stats.xdp.xdp_pass);
        goto build_skb;  /* 继续走 build_skb 路径 */
    case XDP_DROP:
        atomic64_inc(&q->stats.xdp.xdp_drop);
        page_pool_put_page(q->pp, page);  /* 直接归还 page */
        memset(s, 0, sizeof(*s));
        memset(d, 0, sizeof(*d));
        r->consume_idx = stage14_next_idx(r->consume_idx, r->size);
        q->rx_ready--;
        stage14_refill_rx_slot(q, idx);
        return 0;
    case XDP_TX:
        atomic64_inc(&q->stats.xdp.xdp_tx);
        /* 软模型无法真正做 DMA TX，只统计 */
        return 0;
    case XDP_REDIRECT:
        atomic64_inc(&q->stats.xdp.xdp_redirect);
        /* 软模型无法真正做 redirect，只统计 */
        return 0;
    default:
        atomic64_inc(&q->stats.xdp.xdp_err);
        goto build_skb;
    }
}
build_skb:
    /* 原有 stage13 的 build_skb 路径 */
```

---

## XDP 统计

```c
struct stage14_xdp_stats {
    atomic64_t xdp_pass;      /* XDP_PASS 次数 */
    atomic64_t xdp_drop;      /* XDP_DROP 次数 */
    atomic64_t xdp_tx;        /* XDP_TX 次数 */
    atomic64_t xdp_redirect;  /* XDP_REDIRECT 次数 */
    atomic64_t xdp_err;       /* 未知 action 次数 */
};
```

---

## XDP 与 page_pool 的关系

```
正常 RX 路径（无 XDP）：
  page → build_skb → skb_mark_for_recycle → skb destructor → page_pool_put_full_page

XDP_DROP 路径：
  page → page_pool_put_page → page 直接归还 page_pool（不经过 build_skb）

XDP_PASS 路径：
  page → xdp_buff → XDP program 返回 PASS → build_skb → (同上)
```

**关键点**：XDP_DROP 时，page 直接通过 `page_pool_put_page()` 归还，不创建 skb，避免了内存分配开销。

---

## BPF program 加载

```bash
# 加载 XDP program
ip link set dev nds14s xdp obj xdp_count.o sec test

# 查看是否加载成功
ip link show nds14s
# 应该看到 xdp 标志

# 查看 XDP 统计
ethtool -S nds14s | grep xdp

# 卸载 XDP program
ip link set dev nds14s xdp off
```

---

## XDP program 示例（xdp_count.c）

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 256);
    __type(key, u32);
    __type(value, u64);
} xdp_stats SEC(".maps");

SEC("test")
int xdp_count_prog(struct xdp_buff *ctx)
{
    void *data = ctx->data;
    void *data_end = ctx->data_end;
    u32 key = 0;
    u64 *count;

    if (data + sizeof(struct ethhdr) > data_end)
        return XDP_PASS;

    count = bpf_map_lookup_elem(&xdp_stats, &key);
    if (count)
        (*count)++;

    return XDP_PASS;
}
```

编译：
```bash
clang -target bpf -O2 -c xdp_count.c
```
