// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage02.c - stage02 skb path / software loopback teaching driver
 *
 * ==================== 文件定位 ====================
 *
 * stage01 已经证明：最小 net_device 能被 register_netdev() 注册出来，
 * 也能在 ndo_start_xmit() 里看到用户态发来的帧。
 *
 * stage02 再往前走一步：
 *   1. 在 start_xmit() 里真正把 skb 当作核心数据对象看待
 *   2. 把原始 TX skb 通过 copy/clone 方式构造成一份 RX skb
 *   3. 用 netif_rx() 把这份 RX skb 再送回协议栈
 *   4. 让用户态 sender / receiver 之间形成教学型软件闭环
 *
 * 【注意】这不是"真实网卡收包模型"，因为：
 *   - 没有硬件 DMA
 *   - 没有 descriptor ring
 *   - 没有 NAPI poll
 *
 * stage02 的边界：
 *   → 先把 skb、TX/RX 交接和 software loopback 吃透
 *   → stage03 再讲 NAPI
 *   → stage04 再讲 ring / DMA / RX replenishment
 *
 * ==================== 代码结构 ====================
 *
 *  第1部分: 头文件（第28~36行）
 *  第2部分: 版本兼容宏（第44~58行）      ← stage01 踩过的坑，stage02 直接做兼容
 *  第3部分: 模块参数（第60~67行）        ← ifname / loop_mode 可在加载时指定
 *  第4部分: priv 数据结构（第68~92行）  ← 所有统计计数、loop_mode 状态
 *  第5部分: 统计更新函数（第96~175行）   ← note_* 系列 + u64_stats 保护
 *  第6部分: 核心环回函数（第177~217行）  ← stage02_build_rx_skb() ★
 *  第7部分: netdev_ops 实现（第219~273行）
 *  第8部分: debugfs 统计（第299~367行）
 *  第9部分: 模块 init/exit（第377~426行）
 *
 * ==================== 核心学习点 ====================
 *
 *  1. skb_clone vs skb_copy 的本质区别
 *  2. skb_orphan / eth_type_trans / netif_rx 的 RX 注入三连
 *  3. ndo_start_xmit 的返回语义（NETDEV_TX_OK vs NETDEV_TX_BUSY）
 *  4. u64_stats 并发保护（来自 stage01 的真实教训）
 */

/* ==================== 第1部分：头文件 ==================== */
/*
 * 【头文件选择说明】
 *
 * <linux/debugfs.h>
 *   → debugfs_create_dir() / debugfs_create_file()
 *   → 用于导出 stats 到 /sys/kernel/debug/netdev_stage02/stats
 *
 * <linux/etherdevice.h>
 *   → ether_setup()：用标准以太网参数初始化 net_device
 *   → eth_hw_addr_random()：生成随机 MAC 地址
 *   → eth_type_trans()：解析以太头，确定上层协议 ★
 *
 * <linux/skbuff.h>
 *   → struct sk_buff：网络栈核心数据对象
 *   → skb_clone() / skb_copy()：复制 skb
 *   → skb_orphan()：断开 socket 关联
 *   → netif_rx()：将 skb 注入 RX 路径
 *   → dev_consume_skb_any()：消费 skb
 *
 * <linux/netdevice.h>
 *   → struct net_device：网络设备抽象
 *   → alloc_etherdev_mqs()：分配以太网设备
 *   → register_netdev() / unregister_netdev()
 *   → netif_carrier_on/off()：载波状态
 *   → netif_start/stop_queue()：TX 队列状态
 *
 * <linux/seq_file.h>
 *   → seq_read() / seq_printf()：debugfs 顺序文件操作
 *
 * <linux/u64_stats_sync.h>
 *   → u64_stats_sync：64位原子统计同步机制
 *   → stage01 真实遇到过 Linux 6.8 API 变更
 *
 * <linux/version.h>
 *   → LINUX_VERSION_CODE / KERNEL_VERSION：内核版本判断
 */
#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/u64_stats_sync.h>
#include <linux/version.h>

#define DRV_NAME "netdev_stage02"

/* ==================== 第2部分：版本兼容宏 ==================== */
/*
 * 【为什么需要这个兼容层？】
 *
 * stage01 测试时在 Ubuntu 22.04 / Linux 6.8 上遇到了真实问题：
 *   - u64_stats_update_begin_irqsave() 的返回类型变了
 *   - u64_stats_fetch_begin_irq() → u64_stats_fetch_begin（去掉了 _irq）
 *
 * 直接在代码里写 #if 判断，避免 stage02 再踩一次同样的坑。
 *
 * 【两套 API 对比】
 *
 *   Linux 5.x（旧）:
 *     u64_stats_update_begin_irqsave(&syncp, flags);  // flags 通过指针输出
 *     u64_stats_fetch_begin_irq(&syncp);              // 带 _irq 后缀
 *     u64_stats_fetch_retry_irq(&syncp, start);        // 带 _irq 后缀
 *
 *   Linux 6.8+（新）:
 *     flags = u64_stats_update_begin_irqsave(&syncp); // flags 通过返回值
 *     u64_stats_fetch_begin(&syncp);                   // 去掉 _irq 后缀
 *     u64_stats_fetch_retry(&syncp, start);            // 去掉 _irq 后缀
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE02_U64_UPDATE_BEGIN(syncp, flags) \
	do { (flags) = u64_stats_update_begin_irqsave((syncp)); } while (0)
#define STAGE02_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define STAGE02_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#else
#define STAGE02_U64_UPDATE_BEGIN(syncp, flags) \
	u64_stats_update_begin_irqsave((syncp), (flags))
#define STAGE02_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin_irq((syncp)); } while (0)
#define STAGE02_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry_irq((syncp), (start))
#endif

/* ==================== 第3部分：模块参数 ==================== */
/*
 * 【模块参数的作用】
 *
 * module_param_string 会：
 *   1. 创建一个 /sys/module/netdev_stage02/parameters/ifname
 *   2. 允许在 insmod 时指定：insmod netdev_stage02.ko ifname=nds2 loop_mode=clone
 *   3. 默认值在代码里指定
 *
 * loop_mode 的两种取值：
 *   "copy"  → skb_copy()，独立数据区，教学首选
 *   "clone" → skb_clone()，共享数据区，进阶理解引用计数
 */
