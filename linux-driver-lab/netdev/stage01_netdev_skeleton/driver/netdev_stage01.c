// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage01.c - stage01 最小 net_device 骨架
 *
 * ==================== 文件概述 ====================
 *
 * stage01 的目标不是实现一个"完整网卡"，而是把 net_device 的最小生命周期
 * 做成一个可观测、可解释的教学骨架。
 *
 * 【学习焦点】
 *   1. net_device 生命周期：alloc → register → open/close → unregister → free
 *   2. ndo_open / ndo_stop / ndo_start_xmit / ndo_get_stats64 四个核心回调
 *   3. u64_stats_sync 并发保护：软中断写 + 进程上下文读如何安全共享统计
 *   4. debugfs 导出：seq_file 接口导出私有统计（open_count / last_proto 等）
 *   5. 教学策略：为什么 skb 直接消费而不是真实发送
 *
 * 【与 stage00 的关系】
 *   stage00：环境验证（kernel headers / QEMU / toolchain 是否就绪）
 *   stage01：在验证过的环境上，跑通最小 net_device 骨架
 *
 * 【与 stage02~stage06 的关系】
 *   stage01 是整个 netdev 路径的"坐标原点"：
 *     - stage02：加入 skb 生命周期（clone / copy）
 *     - stage03：加入 NAPI / poll / 中断抑制
 *     - stage04：加入 ring / DMA / RX replenishment
 *     - stage05：virtio-net 对照
 *     - stage06：ARM64 迁移
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头部注释与 include（第1~40行）
 *  2. 宏与模块参数（第42~45行）
 *  3. 私有数据结构 stage01_priv（第47~63行）
 *  4. 全局 net_device 指针（第65行）
 *  5. 统计更新函数（第67~86行）
 *  6. ndo_open / ndo_stop（第88~122行）
 *  7. ndo_start_xmit（第124~143行）
 *  8. ndo_get_stats64（第145~164行）
 *  9. net_device_ops 填充（第166~173行）
 * 10. debugfs seq_file 导出（第175~238行）
 * 11. stage01_setup（第240~251行）
 * 12. 模块 init / exit（第253~294行）
 *
 * 【为什么这样组织？】
 *   → stage01 故意保持最小，所有内容在一个文件里
 *   → 便于阅读和理解 net_device 生命周期的每一个环节
 *   → 后续 stage02~04 会在此基础上逐步扩展
 */

/* ==================== 第1部分：头文件 ==================== */
#include <linux/debugfs.h>      /* debugfs_create_dir / debugfs_create_file */
#include <linux/etherdevice.h>  /* ether_setup / eth_hw_addr_random */
#include <linux/init.h>         /* module_init / module_exit */
#include <linux/module.h>       /* MODULE_* 宏 */
#include <linux/netdevice.h>    /* struct net_device / netdev_ops / register_netdev */
#include <linux/seq_file.h>     /* struct seq_file / seq_printf */
#include <linux/skbuff.h>       /* struct sk_buff / dev_consume_skb_any */
#include <linux/u64_stats_sync.h> /* u64_stats_update_begin / u64_stats_fetch_begin */

/*
 * 【头文件选择说明】
 *
 * netdevice.h 是网络驱动最核心的头文件，包含了：
 *   - struct net_device：网络设备结构体
 *   - struct net_device_ops：驱动回调操作集
 *   - struct rtnl_link_stats64：标准统计结构体
 *   - register_netdev / unregister_netdev：设备注册/注销
 *   - netif_carrier_on/off：载波状态管理
 *   - netif_start/stop_queue：队列管理
 *
 * debugfs.h 和 seq_file.h 用于导出私有调试信息：
 *   - debugfs_create_dir：在 /sys/kernel/debug/ 下创建目录
 *   - debugfs_create_file：创建文件并绑定 file_operations
 *   - seq_file：迭代器接口，比直接 read() 更适合导出变长数据
 */

/* ==================== 第2部分：宏与模块参数 ==================== */
#define DRV_NAME "netdev_stage01"

/*
 * 模块参数：允许用户在 insmod 时指定接口名
 *   insmod netdev_stage01.ko ifname=eth_edu
 * 默认值 "nds0"：nds = netdev stage01 的缩写
 */
