/* SPDX-License-Identifier: GPL-2.0
 * xdp_pass_kern.c — XDP_PASS 示例，所有包都上送协议栈
 *
 * 用途：验证 XDP program 加载后流量正常上送
 *
 * 编译（需要 clang + llvm + kernel headers）：
 *   clang -O2 -target bpf -Wall \
 *     -I /usr/include/bpf \
 *     -c xdp_pass_kern.c -o xdp_pass_kern.o
 *
 *   或者指定内核 build 目录：
 *   KDIR=/lib/modules/$(uname -r)/build
 *   clang -O2 -target bpf -Wall \
 *     -I $KDIR/tools/lib/bpf \
 *     -I $KDIR/include \
 *     -c xdp_pass_kern.c -o xdp_pass_kern.o
 *
 * 加载：
 *   sudo ip link set dev nds14s xdp obj xdp_pass_kern.o sec xdp_pass
 *
 * 验证：
 *   ip link show nds14s      # 应显示 xdp 标志
 *   ip netconf show dev nds14s  # 应显示 XDP program
 *   ethtool -S nds14s | grep xdp_pass  # 运行 traffic 后应增长
 *
 * 卸载：
 *   sudo ip link set dev nds14s xdp off
 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

/*
 * BPF map: per-CPU 统计计数器
 *   index 0 = xdp_pass 计数
 *   index 1 = xdp_drop 计数
 *   index 2 = xdp_tx 计数
 *   index 3 = xdp_redirect 计数
 */
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
 * XDP_PASS: 所有包都上送协议栈
 *
 * 返回 XDP_PASS 后，内核会继续调用 build_skb 路径，
 * 数据包按正常网络栈处理（GRO -> netif_receive_skb -> 协议层）。
 *
 * 测试用例：
 *   - 加载后收发应正常（与无 XDP 时行为一致）
 *   - ethtool -S nds14s | grep xdp_pass 应增长
 *   - tcpdump 在 nds14s 上应能看到包
 */
SEC("xdp_pass")
int xdp_pass_func(struct xdp_buff *xdp)
{
    stats_inc(0);
    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