static char ifname[IFNAMSIZ] = "nds2";
module_param_string(ifname, ifname, sizeof(ifname), 0644);
MODULE_PARM_DESC(ifname, "interface name for stage02 skb path net_device");

static char loop_mode[16] = "copy";
module_param_string(loop_mode, loop_mode, sizeof(loop_mode), 0644);
MODULE_PARM_DESC(loop_mode, "software loopback build mode: copy|clone");

/* ==================== 第4部分：priv 数据结构 ==================== */
/*
 * 【stage02_priv 字段分类】
 *
 * 设备信息：
 *   ndev        → 回指 net_device，TX/RX 都需要知道自己在哪个设备上操作
 *   dbg_dir     → debugfs 目录句柄
 *
 * TX 统计：
 *   tx_packets  → 发送帧数
 *   tx_bytes    → 发送字节数
 *   tx_dropped  → TX 丢包数（罕见，TX 路径一般不丢）
 *   last_tx_len → 最近一次 TX 帧长
 *   last_tx_proto → 最近一次 TX ETHERTYPE
 *
 * RX 统计：
 *   rx_packets  → 环回 RX 帧数
 *   rx_bytes    → 环回 RX 字节数
 *   rx_dropped  → RX 丢包数（netif_rx 返回 DROP）
 *   last_rx_len → 最近一次 RX 帧长
 *   last_rx_proto → 最近一次 RX ETHERTYPE
 *
 * 生命周期统计：
 *   open_count  → ndo_open 调用次数
 *   stop_count  → ndo_stop 调用次数
 *
 * 环回专用统计：
 *   loop_injected → 注入协议栈的总次数
 *   copy_built    → skb_copy() 调用次数
 *   clone_built   → skb_clone() 调用次数
 *   netif_rx_success → netif_rx() 返回非 DROP 的次数
 *   netif_rx_drop    → netif_rx() 返回 NET_RX_DROP 的次数
 *   last_netif_rx_rc → 最近一次 netif_rx() 的返回值
 *
 * 【为什么所有统计都是 u64？】
 *   → 网络设备TX/RX计数可能非常大（GB级别流量）
 *   → 32位 int 会溢出
 *   → u64_stats_sync 提供-reader-writer 保护，但不保证64位原子
 */
struct stage02_priv {
	struct net_device *ndev;       /* 回指设备，RX 注入时需要 */
	struct dentry *dbg_dir;        /* debugfs 目录 */
	struct u64_stats_sync syncp;   /* 并发保护同步锁 */

	/* TX 统计 */
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_dropped;
	u64 last_tx_len;
	u64 last_tx_proto;

	/* RX 统计 */
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_dropped;
	u64 last_rx_len;
	u64 last_rx_proto;

	/* 生命周期统计 */
	u64 open_count;
	u64 stop_count;

	/* 环回专用统计 */
	u64 loop_injected;      /* 总注入次数 */
	u64 copy_built;         /* skb_copy() 次数 */
	u64 clone_built;        /* skb_clone() 次数 */
	u64 netif_rx_success;   /* netif_rx 成功次数 */
	u64 netif_rx_drop;      /* netif_rx 返回 DROP 次数 */
	u64 last_netif_rx_rc;   /* 最近一次 netif_rx 返回码 */
};

/* 全局设备指针：stage02_init/exit 需要，简化资源管理 */
static struct net_device *stage02_dev;

/* ==================== 第5部分：统计更新函数 ==================== */
/*
 * 【note_* 系列函数的模式】
 *
 * 所有 note_* 函数遵循相同模式：
 *   1. STAGE02_U64_UPDATE_BEGIN → 获取写锁（禁用中断）
 *   2. 修改统计字段
 *   3. STAGE02_U64_UPDATE_END → 释放锁
 *
 * 【为什么要原子操作？】
 *   → 网络驱动的中断上半部（napi_disable）和下半部可能并发修改
 *   → ndo_open/ndo_stop 从一个核调用，ndo_start_xmit 从另一个核调用
 *   → 不保护会导致统计错乱
 */
static bool stage02_loop_mode_is_clone(void)
{
	return !strncmp(loop_mode, "clone", sizeof(loop_mode));
}

