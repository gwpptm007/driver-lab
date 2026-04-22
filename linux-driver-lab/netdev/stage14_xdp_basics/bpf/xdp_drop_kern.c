/* SPDX-License-Identifier: GPL-2.0
 * xdp_drop_kern.c — XDP_DROP 示例，所有包都丢弃
 *
 * 用途：验证 XDP_DROP 在驱动层的零开销丢弃能力
 *
 * 编译：
 *   clang -O2 -target bpf -Wall \
 *     -I /usr/include/bpf \
 *     -c xdp_drop_kern.c -o xdp_drop_kern.o
 *
 * 加载：
 *   sudo ip link set dev nds14s xdp obj xdp_drop_kern.o sec xdp_drop
 *
 * 验证：
 *   # 运行前先记录基准
 *   ethtool -S nds14s | grep -E 'xdp_drop|rx_packets'
 *   # 运行 traffic
 *   ping ... 或 smoke test
 *   # XDP_DROP 时 rx_packets 和 xdp_drop 都应增长
 *   #   但协议栈收不到包（tcpdump 看不到，ping 无响应）
 *   #   这是因为 XDP 在 build_skb 之前就丢弃了
 *
 * 卸载：
 *   sudo ip link set dev nds14s xdp off
 *
 * 注意：XDP_DROP 丢弃的包不经过 GRO，不创建 skb，
 *       所以 tcpdump 抓不到这些包——这是正常现象，不是抓包工具的问题。
 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 4);
    __type(key, __u32);
    __type(value, __u64);
} xdp_stats SEC(".maps");

static __always_inline void stats_inc(__u32 idx)
{
    __u32 key = idx;
    __u64 *cnt = bpf_map_lookup_elem(&xdp_stats, &key);
    if (cnt)
        *cnt += 1;
}

/*
 * XDP_DROP: 所有包都丢弃
 *
 * 返回 XDP_DROP 后，驱动调用 page_pool_put_page() 归还 page，
 * 不调用 build_skb，不上送协议栈。
 *
 * 软模型（stage14）限制：
 *   - XDP_DROP 在软模型中确实丢弃（page 归还 pool）
 *   - 真实硬件 NIC 上 XDP_DROP 更早（DMA 阶段就丢弃），性能收益更大
 *
 * 测试注意：
 *   - tcpdump 在 nds14s 上看不到任何包（因为根本没到协议栈）
 *   - ping nds14s 无响应（协议栈没收到包）
 *   - 但 ethtool -S nds14s 显示 xdp_drop 增长
 */
SEC("xdp_drop")
int xdp_drop_func(struct xdp_buff *xdp)
{
    stats_inc(1);
    return XDP_DROP;
}

char LICENSE[] SEC("license") = "GPL";