static char ifname[IFNAMSIZ] = "nds0";
module_param_string(ifname, ifname, sizeof(ifname), 0644);
MODULE_PARM_DESC(ifname, "interface name for stage01 teaching net_device");

/* ==================== 第3部分：私有数据结构 ==================== */
/*
 * stage01_priv：挂在 net_device->priv 上的驱动私有数据
 *
 * 【为什么需要私有数据？】
 *   → net_device 是通用结构体，不包含驱动特定的运行时统计
 *   → priv 区域由驱动自行管理，存储本驱动特有的状态
 *
 * 【字段分类】
 *   基本统计（内核标准，ip -s link 能读到）：
 *     tx_packets / tx_bytes / tx_dropped
 *   私有扩展（只有 debugfs 有，ip link 看不到）：
 *     open_count / stop_count / last_proto / last_len
 *
 * 【为什么基本统计和私有扩展要分开？】
 *   → 基本统计是内核标准字段，由 ndo_get_stats64 填充
 *   → 私有扩展是教学用途，帮助理解 start_xmit 的行为
 *   → 两者使用同一套 u64_stats_sync 保护，但输出到不同地方
 *
 * 【u64_stats_sync 并发保护说明】
 *   → ndo_start_xmit 在软中断（BH）上下文执行
 *   → ip link show 在进程上下文执行
 *   → 两者同时访问 priv 里的统计字段，需要 seqcount 保护
 */
struct stage01_priv {
    struct net_device *ndev;      /* 回指到父设备，debugfs show 里读取设备名 */
    struct dentry *dbg_dir;         /* debugfs 目录项，卸载时 remove_recursive */
    struct u64_stats_sync syncp;   /* 并发保护：seqcount 标记读写事务 */

    /* ───────── 基本统计（ndo_get_stats64 填充）───────── */
    u64 tx_packets;                /* 总发包计数 */
    u64 tx_bytes;                  /* 总发字节计数 */
    u64 tx_dropped;                /* 丢弃计数 */

    /* ───────── 私有扩展（仅 debugfs 可见）───────── */
    u64 open_count;                /* ndo_open 被调用次数 */
    u64 stop_count;                /* ndo_stop 被调用次数 */
    u64 last_proto;                /* 最后一个包的 ETHERTYPE（主机序）*/
    u64 last_len;                  /* 最后一个包的长度 */
};

/* ==================== 第4部分：全局 net_device 指针 ==================== */
/*
 * stage01_dev：模块级别唯一实例
 *
 * 【为什么用全局变量而不是嵌入到 priv 里？】
 *   → stage01 是单设备教学驱动，不需要管理多个 net_device
 *   → 全局变量是最简单的方案
 *   → 真实多队列网卡会管理设备数组，但 stage01 不需要
 *
 * 【卸载安全】
 *   → stage01_exit 里用 NULL 判断，防止重复卸载
 *   → unregister_netdev 后立即置 NULL
 */
static struct net_device *stage01_dev;

/* ==================== 第5部分：统计更新函数 ==================== */
/*
 * stage01_update_tx_stats：更新 TX 统计
 *
 * 【为什么要封装成函数而不是直接内联？】
 *   → 封装后更清晰，open/xmit/stop 都可以调用
 *   → 后续 stage02 如果要加更复杂的统计逻辑，只需改这一处
 *
 * 【参数说明】
 *   priv：驱动私有数据指针
 *   len：本次发包的字节数
 *   proto：ETHERTYPE（网络字节序）
 *
 * 【ntohs 的作用】
 *   → skb->protocol 是网络字节序（大端）
 *   → 存储到 last_proto 时转为主机字节序（显示更直观）
 *   → 网络字节序用于穿过内核协议栈，主机字节序用于本地显示
 */