/*
 * stage02_note_open - 记录 ndo_open 调用
 *
 * 【调用时机】
 *   → 用户 ip link set nds2 up 时触发
 *   → netif_carrier_on + netif_start_queue 之后调用
 */
static void stage02_note_open(struct stage02_priv *priv)
{
	unsigned long flags;

	STAGE02_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->open_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage02_note_stop - 记录 ndo_stop 调用
 *
 * 【调用时机】
 *   → 用户 ip link set nds2 down 时触发
 *   → netif_stop_queue + netif_carrier_off 之后调用
 */
static void stage02_note_stop(struct stage02_priv *priv)
{
	unsigned long flags;

	STAGE02_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->stop_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage02_note_tx - 记录 TX 帧信息
 *
 * @len    → 帧长度（不含 CRC，skb->len 就是数据长度）
 * @proto  → ETHERTYPE（__be16 网络字节序）
 *
 * 【为什么记录 last_tx_proto？】
 *   → 用于 debugfs 查看"最近一次发的是什么协议"
 *   → ntohs() 转换为主机字节序存储
 */
static void stage02_note_tx(struct stage02_priv *priv, unsigned int len, __be16 proto)
{
	unsigned long flags;

	STAGE02_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_packets++;
	priv->tx_bytes += len;
	priv->last_tx_len = len;
	priv->last_tx_proto = ntohs(proto);
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage02_note_tx_drop - 记录 TX 丢包
 *
 * 【丢包场景】
 *   → skb 为 NULL（理论上不会发生，但防御性编程）
 *   → stage02 实际很少走到这里
 */
static void stage02_note_tx_drop(struct stage02_priv *priv)
{
	unsigned long flags;

	STAGE02_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage02_note_rx_inject - 记录 RX 注入结果
 *
 * @len           → rx_skb 的长度（注意：是 tx_skb->len，不是 rx_skb->len）
 * @proto         → ETHERTYPE（从 rx_skb->protocol 来）
 * @built_by_clone → true=clone 模式，false=copy 模式
 * @netif_rc      → netif_rx() 的返回值
 *
 * 【为什么要记录 ntohs(proto)？】
 *   → rx_skb->protocol 是 __be16（网络字节序）
 *   → debugfs 输出时需要主机字节序
 *
 * 【netif_rx() 返回值】
 *   → NET_RX_SUCCESS(0)  ：成功，协议栈会处理
 *   → NET_RX_DROP(1)    ：丢包，协议栈丢弃
 *   → NET_RX_CN_LOW(2)  ：拥塞等级低（softirq 队列慢）
 */
static void stage02_note_rx_inject(struct stage02_priv *priv,
				   unsigned int len, __be16 proto,
				   bool built_by_clone, int netif_rc)
{
	unsigned long flags;

	STAGE02_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->loop_injected++;                      /* 总注入次数 +1 */

	/* 根据 netif_rx 返回值更新 RX 统计 */
	if (netif_rc == NET_RX_DROP)
		priv->rx_dropped++;
	else {
		priv->rx_packets++;
		priv->rx_bytes += len;
	}

	/* 记录最近一次 RX 信息 */
	priv->last_rx_len = len;
	priv->last_rx_proto = ntohs(proto);
	priv->last_netif_rx_rc = netif_rc;

	/* 区分 copy/clone 模式 */
	if (built_by_clone)
		priv->clone_built++;
	else
		priv->copy_built++;

	/* 区分 netif_rx 成功/失败 */
	if (netif_rc == NET_RX_DROP)
		priv->netif_rx_drop++;
	else
		priv->netif_rx_success++;

	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage02_note_rx_alloc_fail - 记录 RX skb 分配失败
 *
 * 【丢包场景】
 *   → skb_clone() 或 skb_copy() 分配内存失败（-ENOMEM）
 *   → 这是 stage02 唯一常见的丢包场景
 */
static void stage02_note_rx_alloc_fail(struct stage02_priv *priv)
{
	unsigned long flags;

	STAGE02_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/* ==================== 第6部分：核心环回函数 ★ ==================== */
/*
 * stage02_build_rx_skb - 构建环回 RX skb
 *
 * 【函数功能】
 *   把原始 TX skb 转换为可以注入协议栈 RX 路径的 RX skb
 *
 * 【参数】
 *   tx_skb → ndo_start_xmit 收到的 TX skb（作为模板）
 *   priv   → 驱动私有数据
 *
 * 【返回值】
 *   >= 0   → netif_rx() 的返回值（成功或 DROP）
 *   < 0    → 分配失败（-ENOMEM）
 *
 * 【核心流程】
 *
 *   1. skb_clone() 或 skb_copy()  ← 用 tx_skb 构造 rx_skb
 *          ↓
 *   2. skb_orphan()                ← 断开原 socket 关联
 *          ↓
 *   3. rx_skb->dev = ndev          ← 设置目标设备
 *          ↓
 *   4. rx_skb->pkt_type = PACKET_HOST  ← 设为接收类型
 *          ↓
 *   5. eth_type_trans()            ← 解析以太头，确定 protocol
 *          ↓
 *   6. netif_rx(rx_skb)            ← 注入协议栈 RX 路径
 *
 * 【为什么选 PACKET_HOST？】
 *   → 这个包是要交给本机协议栈处理的
 *   → PACKET_HOST 表示"发往本机的包"
 *   → 其他选项：PACKET_BROADCAST（广播）、PACKET_MULTICAST（多播）
 *
 * 【eth_type_trans() 做了什么？】
 *   → 从 mac_header 位置读取 ETHERTYPE
 *   → 设置 skb->protocol = ethertype（网络字节序）
 *   → eth hdr 指向上层协议的起始位置（跳过以太网头）
 *   → 更新 skb->pkt_type（根据目的 MAC 判断）
 *
 * 【skb_orphan() 为什么需要？】
 *   → tx_skb 可能关联着某个 socket（如果是用户进程发的）
 *   → 如果不断开，rx_skb 析构时会尝试释放那个 socket 的引用
 *   → 断开后 rx_skb 变成"orphan skb"，由驱动负责最终释放
 *
 * 【netif_rx() vs netif_receive_skb()】
 *   → netif_rx()：非 NAPI 驱动使用，触发 NET_RX_SOFTIRQ
 *   → netif_receive_skb()：NAPI 驱动使用，poll 上下文直接处理
 *   → stage02 没有 NAPI，所以用 netif_rx()
 *   → stage03 会详细讲两者的区别
 */
static int stage02_build_rx_skb(struct sk_buff *tx_skb,
					    struct stage02_priv *priv)
{
	struct sk_buff *rx_skb;
	bool built_by_clone = stage02_loop_mode_is_clone();
	__be16 rx_proto;
	int rc;

	/*
	 * 【skb_copy vs skb_clone 的选择】
	 *
	 * skb_copy():
	 *   → 分配全新 skb + 复制数据区（memcpy）
	 *   → 两份独立数据，互不影响
	 *   → 内存成本高，但语义直观
	 *   → 适合教学："发一帧，收一帧，两份独立"
	 *
	 * skb_clone():
	 *   → 分配全新 skb 头
	 *   → 数据区共享（skb_shinfo refcount +1）
	 *   → 内存成本低，但要理解"共享 + 引用计数"
	 *   → 适合进阶：理解 skb 生命周期
	 *
	 * 【GFP_ATOMIC 的含义】
	 *   → ATOMIC 上下文可以睡眠
	 *   → start_xmit 可能运行在软中断上下文，所以用 ATOMIC
	 *   → 如果失败，只能返回 -ENOMEM
	 */
	if (built_by_clone)
		rx_skb = skb_clone(tx_skb, GFP_ATOMIC);
	else
		rx_skb = skb_copy(tx_skb, GFP_ATOMIC);

	if (!rx_skb)
		return -ENOMEM;

	/*
	 * ====== 以下开始把 skb 从"TX 上下文"转换为"RX 上下文" ======
	 *
	 * 目标：让协议栈把这个包当作正常 RX 包处理
	 *       而不是"刚从本机发出去的包"
	 */

	/*
	 * 【skb_orphan()】
	 *   → 清除 skb->sk（socket 指针）
	 *   → 断开与原 socket 的关联
	 *   → 防止包在 RX 路径被错误地关联到发送 socket
	 */
	skb_orphan(rx_skb);

	/*
	 * 【rx_skb->dev = ndev】
	 *   → 告诉协议栈这个包是从哪个设备进来的
	 *   → 协议栈需要这个信息做路由决策
	 */
	rx_skb->dev = priv->ndev;

	/*
	 * 【rx_skb->pkt_type = PACKET_HOST】
	 *   → PACKET_HOST：发往本机的包（正常 RX）
	 *   → PACKET_OUTGOING：从本机发出的包（TX 路径）
	 *   → PACKET_BROADCAST：广播包
	 *   → PACKET_MULTICAST：多播包
	 *   → 其他还有 PACKET_OTHERHOST（发往其他主机的混杂包）
	 */
	rx_skb->pkt_type = PACKET_HOST;

	/*
	 * 【rx_skb->skb_iif = 0】
	 *   → skb_iif：接收接口索引
	 *   → 0 表示"未知接口"，真实驱动会设置为实际 ifindex
	 *   → 用于 traceroute 等工具记录包经过的接口
	 */
	rx_skb->skb_iif = 0;

	/*
	 * 【rx_skb->ip_summed = CHECKSUM_UNNECESSARY】
	 *   → 告诉协议栈 checksum 已经验证过，不需要再校验
	 *   → 其他选项：
	 *     - CHECKSUM_NONE：需要校验
	 *     - CHECKSUM_UNNECESSARY：已校验
	 *     - CHECKSUM_COMPLETE：本地计算的 checksum
	 *   → stage02 是软件环回，所以设为 UNNECESSARY
	 */
	rx_skb->ip_summed = CHECKSUM_UNNECESSARY;

	/*
	 * 【eth_type_trans() - 解析以太头 ★】
	 *
	 * 这是驱动向协议栈交接的关键一步：
	 *   1. 从 skb->data（当前是 mac_header）读取 ETHERTYPE
	 *   2. 设置 skb->protocol = ETHERTYPE（网络字节序）
	 *   3. 根据 ETHERTYPE 推进 skb->data 到 L3 头位置
	 *   4. 根据目的 MAC 设置/验证 skb->pkt_type
	 *
	 * 调用后：
	 *   - skb->protocol = ethertype (e.g., 0x0800 for IPv4)
	 *   - skb->data 指向 L3 头（跳过 ETH_HLEN=14 字节）
	 *   - skb->pkt_type 已更新（eth_type_trans 会检查 MAC）
	 */
	skb_reset_mac_header(rx_skb);
	rx_skb->protocol = eth_type_trans(rx_skb, priv->ndev);
	rx_proto = rx_skb->protocol;

	/*
	 * 【netif_rx() - 注入协议栈 RX 路径 ★】
	 *
	 * 调用 netif_rx() 后，包进入协议栈的 RX 处理流程：
	 *
	 *   netif_rx(rx_skb)
	 *       ↓
	 *   ___netif_rx()  ← 开启 softirq（NET_RX_SOFTIRQ）
	 *       ↓
	 *   协议栈在 softirq 上下文处理包（稍后）
	 *       ↓
	 *   根据 protocol 找到对应 handler（ip_rcv() / arp_rcv() 等）
	 *       ↓
	 *   到达用户 socket（如果 AF_PACKET bind 了对应 ethertype）
	 *
	 * 【返回值】
	 *   NET_RX_SUCCESS(0)  ：成功排队，等待处理
	 *   NET_RX_DROP(1)     ：丢包（内存不足等）
	 *   NET_RX_CN_LOW(2)   ：拥塞，队列慢
	 */
	rc = netif_rx(rx_skb);

	/*
	 * 【统计记录】
	 *
	 * 注意：传的是 tx_skb->len，不是 rx_skb->len
	 * 因为 copy/clone 后长度没变，但明确一下语义
	 */
	stage02_note_rx_inject(priv, tx_skb->len, rx_proto,
			      built_by_clone, rc);

	return rc;
}

/* ==================== 第7部分：netdev_ops 实现 ==================== */
/*
 * 【netdev_ops 结构体】
 *
 * net_device 的操作函数表，类似于文件操作结构体 file_operations。
 * 内核在适当时候调用对应的函数。
 */

/*
 * stage02_open - ndo_open 实现
 *
 * 【调用时机】
 *   用户执行：ip link set nds2 up
 *
 * 【做了什么】
 *   1. stage02_note_open：记录打开次数
 *   2. netif_carrier_on：通知内核载波正常（链路 UP）
 *   3. netif_start_queue：允许 TX 队列开始发送
 *
 * 【为什么需要 netif_carrier_on？】
 *   → 某些协议栈功能依赖载波状态
 *   → 载波 off 时，协议栈不会尝试发送
 *   → 模拟真实网卡：网线插上 = carrier on
 */
static int stage02_open(struct net_device *ndev)
{
	struct stage02_priv *priv = netdev_priv(ndev);

	stage02_note_open(priv);
	netif_carrier_on(ndev);
	netif_start_queue(ndev);

	netdev_info(ndev, "stage02 open: queue started, carrier on, loop_mode=%s\n",
		    stage02_loop_mode_is_clone() ? "clone" : "copy");
	return 0;
}

/*
 * stage02_stop - ndo_stop 实现
 *
 * 【调用时机】
 *   用户执行：ip link set nds2 down
 *
 * 【做了什么】
 *   1. netif_stop_queue：停止 TX 队列（不允许再发送）
 *   2. netif_carrier_off：通知内核载波丢失
 *   3. stage02_note_stop：记录关闭次数
 *
 * 【与 open 对称】
 *   open 做的每件事，stop 都要反过来做：
 *   - netif_start_queue → netif_stop_queue
 *   - netif_carrier_on → netif_carrier_off
 *   - note_open → note_stop
 */
static int stage02_stop(struct net_device *ndev)
{
	struct stage02_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);
	stage02_note_stop(priv);

	netdev_info(ndev, "stage02 stop: queue stopped, carrier off\n");
	return 0;
}

/*
 * stage02_start_xmit - ndo_start_xmit 实现 ★
 *
 * 【TX 路径的入口函数】
 *
 * 内核协议栈在需要发送数据时调用此函数：
 *   协议栈 → __dev_queue_xmit() → ndo_start_xmit()
 *
 * 【返回值语义】
 *   NETDEV_TX_OK：
 *     → 驱动已消费 skb（或成功交付给硬件）
 *     → 不需要协议栈保留副本
 *     → 这是 stage02 的返回值，因为我们消费了 skb
 *
 *   NETDEV_TX_BUSY：
 *     → 硬件 TX 队列满，暂时不能发送
 *     → 协议栈会稍后重传（通过 qdisc 重排队）
 *     → 真实网卡在硬件队列满时返回这个
 *
 * 【skb 处理流程】
 *
 *   1. 检查 skb 是否为 NULL（防御性）
 *   2. 记录 TX 统计
 *   3. 调用 stage02_build_rx_skb() 构造环回 RX skb
 *   4. 如果构造失败：记录丢包，释放 skb
 *   5. 如果构造成功：消费原始 TX skb（使命完成）
 *
 * 【为什么用 dev_consume_skb_any？】
 *   → _any 后缀表示可以在任何上下文调用（中断/非中断）
 *   → 其他变体：
 *     - dev_kfree_skb_any()：同 _any
 *     - dev_kfree_skb()：只能在进程上下文
 *   → start_xmit 可能在软中断上下文，所以用 _any
 */
static netdev_tx_t stage02_start_xmit(struct sk_buff *skb,
				      struct net_device *ndev)
{
	struct stage02_priv *priv = netdev_priv(ndev);
	int rx_rc;

	/* 【防御性检查】理论上 skb 不应该为 NULL */
	if (unlikely(!skb)) {
		stage02_note_tx_drop(priv);
		return NETDEV_TX_OK;  /* 仍然返回 OK，防止协议栈困惑 */
	}

	/* 1. 记录 TX 统计 */
	stage02_note_tx(priv, skb->len, skb->protocol);

	/* 2. 构造环回 RX skb */
	rx_rc = stage02_build_rx_skb(skb, priv);
	if (rx_rc < 0) {
		/* 分配失败：记录丢包，释放 skb */
		stage02_note_rx_alloc_fail(priv);
		stage02_note_tx_drop(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/*
	 * ====== 原始 TX skb 的使命完成 ======
	 *
	 * - 统计已经记录
	 * - RX skb 已经构造并注入
	 * → 可以安全消费
	 */
	dev_consume_skb_any(skb);
	return NETDEV_TX_OK;
}

/*
 * stage02_get_stats64 - ndo_get_stats64 实现
 *
 * 【cat /proc/net/dev 显示的统计数据从这里来】
 *
 * @stats → 内核填充的 rtnl_link_stats64 结构体
 *          函数返回后，内核会把 stats 复制到 /proc/net/dev
 *
 * 【为什么需要 do-while 循环？】
 *   → 读取 priv->tx_packets 等64位字段
 *   → 在读取过程中，另一个核可能在写
 *   → u64_stats_fetch_begin/retry 提供了 reader-writer 保护
 *
 * 【reader-writer 保护原理】
 *   → 写操作：获取锁（关闭中断），原子修改
 *   → 读操作：不获取锁，但用 seqcount 检测并发冲突
 *   → 如果检测到冲突，重试
 *   → 优点：读操作不会被写操作阻塞（高并发友好）
 */
static void stage02_get_stats64(struct net_device *ndev,
				struct rtnl_link_stats64 *stats)
{
	struct stage02_priv *priv = netdev_priv(ndev);
	unsigned int start;

	do {
		STAGE02_U64_FETCH_BEGIN(&priv->syncp, start);
		stats->tx_packets = priv->tx_packets;
		stats->tx_bytes   = priv->tx_bytes;
		stats->tx_dropped = priv->tx_dropped;
		stats->rx_packets = priv->rx_packets;
		stats->rx_bytes   = priv->rx_bytes;
		stats->rx_dropped = priv->rx_dropped;
	} while (STAGE02_U64_FETCH_RETRY(&priv->syncp, start));
}

/*
 * 【netdev_ops 结构体定义】
 *
 * 这是驱动的"函数指针表"，内核通过它调用驱动实现。
 * static const 意味着编译时绑定，不能运行时替换。
 */
static const struct net_device_ops stage02_netdev_ops = {
	.ndo_open       = stage02_open,
	.ndo_stop       = stage02_stop,
	.ndo_start_xmit = stage02_start_xmit,
	.ndo_get_stats64 = stage02_get_stats64,
};

/* ==================== 第8部分：debugfs 统计 ==================== */
/*
 * 【debugfs 统计输出】
 *
 * 输出到：/sys/kernel/debug/netdev_stage02/stats
 *
 * 使用 seq_file 接口：
 *   → single_open() 包装一层
 *   → stage02_stats_show() 实现具体输出
 *   → seq_printf() 格式化输出（类似 printf）
 *
 * 【为什么要用 seq_file？】
 *   → debugfs 文件可能被多次 open/read
 *   → seq_file 处理了偏移、边界等细节
 *   → 比直接实现 file_operations 简单
 */

/*
 * stage02_stats_show - debugfs 读取时的输出函数
 *
 * 【输出字段说明】
 *
 * ifname / loop_mode：设备基本信息
 * open_count / stop_count：生命周期计数
 *
 * tx_* / rx_*：收发统计（与 rtnl_link_stats64 对应）
 *
 * loop_injected：总环回注入次数
 *   = copy_built + clone_built
 *   = netif_rx_success + netif_rx_drop
 *
 * copy_built / clone_built：区分两种模式的使用次数
 *
 * netif_rx_success / netif_rx_drop：区分注入结果
 *
 * last_netif_rx_rc：最近一次返回值
 *   0 = NET_RX_SUCCESS
 *   1 = NET_RX_DROP
 *   2 = NET_RX_CN_LOW
 *
 * last_tx_len / last_rx_len：最近一次帧长
 * last_tx_proto / last_rx_proto：最近一次 ETHERTYPE（十六进制）
 */
static int stage02_stats_show(struct seq_file *m, void *v)
{
	struct net_device *ndev = m->private;
	struct stage02_priv *priv = netdev_priv(ndev);
	unsigned int start;
	u64 tx_packets, tx_bytes, tx_dropped;
	u64 rx_packets, rx_bytes, rx_dropped;
	u64 open_count, stop_count, loop_injected;
	u64 copy_built, clone_built, netif_rx_success, netif_rx_drop;
	u64 last_netif_rx_rc, last_tx_len, last_tx_proto, last_rx_len, last_rx_proto;

	/*
	 * 【统计读取需要 do-while 循环】
	 *   与 stage02_get_stats64 相同的原子读取逻辑
	 *   确保读取过程中不会被并发的 note_* 函数打断
	 */
	do {
		STAGE02_U64_FETCH_BEGIN(&priv->syncp, start);
		tx_packets = priv->tx_packets;
		tx_bytes = priv->tx_bytes;
		tx_dropped = priv->tx_dropped;
		rx_packets = priv->rx_packets;
		rx_bytes = priv->rx_bytes;
		rx_dropped = priv->rx_dropped;
		open_count = priv->open_count;
		stop_count = priv->stop_count;
		loop_injected = priv->loop_injected;
		copy_built = priv->copy_built;
		clone_built = priv->clone_built;
		netif_rx_success = priv->netif_rx_success;
		netif_rx_drop = priv->netif_rx_drop;
		last_netif_rx_rc = priv->last_netif_rx_rc;
		last_tx_len = priv->last_tx_len;
		last_tx_proto = priv->last_tx_proto;
		last_rx_len = priv->last_rx_len;
		last_rx_proto = priv->last_rx_proto;
	} while (STAGE02_U64_FETCH_RETRY(&priv->syncp, start));

	/* 【seq_printf 输出】 */
	seq_printf(m, "ifname=%s\n", ndev->name);
	seq_printf(m, "loop_mode=%s\n", stage02_loop_mode_is_clone() ? "clone" : "copy");
	seq_printf(m, "open_count=%llu\n", open_count);
	seq_printf(m, "stop_count=%llu\n", stop_count);
	seq_printf(m, "tx_packets=%llu\n", tx_packets);
	seq_printf(m, "tx_bytes=%llu\n", tx_bytes);
	seq_printf(m, "tx_dropped=%llu\n", tx_dropped);
	seq_printf(m, "rx_packets=%llu\n", rx_packets);
	seq_printf(m, "rx_bytes=%llu\n", rx_bytes);
	seq_printf(m, "rx_dropped=%llu\n", rx_dropped);
	seq_printf(m, "loop_injected=%llu\n", loop_injected);
	seq_printf(m, "copy_built=%llu\n", copy_built);
	seq_printf(m, "clone_built=%llu\n", clone_built);
	seq_printf(m, "netif_rx_success=%llu\n", netif_rx_success);
	seq_printf(m, "netif_rx_drop=%llu\n", netif_rx_drop);
	seq_printf(m, "last_netif_rx_rc=%llu\n", last_netif_rx_rc);
	seq_printf(m, "last_tx_len=%llu\n", last_tx_len);
	seq_printf(m, "last_tx_proto=0x%04llx\n", last_tx_proto);
	seq_printf(m, "last_rx_len=%llu\n", last_rx_len);
	seq_printf(m, "last_rx_proto=0x%04llx\n", last_rx_proto);

	return 0;
}

/*
 * stage02_stats_open - debugfs open 包装
 *
 * single_open() 会：
 *   → 分配一个 struct seq_file
 *   → 调用 stage02_stats_show() 填充内容
 *   → 用户 read 时调用 seq_read()
 */
static int stage02_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stage02_stats_show, inode->i_private);
}

/*
 * 【file_operations for debugfs stats】
 *
 * debugfs 文件不支持直接定义 .owner，所以用 THIS_MODULE 显式指定。
 * 其他函数都是 seq_file 标准的。
 */
static const struct file_operations stage02_stats_fops = {
	.owner = THIS_MODULE,
	.open = stage02_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* ==================== 第9部分：stage02_setup ==================== */
/*
 * stage02_setup - 初始化 net_device 的通用字段
 *
 * 【ether_setup 做了什么？】
 *   → 设置 dev->type = ARPHRD_ETHER（以太网）
 *   → 设置 dev->mtu = ETH_DATA_LEN（1500）
 *   → 设置 dev->flags |= IFF_BROADCAST | IFF_MULTICAST
 *   → 设置 dev->addr_len = ETH_ALEN（6字节 MAC）
 *   → 初始化 dev->broadcast 为全 ff
 *
 * 【IFF_NOARP 的含义】
 *   → 网络设备不支持 ARP 协议
 *   → stage01/stage02 都不需要真实 ARP
 *   → 真实网卡需要 ARP 来建立 IP-MAC 映射
 */
static void stage02_setup(struct net_device *ndev)
{
	ether_setup(ndev);                    /* 用以太网默认值初始化 */
	ndev->netdev_ops = &stage02_netdev_ops; /* 挂载操作函数表 */
	ndev->flags |= IFF_NOARP;              /* 不支持 ARP */
	eth_hw_addr_random(ndev);              /* 随机生成 MAC 地址 */
}

/* ==================== 第10部分：模块 init/exit ==================== */
/*
 * stage02_init - 模块入口
 *
 * 【初始化顺序（重要！）】
 *
 *   1. alloc_etherdev_mqs()     → 分配 net_device + priv
 *         ↓
 *   2. stage02_setup()          → 初始化 net_device 字段
 *         ↓
 *   3. strscpy(stage02_dev->name, ...) → 设置接口名
 *         ↓
 *   4. netdev_priv() + memset   → 初始化 priv 为零
 *         ↓
 *   5. u64_stats_init()          → 初始化统计同步锁
 *         ↓
 *   6. register_netdev()        → 注册设备（这之后 ip link 能看到）
 *         ↓
 *   7. debugfs_create_dir/file   → 创建 debugfs 条目
 *
 * 【如果顺序错了会怎样？】
 *   → register_netdev() 之前必须设置好 name
 *   → 否则 /sys/class/net/ 下名字不对
 *   → priv 必须初始化，否则 priv->syncp 未定义
 *
 * 【为什么用 alloc_etherdev_mqs？】
 *   → ether：分配以太网类型的 net_device
 *   → _mqs：Multiple Queues，支持多 TX 队列
 *   → stage02 用 1 个 TX 队列，1 个 RX 队列（简化）
 */
static int __init stage02_init(void)
{
	struct stage02_priv *priv;

	/* 1. 分配设备（包含 priv） */
	stage02_dev = alloc_etherdev_mqs(sizeof(struct stage02_priv), 1, 1);
	if (!stage02_dev)
		return -ENOMEM;

	/* 2. 初始化设备 */
	stage02_setup(stage02_dev);
	strscpy(stage02_dev->name, ifname, sizeof(stage02_dev->name));

	/* 3. 初始化 priv */
	priv = netdev_priv(stage02_dev);
	memset(priv, 0, sizeof(*priv));
	priv->ndev = stage02_dev;
	u64_stats_init(&priv->syncp);

	/* 4. 注册设备 */
	if (register_netdev(stage02_dev)) {
		free_netdev(stage02_dev);
		stage02_dev = NULL;
		return -EINVAL;
	}

	/* 5. 创建 debugfs 条目 */
	priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
	if (priv->dbg_dir)
		debugfs_create_file("stats", 0444, priv->dbg_dir,
				    stage02_dev, &stage02_stats_fops);

	netdev_info(stage02_dev, "loaded, ifname=%s mac=%pM loop_mode=%s\n",
		    stage02_dev->name, stage02_dev->dev_addr,
		    stage02_loop_mode_is_clone() ? "clone" : "copy");
	return 0;
}

/*
 * stage02_exit - 模块出口
 *
 * 【清理顺序（必须与 init 严格对称！）】
 *
 *   1. 检查 stage02_dev 是否存在
 *         ↓
 *   2. debugfs_remove_recursive() → 删除 debugfs 条目
 *         ↓
 *   3. unregister_netdev()        → 注销设备（这之后 ip link 看不到）
 *         ↓
 *   4. free_netdev()              → 释放设备内存
 *         ↓
 *   5. stage02_dev = NULL         → 防止 use-after-free
 *
 * 【unregister_netdev 做了什么？】
 *   → 如果设备还在 UP 状态，自动调用 ndo_stop
 *   → 从网络命名空间移除设备
 *   → 通知协议栈设备消失
 *
 * 【free_netdev 做了什么？】
 *   → 释放 net_device
 *   → 释放 priv（因为 priv 是 alloc_etherdev 一起分配的）
 *   → 释放设备名（如果是用 __alloc_name 分配的）
 */
static void __exit stage02_exit(void)
{
	struct stage02_priv *priv;

	if (!stage02_dev)
		return;

	priv = netdev_priv(stage02_dev);
	if (priv->dbg_dir)
		debugfs_remove_recursive(priv->dbg_dir);

	unregister_netdev(stage02_dev);
	free_netdev(stage02_dev);
	stage02_dev = NULL;

	pr_info(DRV_NAME ": unloaded\n");
}

module_init(stage02_init);
module_exit(stage02_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("stage02 skb path teaching driver with software TX/RX loopback");

/*
 * ==================== 附录：完整调用链 ====================
 *
 * 【stage02 完整数据流】
 *
 *   用户态 sendto()
 *       ↓
 *   AF_PACKET / SOCK_RAW
 *       ↓
 *   内核 __dev_queue_xmit()
 *       ↓
 *   ndo_start_xmit: stage02_start_xmit()
 *       ├→ stage02_note_tx()      ← TX 统计
 *       │
 *       ├→ stage02_build_rx_skb()
 *       │     ↓
 *       │   skb_clone/skb_copy    ← 构造 rx_skb
 *       │     ↓
 *       │   skb_orphan()         ← 断开 socket 关联
 *       │     ↓
 *       │   rx_skb->dev = ndev   ← 设置设备
 *       │     ↓
 *       │   rx_skb->pkt_type = PACKET_HOST
 *       │     ↓
 *       │   eth_type_trans()     ← 解析 ETHERTYPE
 *       │     ↓
 *       │   netif_rx(rx_skb)     ← ★ 注入 RX 路径
 *       │     ↓
 *       │   stage02_note_rx_inject() ← RX 统计
 *       │
 *       └→ dev_consume_skb_any(skb) ← 消费原始 TX skb
 *
 *       ↓
 *   协议栈 RX 路径（softirq）
 *       ↓
 *   用户态 recvfrom()
 *
 * ==================== 附录：与 stage01 的对比 ====================
 *
 *   stage01: ndo_start_xmit() → dev_consume_skb_any(skb) → 结束
 *   stage02: ndo_start_xmit() → build_rx_skb() → netif_rx() → RX 路径继续
 *
 *   stage01 是"看见"
 *   stage02 是"环回"
 *   stage03 是"NAPI 批处理"
 */