static void stage01_update_tx_stats(struct stage01_priv *priv,
                                    unsigned int len, __be16 proto)
{
    unsigned long flags;

    /* 写侧：进入临界区，seqcount 变奇数 */
    flags = u64_stats_update_begin_irqsave(&priv->syncp);
    priv->tx_packets++;
    priv->tx_bytes += len;
    priv->last_len = len;
    priv->last_proto = ntohs(proto);   /* 转主机序存储 */
    /* 退出临界区，seqcount 变偶数 */
    u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage01_update_drop_stats：更新丢弃统计
 *
 * 【为什么单独一个函数？】
 *   → 与 update_tx_stats 对称，语义清晰
 *   → 如果后续要加丢包原因分类，只需改这一处
 */
static void stage01_update_drop_stats(struct stage01_priv *priv)
{
    unsigned long flags;

    flags = u64_stats_update_begin_irqsave(&priv->syncp);
    priv->tx_dropped++;
    u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/* ==================== 第6部分：ndo_open / ndo_stop ==================== */
/*
 * stage01_open：设备打开（ip link set up 触发）
 *
 * 【调用时机】
 *   用户执行：ip link set nds0 up
 *   内核调用链：rtnetlink_rcv → do_setlink → dev_change_flags → dev_open
 *   最终找到：ndev->netdev_ops->ndo_open(ndev)
 *
 * 【为什么 open 要增加 open_count？】
 *   → 教学目的：让用户能看到"确实触发了 ndo_open"
 *   → 真实网卡在 open 里会初始化硬件（PHY 复位、队列分配等）
 *
 * 【为什么直接 netif_carrier_on？】
 *   → 真实网卡有真实 PHY，载波状态由链路训练决定
 *   → stage01 没有真实 PHY，载波永远是 off
 *   → 如果不主动标记 carrier on，用户态 sendto() 会返回 ENETDOWN
 *   → 这是 stage01 的教学策略，不是生产网卡的写法
 *
 * 【netif_start_queue 的作用】
 *   → 告诉内核 TX 队列可以接受新包
 *   → 如果不调用，内核不会往这个接口发包
 *   → 与 netif_carrier_on 独立，但通常一起操作
 */
static int stage01_open(struct net_device *ndev)
{
    struct stage01_priv *priv = netdev_priv(ndev);
    unsigned long flags;

    /* 更新 open 计数（需要并发保护）*/
    flags = u64_stats_update_begin_irqsave(&priv->syncp);
    priv->open_count++;
    u64_stats_update_end_irqrestore(&priv->syncp, flags);

    /* 标记载波在线（无真实 PHY）*/
    netif_carrier_on(ndev);
    /* 开启 TX 队列（允许内核发包）*/
    netif_start_queue(ndev);

    netdev_info(ndev, "stage01 open: queue started, carrier on\n");
    return 0;
}

/*
 * stage01_stop：设备关闭（ip link set down 触发）
 *
 * 【调用时机】
 *   用户执行：ip link set nds0 down
 *   内核调用链：rtnetlink_rcv → do_setlink → dev_change_flags → dev_close
 *   最终找到：ndev->netdev_ops->ndo_stop(ndev)
 *
 * 【为什么顺序是 stop_queue → carrier_off？】
 *   → 先停止接受新包，再标记载波离线
 *   → 防止在标记载波离线期间，内核还在尝试发包
 *
 * 【为什么 stop 要增加 stop_count？】
 *   → 与 open_count 对称，教学目的
 */
static int stage01_stop(struct net_device *ndev)
{
    struct stage01_priv *priv = netdev_priv(ndev);
    unsigned long flags;

    /* 先停止 TX 队列，再标记载波离线 */
    netif_stop_queue(ndev);
    netif_carrier_off(ndev);

    /* 更新 stop 计数 */
    flags = u64_stats_update_begin_irqsave(&priv->syncp);
    priv->stop_count++;
    u64_stats_update_end_irqrestore(&priv->syncp, flags);

    netdev_info(ndev, "stage01 stop: queue stopped, carrier off\n");
    return 0;
}

/* ==================== 第7部分：ndo_start_xmit ==================== */
/*
 * stage01_start_xmit：发包入口（用户态发包触发）
 *
 * 【调用时机】
 *   用户态：sendto(socket_fd, frame, frame_len, 0, addr, addr_len)
 *   内核链路：
 *     sys_sendto → sock_sendmsg → inet_sendmsg → udp_sendmsg
 *     → __dev_queue_xmit() → netdev_start_xmit() → ops->ndo_start_xmit()
 *
 * 【stage01 的教学策略：为什么直接消费 skb？】
 *   → stage01 的目标不是"真正把包发到网络上"
 *   → stage01 的目标是证明"用户态发包确实走到了驱动入口"
 *   → 用 dev_consume_skb_any() 模拟"已发送并消费"
 *   → 后续 stage02~04 会逐步加上真实路径（skb clone、DMA 映射等）
 *
 * 【dev_consume_skb_any vs dev_kfree_skb_any 的区别】
 *   → dev_consume_skb_any()：优先 fast path（直接 free pages），TX 路径用它更快
 *   → dev_kfree_skb_any()：优先 slow path（归还 skb 缓存池）
 *   → start_xmit 在软中断上下文，用 _any 版本
 *
 * 【NETDEV_TX_OK vs NETDEV_TX_BUSY】
 *   → NETDEV_TX_OK (0)：发送成功（或已消费），TX 路径清空
 *   → NETDEV_TX_BUSY (1)：设备忙，调用方应重新入队
 *   → stage01 的 queue 永远不会满，所以返回 OK
 *
 * 【防御性检查：unlikely(!skb)】
 *   → skb 通常不为 NULL，但防御性检查是好的习惯
 *   → unlikely() 提示编译器这个条件通常为 false，优化分支预测
 */
static netdev_tx_t stage01_start_xmit(struct sk_buff *skb,
                                      struct net_device *ndev)
{
    struct stage01_priv *priv = netdev_priv(ndev);

    /* 防御性检查：skb 不应该为 NULL */
    if (unlikely(!skb)) {
        stage01_update_drop_stats(priv);
        return NETDEV_TX_OK;
    }

    /* 更新统计：长度、ETHERTYPE */
    stage01_update_tx_stats(priv, skb->len, skb->protocol);

    netdev_dbg(ndev, "stage01 xmit: len=%u proto=0x%04x\n",
               skb->len, ntohs(skb->protocol));

    /*
     * 核心教学操作：消费 skb
     *
     * stage01 不做真实发送，只是证明"包确实走到了这里"。
     * 真实网卡在这里做 DMA 映射、放入 TX ring、触发发送。
     * stage02 会在这里加 skb_clone()，为 TX 完成保留原始 skb。
     */
    dev_consume_skb_any(skb);
    return NETDEV_TX_OK;
}

/* ==================== 第8部分：ndo_get_stats64 ==================== */
/*
 * stage01_get_stats64：查询设备统计（ip -s link show 触发）
 *
 * 【调用时机】
 *   用户执行：ip -s link show nds0
 *   内核调用 rtnetlink_fill_ifstats() → ndo_get_stats64()
 *
 * 【为什么用 do-while 而不是直接赋值？】
 *   → 如果在读取过程中 ndo_start_xmit 写入了统计
 *   → u64_stats_fetch_retry() 会检测到 seqcount 变化
 *   → 整个 do-while 重新读一遍，直到读到一致的快照
 *
 * 【为什么不填充 rx 相关字段？】
 *   → stage01 还没有 RX 路径（留到 stage02）
 *   → tx 相关字段由 start_xmit 更新
 *
 * 【struct rtnl_link_stats64 的来源】
 *   → include/linux/rtnl.h
 *   → 内核网络栈的标准统计结构体
 *   → ip -s link show 读取的就是这个结构体
 */
static void stage01_get_stats64(struct net_device *ndev,
                                struct rtnl_link_stats64 *stats)
{
    struct stage01_priv *priv = netdev_priv(ndev);
    unsigned int start;
    u64 tx_packets, tx_bytes, tx_dropped;

    /* 读侧：seqcount 保护下的快照读取 */
    do {
        start = u64_stats_fetch_begin(&priv->syncp);
        tx_packets = priv->tx_packets;
        tx_bytes   = priv->tx_bytes;
        tx_dropped = priv->tx_dropped;
    } while (u64_stats_fetch_retry(&priv->syncp, start));

    /* 填充标准统计字段 */
    stats->tx_packets = tx_packets;
    stats->tx_bytes   = tx_bytes;
    stats->tx_dropped = tx_dropped;
}

/* ==================== 第9部分：net_device_ops ==================== */
/*
 * stage01_netdev_ops：驱动回调操作集
 *
 * 【什么是 net_device_ops？】
 *   → 内核网络设备的核心操作表
 *   → 类似于 file_operations，但针对网络设备
 *   → 内核通过 ops->ndo_xxx 调用驱动的具体实现
 *
 * 【为什么 eth_validate_addr / eth_mac_addr 不用自己实现？】
 *   → etherdevice.h 提供了通用实现
 *   → ether_setup() 已经设置好了标准以太网地址验证和设置逻辑
 *   → stage01 直接引用通用实现即可
 *
 * 【stage01 没有实现 ndo_set_rx_mode】
 *   → 这是因为 stage01 没有真实 RX，不需要多播过滤
 *   → 后续 stage04 virtio-net 会对接这个回调
 */
static const struct net_device_ops stage01_netdev_ops = {
    .ndo_open            = stage01_open,
    .ndo_stop            = stage01_stop,
    .ndo_start_xmit      = stage01_start_xmit,
    .ndo_get_stats64     = stage01_get_stats64,
    .ndo_validate_addr   = eth_validate_addr,  /* 以太网地址验证（通用实现）*/
    .ndo_set_mac_address = eth_mac_addr,        /* 以太网地址设置（通用实现）*/
};

/* ==================== 第10部分：debugfs seq_file 导出 ==================== */
/*
 * debugfs 导出设计的核心问题：
 *   为什么用 debugfs 而不是 sysfs？
 *     → sysfs 有严格规范，每个属性需要 show/store 函数指针
 *     → debugfs 没有格式限制，直接暴露 seq_file，更灵活
 *     → stage01 需要导出非标准字段（open_count, last_proto 等）
 *     → 这些不是内核标准统计，无法通过 ip -s link 读取
 */

/*
 * stage01_stats_show：seq_file 的 show 回调
 *
 * 【seq_file 的工作原理】
 *   1. single_open(file, stage01_stats_show, priv)
 *      → 把 priv 存入 seq_file->private
 *   2. read() 调用时，内核循环调用 show()
 *      → 直到 show() 返回 0（EOF）
 *   3. seq_printf() 类似 snprintf，但专门用于 seq_file
 *
 * 【为什么要用 do-while 读取统计？】
 *   → 与 ndo_get_stats64 相同的并发保护问题
 *   → debugfs read() 在进程上下文，start_xmit 在软中断上下文
 *   → seqcount 保护确保读到一致的快照
 */
static int stage01_stats_show(struct seq_file *m, void *v)
{
    struct stage01_priv *priv = m->private;
    unsigned int start;
    u64 open_count, stop_count;
    u64 tx_packets, tx_bytes, tx_dropped;
    u64 last_proto, last_len;

    do {
        start = u64_stats_fetch_begin(&priv->syncp);
        open_count  = priv->open_count;
        stop_count  = priv->stop_count;
        tx_packets  = priv->tx_packets;
        tx_bytes    = priv->tx_bytes;
        tx_dropped  = priv->tx_dropped;
        last_proto  = priv->last_proto;
        last_len    = priv->last_len;
    } while (u64_stats_fetch_retry(&priv->syncp, start));

    /* 格式化输出，每行一个字段 */
    seq_printf(m, "ifname=%s\n",        priv->ndev->name);
    seq_printf(m, "open_count=%llu\n",  open_count);
    seq_printf(m, "stop_count=%llu\n",  stop_count);
    seq_printf(m, "tx_packets=%llu\n",  tx_packets);
    seq_printf(m, "tx_bytes=%llu\n",    tx_bytes);
    seq_printf(m, "tx_dropped=%llu\n",  tx_dropped);
    seq_printf(m, "last_len=%llu\n",    last_len);
    seq_printf(m, "last_proto=0x%04llx\n", last_proto);
    return 0;
}

/*
 * stage01_stats_open：seq_file 的 open 回调
 *
 * single_open() 替代了传统的 open() 实现：
 *   → 不需要自己实现 open/release/read/llseek
 *   → single_open 帮我们处理了文件打开和迭代逻辑
 *   → 只需要提供 show() 回调
 */
static int stage01_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage01_stats_show, inode->i_private);
}

/*
 * stage01_stats_fops：debugfs 文件操作集
 *
 * 【owner = THIS_MODULE 的作用】
 *   → 防止模块卸载时，还有文件被打开引用本模块
 *   → 内核会在模块卸载前检查文件引用计数
 */
static const struct file_operations stage01_stats_fops = {
    .owner   = THIS_MODULE,
    .open    = stage01_stats_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/*
 * stage01_debugfs_init：创建 debugfs 目录和统计文件
 *
 * 【debugfs 目录结构】
 *   /sys/kernel/debug/netdev_stage01/stats
 *            └─ debugfs_create_dir   └─ debugfs_create_file
 *
 * 【为什么单独保存 dbg_dir？】
 *   → 卸载时需要 debugfs_remove_recursive(dbg_dir)
 *   → 递归删除目录及目录下所有文件
 *   → 不能依赖内核自动清理
 *
 * 【IS_ERR_OR_NULL 的防御性检查】
 *   → debugfs 在某些配置下可能不可用（CONFIG_DEBUG_FS=n）
 *   → 如果创建失败，打警告日志但继续运行
 *   → stage01 不强制依赖 debugfs，没有它也能跑
 */
static void stage01_debugfs_init(struct stage01_priv *priv)
{
    priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
    if (IS_ERR_OR_NULL(priv->dbg_dir)) {
        priv->dbg_dir = NULL;
        pr_warn(DRV_NAME ": debugfs unavailable, continue without it\n");
        return;
    }

    debugfs_create_file("stats", 0444, priv->dbg_dir, priv,
                        &stage01_stats_fops);
}

/*
 * stage01_debugfs_exit：清理 debugfs
 *
 * 【为什么要先判断 priv->dbg_dir？】
 *   → init 失败时 dbg_dir 为 NULL
 *   → debugfs_remove_recursive(NULL) 是安全的（空操作）
 *   → 但保留这个检查是好习惯
 *
 * 【为什么要把 dbg_dir 置 NULL？】
 *   → 防止卸载后其他代码误用已释放的指针
 *   → 防御性编程
 */
static void stage01_debugfs_exit(struct stage01_priv *priv)
{
    debugfs_remove_recursive(priv->dbg_dir);
    priv->dbg_dir = NULL;
}

/* ==================== 第11部分：stage01_setup ==================== */
/*
 * stage01_setup：net_device 初始化回调
 *
 * 【调用时机】
 *   alloc_netdev_mqs() 内部在分配内存后调用
 *   void *priv = netdev_priv(ndev);
 *   setup(ndev);  ← 这里调用 stage01_setup
 *
 * 【为什么用 setup 回调而不是直接在这里写？】
 *   → alloc_netdev_mqs 需要一个 setup 函数指针
 *   → setup 里先做通用初始化（ether_setup）
 *   → 再做驱动特定填充（netdev_ops）
 *   → 这样 setup 回调可以被不同驱动复用
 *
 * 【ether_setup 做了什么？】
 *   → dev->addr_len = ETH_ALEN (6)
 *   → dev->type = ARPHRD_ETHER (1)
 *   → dev->tx_queue_len = DEFAULT_TX_QUEUE_LEN (1000)
 *   → dev->broadcast = 全 ff
 *   → dev->dev_addr 初始化为 0
 *
 * 【IFF_NOARP 的含义？】
 *   → 告诉内核"这个设备不需要 ARP"
 *   → 不会触发 ARP 解析流程
 *   → virtio-net 默认也有这个标志
 *
 * 【NETIF_F_SG 的含义？】
 *   → 开启分散/聚集（Scatter-Gather）
 *   → 允许一个 skb 使用多个内存页（skb_shinfo->frags）
 *   → 后续 stage02 会用到
 */
static void stage01_setup(struct net_device *ndev)
{
    /* 以太网通用初始化（MAC 长度、类型、tx_queue_len 等）*/
    ether_setup(ndev);

    /* 填充本驱动的操作函数集 */
    ndev->netdev_ops = &stage01_netdev_ops;

    /* TX 看门狗超时 5 秒（通用以太网默认值）*/
    ndev->watchdog_timeo = msecs_to_jiffies(5000);

    /* MTU 范围 */
    ndev->min_mtu = 68;          /* 最小 MTU（IPv4）*/
    ndev->max_mtu = ETH_DATA_LEN; /* 最大 MTU（1500）*/

    /* 标志：不做 ARP（没有真实 PHY）*/
    ndev->flags |= IFF_NOARP;

    /* 特性：开启分散/聚集 */
    ndev->features |= NETIF_F_SG;

    /* 随机生成 MAC 地址（stage01 临时用）*/
    eth_hw_addr_random(ndev);
}

/* ==================== 第12部分：模块 init / exit ==================== */
/*
 * stage01_init：模块入口
 *
 * 【alloc_netdev_mqs 参数说明】
 *   sizeof(*priv)       ：priv 区域大小
 *   ifname              ：接口名（默认 "nds0"，可被模块参数覆盖）
 *   NET_NAME_UNKNOWN    ：命名空间类型（未知）
 *   stage01_setup       ：初始化回调（在上面定义）
 *   1, 1                ：TX 队列数=1，RX 队列数=1
 *
 * 【为什么用 alloc_netdev_mqs 而不是 alloc_etherdev_mqs？】
 *   → alloc_etherdev_mqs 内部直接调用 ether_setup，不够灵活
 *   → stage01 需要先调用 stage01_setup 再做额外设置
 *   → alloc_netdev_mqs 允许传入自定义 setup 回调
 *
 * 【为什么要 memset(priv, 0)？】
 *   → alloc_netdev() 只保证 net_device 部分被清零
 *   → priv 区域可能残留旧数据（从 Slab 缓存分配）
 *   → memset 0 确保 priv 从干净状态开始
 *
 * 【u64_stats_init 的作用】
 *   → 初始化 seqcount 为 0（偶数状态）
 *   → 必须在任何统计读写之前调用
 */
static int __init stage01_init(void)
{
    struct stage01_priv *priv;
    int ret;

    /* 分配 net_device + priv（TX/RX 各 1 个队列）*/
    stage01_dev = alloc_netdev_mqs(sizeof(*priv), ifname,
                                   NET_NAME_UNKNOWN, stage01_setup, 1, 1);
    if (!stage01_dev)
        return -ENOMEM;

    /* 获取 priv 指针并清零 */
    priv = netdev_priv(stage01_dev);
    memset(priv, 0, sizeof(*priv));
    priv->ndev = stage01_dev;

    /* 初始化 seqcount（必须在统计读写前调用）*/
    u64_stats_init(&priv->syncp);

    /* 注册到内核网络栈 */
    ret = register_netdev(stage01_dev);
    if (ret) {
        pr_err(DRV_NAME ": register_netdev failed: %d\n", ret);
        free_netdev(stage01_dev);
        stage01_dev = NULL;
        return ret;
    }

    /* 初始化 debugfs（可选，失败不影响设备运行）*/
    stage01_debugfs_init(priv);

    pr_info(DRV_NAME ": loaded, ifname=%s mac=%pM\n",
            stage01_dev->name, stage01_dev->dev_addr);
    return 0;
}

/*
 * stage01_exit：模块出口
 *
 * 【卸载顺序（关键：与 init 相反）】
 *   1. debugfs_remove_recursive  （清理调试文件）
 *   2. unregister_netdev         （从网络栈注销设备）
 *   3. free_netdev               （释放 net_device 和 priv）
 *
 * 【为什么顺序重要？】
 *   → unregister_netdev 会等待所有引用消失
 *     （正在执行 ndo_start_xmit 的软中断需要先返回）
 *   → free_netdev 必须在设备完全注销后才能调用
 *   → debugfs_remove 顺序无所谓，但通常最先做
 *
 * 【为什么要判断 stage01_dev？】
 *   → 防止重复卸载（rmmod 两次）
 *   → 第二次卸载时 stage01_dev 已经是 NULL
 */
static void __exit stage01_exit(void)
{
    struct stage01_priv *priv;

    if (!stage01_dev)
        return;

    priv = netdev_priv(stage01_dev);
    stage01_debugfs_exit(priv);       /* 第1步：清理 debugfs */
    unregister_netdev(stage01_dev);    /* 第2步：注销设备（会等待引用消失）*/
    free_netdev(stage01_dev);          /* 第3步：释放内存 */
    stage01_dev = NULL;
    pr_info(DRV_NAME ": unloaded\n");
}

module_init(stage01_init);
module_exit(stage01_exit);

MODULE_AUTHOR("OpenAI / ChatGPT");
MODULE_DESCRIPTION("stage01 teaching net_device skeleton for driver-lab netdev track");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：完整调用链图 ====================
 *
 * 【调用链 1：insmod → stage01_init → register_netdev】
 *
 *   用户态                    内核模块层                     网络设备层
 *   ======                    =============                   =============
 *
 *   insmod netdev_stage01.ko
 *     │
 *     │  ──────────────────────────────────────────────────► sys_init_module()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                             do_init_module()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                             module_init()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                    alloc_netdev_mqs(sizeof(*priv),
 *     │                                    ifname, NET_NAME_UNKNOWN,
 *     │                                    stage01_setup, 1, 1)
 *     │                                                    │
 *     │                                      ├→ stage01_setup()
 *     │                                      │      ├→ ether_setup()
 *     │                                      │      ├→ netdev_ops = &stage01_netdev_ops
 *     │                                      │      └→ eth_hw_addr_random()
 *     │                                      │
 *     │                                      ▼
 *     │                               register_netdev()
 *     │                                      │
 *     │                                      ├→ netdev_run_todo() [设备链表]
 *     │                                      ├→ netdev_wait_allrefs() [等待引用]
 *     │                                      └→ 触发 UDEV 事件
 *     │                                                    │
 *     │                                                    ▼
 *     │                                          debugfs_create_dir()
 *     │                                          debugfs_create_file("stats")
 *     │
 *     │  ◄───────────────────────────────────────────────── 返回 0
 *
 * 【调用链 2：ip link set up → ndo_open】
 *
 *   用户态                    内核 netdev 层                    驱动层
 *   ======                    ================                    ======
 *
 *   ip link set nds0 up
 *     │
 *     │  ─────────────────────────────────────────────────► rtnetlink_rcv()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                               do_setlink()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                          dev_change_flags()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                               dev_open()
 *     │                                                    │
 *     │                                      ├→ 等待 netdev 参考计数
 *     │                                      ├→ netdev->flags |= IFF_UP
 *     │                                      └→ 调用 ndo_open
 *     │                                                    │
 *     │                                                    ▼
 *     │                                    ndev->netdev_ops->ndo_open(ndev)
 *     │                                    stage01_open(ndev)  ────────────►
 *     │                                    ├→ netif_carrier_on()
 *     │                                    ├→ netif_start_queue()
 *     │                                    └→ open_count++
 *
 * 【调用链 3：sendto → ndo_start_xmit】
 *
 *   用户态                    内核网络栈                        驱动层
 *   ======                    ===========                        ======
 *
 *   sendto(socket_fd, frame, len, 0, addr, addr_len)
 *     │
 *     │  ─────────────────────────────────────────────────► sys_sendto()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                             sock_sendmsg()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                          sock->ops->sendmsg()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                             inet_sendmsg()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                    udp_sendmsg() [ETHERTYPE 查表]
 *     │                                                    │
 *     │                                                    ▼
 *     │                                      __dev_queue_xmit()  ← 关键入口
 *     │                                                    │
 *     │                                                    ▼
 *     │                                      validate_xmit_skb()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                      netdev_start_xmit()
 *     │                                                    │
 *     │                                                    ▼
 *     │                               ops->ndo_start_xmit(skb, dev)
 *     │                               stage01_start_xmit(skb, ndev)
 *     │                                    ├→ stage01_update_tx_stats()
 *     │                                    └→ dev_consume_skb_any(skb)
 *     │                                                     │
 *     │                                                     ▼
 *     │                                              返回 NETDEV_TX_OK
 *
 * 【调用链 4：cat /sys/kernel/debug/.../stats → debugfs】
 *
 *   用户态                    内核 VFS 层                       debugfs
 *   ======                    ===========                        =======
 *
 *   cat /sys/kernel/debug/netdev_stage01/stats
 *     │
 *     │  ─────────────────────────────────────────────────► sys_open()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                               do_dentry_open()
 *     │                                                    │
 *     │                                                    ▼
 *     │                                          debugfs_create_file()
 *     │                                          注册的 fops
 *     │                                                    │
 *     │                                                    ▼
 *     │                                          single_open(file,
 *     │                                          stage01_stats_show,
 *     │                                          inode->i_private)
 *     │                                          返回 seq_file*
 *     │
 *     ▼
 *   read() 循环：
 *     │
 *     │  ─────────────────────────────────────────────────► stage01_stats_show()
 *     │                                                    │
 *     │                                    读取 priv->tx_packets
 *     │                                    （do-while + seqcount 保护）
 *     │                                                    │
 *     │                                                    ▼
 *     │                                          seq_printf(m, "tx_packets=%llu\n")
 *     │
 *     │  ◄───────────────────────────────────────────────── 返回用户缓冲区
 */
