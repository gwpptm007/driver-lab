// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage03.c - stage03 NAPI / poll 教学驱动
 *
 * ==================== 文件定位 ====================
 *
 * stage03 的目标不是引入真实硬件 ring，而是先把：
 *   1. pending queue（pending_rxq）
 *   2. irq only schedules NAPI（中断只负责通知，不做处理）
 *   3. poll drains queue with budget（poll 按 budget 批量处理）
 *   4. complete re-enables irq（完成后重新开中断）
 *
 * 这一层语义建立起来。
 *
 * ==================== 与 stage02 的关键区别 ====================
 *
 * stage02: TX → build_rx_skb → netif_rx() → 直接注入 RX
 * stage03: TX → build_rx_skb → enqueue → raise_irq → napi_schedule → poll → netif_receive_skb()
 *
 * ==================== 两种 RX 模式 ====================
 *
 * direct 模式（rx_mode=direct）：
 *   → 仍然可以立即 netif_rx()，便于与 stage02 对照
 *   → 无 pending queue，无 NAPI 语义
 *
 * napi 模式（rx_mode=napi）：
 *   → 完整的 NAPI 教学语义
 *   → pending_rxq 入队 / poll 出队 / netif_receive_skb()
 *   → 可观测 irq raise/mask/unmask / schedule / poll / complete
 *
 * ==================== 代码结构 ====================
 *
 *  1. 头文件 + 版本兼容宏（第1~50行）
 *  2. 模块参数（第52~65行）
 *  3. stage03_priv 私有数据结构（第66~119行）
 *  4. 模块参数解析辅助函数（第123~131行）
 *  5. 统计记录函数（第133~319行）
 *  6. skb 构造与 enqueue/drain（第321~427行）
 *  7. ndo_open/stop/start_xmit（第429~485行）
 *  8. get_stats64 + debugfs（第487~624行）
 *  9. net_device 注册/注销（第625~698行）
 */

/* ==================== 第1部分：头文件与版本兼容 ==================== */
/*
 * 【头文件说明】
 *
 * <linux/debugfs.h>
 *   → debugfs_create_dir() / debugfs_create_file()：导出调试信息
 *
 * <linux/etherdevice.h>
 *   → ether_setup()：初始化以太网卡通用字段
 *   → eth_hw_addr_random()：生成随机 MAC
 *
 * <linux/netdevice.h>
 *   → struct net_device / struct net_device_ops
 *   → struct napi_struct
 *   → netif_rx() / netif_receive_skb()
 *   → netif_napi_add_weight() / netif_napi_del()
 *   → napi_schedule_prep() / __napi_schedule()
 *   → napi_complete_done() / napi_enable() / napi_disable()
 *
 * <linux/skbuff.h>
 *   → struct sk_buff
 *   → skb_clone() / skb_copy() / skb_orphan()
 *   → eth_type_trans() / dev_consume_skb_any()
 *   → skb_queue_head_init() / skb_queue_tail() / skb_dequeue()
 *
 * <linux/spinlock.h>
 *   → spin_lock_init() / spin_lock_irqsave()：保护共享状态
 *
 * <linux/u64_stats_sync.h>
 *   → u64_stats_init() / u64_stats_update_begin/end_irqsave() / u64_stats_fetch_begin/retry()
 *   → 6.8 版本 u64_stats API 有变化，见下方版本宏
 *
 * <linux/version.h>
 *   → LINUX_VERSION_CODE / KERNEL_VERSION()：内核版本检测
 */
#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/u64_stats_sync.h>
#include <linux/version.h>

/* ==================== 第2部分：版本兼容宏 ==================== */
/*
 * 【u64_stats API 在 Linux 6.8 的变化】
 *
 * 6.8 之前：
 *   u64_stats_update_begin_irqsave(&syncp, flags)  ← 宏返回 void
 *   u64_stats_fetch_begin_irq(&syncp)              ← 返回 unsigned int
 *   u64_stats_fetch_retry_irq(&syncp, start)      ← 检查是否重试
 *
 * 6.8 起：
 *   u64_stats_update_begin_irqsave(&syncp)         ← 改为返回 unsigned long (flags)
 *   u64_stats_fetch_begin(&syncp)                 ← 不再需要 _irq 后缀
 *   u64_stats_fetch_retry(&syncp, start)          ← 不再需要 _irq 后缀
 *
 * 这导致 5.x 和 6.8 的代码不兼容，需要版本分支处理。
 */
#define DRV_NAME "netdev_stage03"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE03_U64_UPDATE_BEGIN(syncp, flags) \
	do { (flags) = u64_stats_update_begin_irqsave((syncp)); } while (0)
#define STAGE03_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define STAGE03_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#else
#define STAGE03_U64_UPDATE_BEGIN(syncp, flags) \
	u64_stats_update_begin_irqsave((syncp), (flags))
#define STAGE03_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin_irq((syncp)); } while (0)
#define STAGE03_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry_irq((syncp), (start))
#endif

/* ==================== 第3部分：模块参数 ==================== */
/*
 * 【模块参数说明】
 *
 * ifname：接口名，默认 nds3
 *
 * loop_mode：环回模式
 *   copy  → skb_copy()，独立数据区
 *   clone → skb_clone()，共享数据区（引用计数+1）
 *
 * rx_mode：RX 注入模式
 *   direct → 直接 netif_rx()，沿用 stage02 路径
 *   napi   → 入 pending_rxq → napi_schedule → poll → netif_receive_skb()
 *
 * napi_weight：每次 poll 最多处理的包数（默认 8）
 *   → 影响 budget_exhausted 是否触发
 *   → 值越小越容易触发 budget exhausted
 *
 * max_queue_depth：pending_rxq 最大深度（默认 1024）
 *   → 超过则丢弃入队请求
 */
static char ifname[IFNAMSIZ] = "nds3";
module_param_string(ifname, ifname, sizeof(ifname), 0644);
MODULE_PARM_DESC(ifname, "interface name for stage03 napi poll net_device");

static char loop_mode[16] = "copy";
module_param_string(loop_mode, loop_mode, sizeof(loop_mode), 0644);
MODULE_PARM_DESC(loop_mode, "software loopback build mode: copy|clone");

static char rx_mode[16] = "napi";
module_param_string(rx_mode, rx_mode, sizeof(rx_mode), 0644);
MODULE_PARM_DESC(rx_mode, "receive mode: direct|napi");

static int napi_weight = 8;
module_param(napi_weight, int, 0644);
MODULE_PARM_DESC(napi_weight, "napi poll weight");

static int max_queue_depth = 1024;
module_param(max_queue_depth, int, 0644);
MODULE_PARM_DESC(max_queue_depth, "max pending RX queue depth");

/* ==================== 第4部分：私有数据结构 ==================== */
/*
 * 【stage03_priv 结构解析】
 *
 * 核心字段：
 *   napi         → NAPI 描述符，关联到 net_device
 *   pending_rxq  → pending RX 队列，教学替身（代替硬件 RX ring）
 *   state_lock   → 保护 irq_masked 状态
 *   irq_masked   → 中断抑制状态标志
 *
 * TX 统计：
 *   tx_packets / tx_bytes / tx_dropped
 *
 * RX 统计（最终交付给协议栈的数量）：
 *   rx_packets / rx_bytes / rx_dropped
 *
 * skb 构造统计：
 *   copy_built / clone_built / build_failures
 *
 * 注入路径统计：
 *   direct_inject_count   → direct 模式注入次数
 *   napi_inject_count     → napi 模式注入次数（经过 poll）
 *   netif_rx_success/drop → netif_rx() 结果
 *   netif_receive_success/drop → netif_receive_skb() 结果
 *
 * pending queue 统计：
 *   pending_enqueued  → 入队总次数
 *   pending_drained    → poll 出队总次数
 *   pending_dropped   → 因队列满丢弃次数
 *   pending_peak      → 队列最大深度
 *
 * NAPI 统计：
 *   irq_raised_count       → 触发中断次数
 *   irq_masked_count       → 中断屏蔽次数
 *   irq_unmasked_count     → 中断恢复次数
 *   napi_schedule_count    → napi_schedule 次数
 *   napi_poll_count        → poll 被调用次数
 *   napi_complete_count    → napi_complete_done 次数
 *   napi_budget_exhaust_count → budget 用尽次数
 */
struct stage03_priv {
	struct net_device *ndev;
	struct dentry *dbg_dir;
	struct napi_struct napi;          /* NAPI 描述符 ★ */
	struct sk_buff_head pending_rxq;  /* pending RX 队列 ★ */
	spinlock_t state_lock;            /* 保护 irq_masked */
	struct u64_stats_sync syncp;
	bool irq_masked;                  /* 中断抑制状态 ★ */

	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_dropped;
	u64 last_tx_len;
	u64 last_tx_proto;

	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_dropped;
	u64 last_rx_len;
	u64 last_rx_proto;

	u64 open_count;
	u64 stop_count;

	u64 copy_built;
	u64 clone_built;
	u64 build_failures;

	u64 direct_inject_count;
	u64 napi_inject_count;
	u64 netif_rx_success;
	u64 netif_rx_drop;
	u64 netif_receive_success;
	u64 netif_receive_drop;
	u64 last_inject_rc;

	u64 pending_enqueued;
	u64 pending_drained;
	u64 pending_dropped;
	u64 pending_peak;
	u64 pending_last_depth;

	u64 irq_raised;
	u64 irq_masked_count;
	u64 irq_unmasked_count;

	u64 napi_schedule_count;
	u64 napi_poll_count;
	u64 napi_complete_count;
	u64 napi_budget_exhaust_count;
	u64 napi_work_total;
	u64 last_poll_budget;
	u64 last_poll_work;
};

static struct net_device *stage03_dev;

/* ==================== 第5部分：模块参数解析 ==================== */
/*
 * stage03_loop_mode_is_clone / stage03_rx_mode_is_napi
 *
 * 【为什么用 strncmp 而不是 strcmp？】
 *   strncmp 有长度限制，更安全
 *   loop_mode / rx_mode 是模块参数，值不可信
 */
static bool stage03_loop_mode_is_clone(void)
{
	return !strncmp(loop_mode, "clone", sizeof(loop_mode));
}

static bool stage03_rx_mode_is_napi(void)
{
	return !strncmp(rx_mode, "napi", sizeof(rx_mode));
}

/* ==================== 第6部分：统计记录函数 ==================== */
/*
 * 【统计记录的设计原则】
 *
 * 所有统计字段都是 u64，使用 u64_stats_sync 保护。
 * 写操作：STAGE03_U64_UPDATE_BEGIN → 修改 → STAGE03_U64_UPDATE_END_irqrestore
 * 读操作：STAGE03_U64_FETCH_BEGIN → 拷贝到局部变量 → STAGE03_U64_FETCH_RETRY 检查重排
 *
 * 这样确保：
 *   1. 64位计数器在 32位系统上不会因并发丢失更新
 *   2. 读取过程中数据不会被修改
 *
 * 【note 函数的命名规律】
 *   stage03_note_xxx → 原子地更新 xxx 相关的统计字段
 *   通常成对出现：enqueue/drained, irq_masked/unmasked, schedule/complete
 */
static void stage03_note_open(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->open_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_stop(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->stop_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_tx(struct stage03_priv *priv, unsigned int len, __be16 proto)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_packets++;
	priv->tx_bytes += len;
	priv->last_tx_len = len;
	priv->last_tx_proto = ntohs(proto);
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_tx_drop(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_build_failure(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->build_failures++;
	priv->rx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_build_success(struct stage03_priv *priv, bool built_by_clone)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	if (built_by_clone)
		priv->clone_built++;
	else
		priv->copy_built++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage03_note_enqueue - 记录 skb 入队
 *
 * 【关键】同时更新 peak 深度：pending_peak = max(pending_peak, depth)
 *   → 用于观测 burst 场景下队列最大积压深度
 */
static void stage03_note_enqueue(struct stage03_priv *priv, unsigned int depth)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->pending_enqueued++;
	priv->pending_last_depth = depth;
	if (depth > priv->pending_peak)
		priv->pending_peak = depth;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_pending_drop(struct stage03_priv *priv, unsigned int depth)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->pending_dropped++;
	priv->rx_dropped++;
	priv->pending_last_depth = depth;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_irq_raised(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->irq_raised++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_irq_masked(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->irq_masked_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_irq_unmasked(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->irq_unmasked_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_napi_schedule(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_schedule_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage03_note_napi_poll - 记录 poll 调用
 *
 * 【参数】
 *   budget_exhausted = work_done >= budget && queue 还有包
 *   → 这种情况意味着"本次 poll 用完了 budget 但没清空队列"
 *   → 下一次 poll 会被继续安排
 */
static void stage03_note_napi_poll(struct stage03_priv *priv, int budget, int work_done,
				   bool budget_exhausted)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_poll_count++;
	priv->napi_work_total += work_done;
	priv->last_poll_budget = budget;
	priv->last_poll_work = work_done;
	priv->pending_last_depth = skb_queue_len(&priv->pending_rxq);
	if (budget_exhausted)
		priv->napi_budget_exhaust_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage03_note_napi_complete(struct stage03_priv *priv)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_complete_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage03_note_direct_inject - 记录 direct 模式注入
 *
 * 【与 napi_inject 的区别】
 *   direct:   → netif_rx() 直接注入
 *   napi:     → netif_receive_skb() 在 poll 上下文注入
 *
 * netif_rx() 返回 NET_RX_DROP / NET_RX_SUCCESS / NET_RX_CONE_LOW
 *   → 记录 last_inject_rc 用于调试
 */
static void stage03_note_direct_inject(struct stage03_priv *priv, unsigned int len,
			       __be16 proto, int rc)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->direct_inject_count++;
	priv->last_rx_len = len;
	priv->last_rx_proto = ntohs(proto);
	priv->last_inject_rc = rc;
	if (rc == NET_RX_DROP) {
		priv->netif_rx_drop++;
		priv->rx_dropped++;
	} else {
		priv->netif_rx_success++;
		priv->rx_packets++;
		priv->rx_bytes += len;
	}
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * stage03_note_napi_inject - 记录 napi 模式注入（poll 出队后）
 *
 * 与 direct_inject 的区别：
 *   1. 调用时机：在 poll 循环里，每出队一帧调用一次
 *   2. 注入 API：netif_receive_skb()（不是 netif_rx()）
 *   3. 统计归类：计入 napi_inject_count 和 pending_drained
 */
static void stage03_note_napi_inject(struct stage03_priv *priv, unsigned int len,
			     __be16 proto, int rc)
{
	unsigned long flags;

	STAGE03_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_inject_count++;
	priv->pending_drained++;
	priv->last_rx_len = len;
	priv->last_rx_proto = ntohs(proto);
	priv->last_inject_rc = rc;
	if (rc == NET_RX_DROP) {
		priv->netif_receive_drop++;
		priv->rx_dropped++;
	} else {
		priv->netif_receive_success++;
		priv->rx_packets++;
		priv->rx_bytes += len;
	}
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/* ==================== 第7部分：skb 构造与 enqueue/drain ==================== */
/*
 * stage03_build_rx_skb - 构造环回 RX skb
 *
 * 【与 stage02 的区别】
 *   → 完全相同，复用 stage02 的设计
 *   → skb_clone() 或 skb_copy()，取决于 loop_mode
 *   → skb_orphan() 断开 socket 关联
 *   → eth_type_trans() 解析以太头
 *
 * 【返回值】
 *   成功 → 新的 rx_skb
 *   失败 → NULL（内存不足）
 */
static struct sk_buff *stage03_build_rx_skb(struct sk_buff *tx_skb,
					    struct stage03_priv *priv,
					    bool *built_by_clone)
{
	struct sk_buff *rx_skb;
	bool clone = stage03_loop_mode_is_clone();

	if (built_by_clone)
		*built_by_clone = clone;

	if (clone)
		rx_skb = skb_clone(tx_skb, GFP_ATOMIC);
	else
		rx_skb = skb_copy(tx_skb, GFP_ATOMIC);
	if (!rx_skb)
		return NULL;

	skb_orphan(rx_skb);
	rx_skb->dev = priv->ndev;
	rx_skb->pkt_type = PACKET_HOST;
	rx_skb->skb_iif = 0;
	rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
	rx_skb->protocol = eth_type_trans(rx_skb, priv->ndev);
	return rx_skb;
}

/*
 * stage03_raise_irq - 教学型中断触发
 *
 * 【设计目的】
 *   真实硬件会在收包时触发 IRQ
 *   这里用软件模拟：记录 irq_raised，设置 irq_masked，触发 napi_schedule
 *
 * 【状态机】
 *   irq_masked = false（初始）
 *       ↓ raise_irq() 调用
 *   irq_masked = true + napi_schedule
 *       ↓ poll 完成
 *   irq_masked = false（通过 napi_complete_done）
 *
 * 【need_schedule 逻辑】
 *   如果 irq_masked 已经是 true，说明已经 schedule 过了
 *   不能再 schedule，避免重复
 */
static void stage03_raise_irq(struct stage03_priv *priv)
{
	unsigned long irq_flags;
	bool need_schedule = false;

	stage03_note_irq_raised(priv);
	spin_lock_irqsave(&priv->state_lock, irq_flags);
	if (!priv->irq_masked) {
		priv->irq_masked = true;
		need_schedule = true;
	}
	spin_unlock_irqrestore(&priv->state_lock, irq_flags);

	if (!need_schedule)
		return;

	stage03_note_irq_masked(priv);
	if (napi_schedule_prep(&priv->napi)) {
		stage03_note_napi_schedule(priv);
		__napi_schedule(&priv->napi);
	}
}

/*
 * stage03_enqueue_pending_rx - skb 入 pending_rxq
 *
 * 【队列满的处理】
 *   max_queue_depth > 0 时检查深度
 *   超过最大深度 → 丢弃 skb（kfree_skb）
 *   记录 pending_dropped++
 *
 * 【为什么入队后要 raise_irq？】
 *   入队只是把 skb 放入队列
 *   需要触发 NAPI poll 才能真正处理这些 skb
 *   raise_irq() 负责 schedule NAPI
 *
 * 【返回值】
 *   0     → 入队成功
 *   -ENOSPC → 队列满，已丢弃
 */
static int stage03_enqueue_pending_rx(struct stage03_priv *priv, struct sk_buff *rx_skb)
{
	unsigned int depth;

	depth = skb_queue_len(&priv->pending_rxq);
	if (max_queue_depth > 0 && depth >= (unsigned int)max_queue_depth) {
		stage03_note_pending_drop(priv, depth);
		kfree_skb(rx_skb);
		return -ENOSPC;
	}

	skb_queue_tail(&priv->pending_rxq, rx_skb);
	depth = skb_queue_len(&priv->pending_rxq);
	stage03_note_enqueue(priv, depth);
	stage03_raise_irq(priv);
	return 0;
}

/*
 * stage03_napi_poll - NAPI poll 函数（核心 ★★★）
 *
 * 【函数签名】
 *   int poll(struct napi_struct *napi, int budget)
 *   → napi：NAPI 描述符
 *   → budget：本次 poll 最多处理的包数
 *   → 返回值：实际处理的包数
 *
 * 【poll 循环语义】
 *   while (work_done < budget) {
 *       skb = dequeue()
 *       if (!skb) break
 *       netif_receive_skb(skb)
 *       work_done++
 *   }
 *
 * 【budget_exhausted 条件】
 *   work_done == budget && queue 还有包
 *   → 本次用完了 budget，但没清空队列
 *   → 下一次 poll 会被继续安排
 *   → napi_budget_exhaust_count++
 *
 * 【napi_complete_done 条件】
 *   !budget_exhausted（即队列已空）
 *   → 清 irq_masked 标志
 *   → 记录 irq_unmasked_count++
 *   → 等待下一次 raise_irq
 *
 * 【netif_receive_skb vs netif_rx】
 *   netif_receive_skb()：
 *     → NAPI 驱动专用，在 poll 上下文调用
 *     → 直接在当前 CPU 处理，不经过 NET_RX_SOFTIRQ
 *
 *   netif_rx()：
 *     → 非 NAPI 驱动使用
 *     → 触发 NET_RX_SOFTIRQ 延迟处理
 *
 *   stage03 在 direct 模式用 netif_rx()，napi 模式用 netif_receive_skb()
 *   这样可以直观对比两种 API 的行为差异
 */
static int stage03_napi_poll(struct napi_struct *napi, int budget)
{
	struct stage03_priv *priv = container_of(napi, struct stage03_priv, napi);
	int work_done = 0;
	bool budget_exhausted;

	while (work_done < budget) {
		struct sk_buff *skb;
		unsigned int len;
		__be16 proto;
		int rc;

		skb = skb_dequeue(&priv->pending_rxq);
		if (!skb)
			break;

		len = skb->len;
		proto = skb->protocol;
		rc = netif_receive_skb(skb);
		stage03_note_napi_inject(priv, len, proto, rc);
		work_done++;
	}

	budget_exhausted = (work_done >= budget) && (skb_queue_len(&priv->pending_rxq) > 0);
	stage03_note_napi_poll(priv, budget, work_done, budget_exhausted);

	if (!budget_exhausted) {
		if (napi_complete_done(napi, work_done)) {
			unsigned long irq_flags;

			spin_lock_irqsave(&priv->state_lock, irq_flags);
			priv->irq_masked = false;
			spin_unlock_irqrestore(&priv->state_lock, irq_flags);
			stage03_note_napi_complete(priv);
			stage03_note_irq_unmasked(priv);
		}
	}

	return work_done;
}

/* ==================== 第8部分：net_device_ops 实现 ==================== */
/*
 * stage03_open / stage03_stop
 *
 * 【open】
 *   napi_enable()：启用 NAPI（必须）
 *   netif_carrier_on()：通知网络子系统载波检测通过
 *   netif_start_queue()：允许 TX 队列开始发送
 *
 * 【stop】
 *   netif_stop_queue()：停止 TX 队列
 *   netif_carrier_off()：清除载波状态
 *   napi_disable()：禁用 NAPI
 *   清空 pending_rxq：防止卸载时 skb 泄漏
 */
static int stage03_open(struct net_device *ndev)
{
	struct stage03_priv *priv = netdev_priv(ndev);

	napi_enable(&priv->napi);
	netif_carrier_on(ndev);
	netif_start_queue(ndev);
	stage03_note_open(priv);
	return 0;
}

static int stage03_stop(struct net_device *ndev)
{
	struct stage03_priv *priv = netdev_priv(ndev);
	struct sk_buff *skb;

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);
	napi_disable(&priv->napi);
	while ((skb = skb_dequeue(&priv->pending_rxq)) != NULL)
		kfree_skb(skb);
	stage03_note_stop(priv);
	return 0;
}

/*
 * stage03_start_xmit - TX 发送函数（入口 ★★★）
 *
 * 【TX → RX 环回的核心路径】
 *
 * direct 模式：
 *   start_xmit → build_rx_skb → netif_rx(rx_skb) → dev_consume_skb_any(skb)
 *
 * napi 模式：
 *   start_xmit → build_rx_skb → enqueue_pending_rx() → dev_consume_skb_any(skb)
 *                                                          ↑
 *                                              注意：TX skb 在入队后立即消费
 *                                              RX skb 在 poll 中才交付协议栈
 *
 * 【返回值 NETDEV_TX_OK】
 *   表示"驱动已消费 skb"（或成功交付硬件）
 *   并不代表对端已收到
 *   真实硬件在 TX 队列满时返回 BUSY，协议栈稍后重传
 */
static netdev_tx_t stage03_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct stage03_priv *priv = netdev_priv(ndev);
	struct sk_buff *rx_skb;
	bool built_by_clone = false;

	if (!skb) {
		stage03_note_tx_drop(priv);
		return NETDEV_TX_OK;
	}

	stage03_note_tx(priv, skb->len, skb->protocol);
	rx_skb = stage03_build_rx_skb(skb, priv, &built_by_clone);
	if (!rx_skb) {
		stage03_note_build_failure(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	stage03_note_build_success(priv, built_by_clone);

	if (stage03_rx_mode_is_napi()) {
		(void)stage03_enqueue_pending_rx(priv, rx_skb);
	} else {
		unsigned int len = rx_skb->len;
		__be16 proto = rx_skb->protocol;
		int rc = netif_rx(rx_skb);
		stage03_note_direct_inject(priv, len, proto, rc);
	}

	dev_consume_skb_any(skb);
	return NETDEV_TX_OK;
}

/*
 * stage03_get_stats64 - 填充 rtnl_link_stats64
 *
 * 【为什么要单独实现？】
 *   netdev 默认的 get_stats64 会读 /proc/net/dev
 *   stage03 用自定义 debugfs 统计，但 ndo_get_stats64 仍然需要实现
 *   这样 ip -s link show 才能显示正确的 TX/RX 计数
 *
 * 【u64_stats 读取模式】
 *   do { start = fetch_begin(); ... = priv->field; } while (fetch_retry());
 *   → 确保读取过程中没有更新，必要时重试
 */
static void stage03_get_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
	struct stage03_priv *priv = netdev_priv(ndev);
	unsigned int start;

	do {
		STAGE03_U64_FETCH_BEGIN(&priv->syncp, start);
		stats->tx_packets = priv->tx_packets;
		stats->tx_bytes = priv->tx_bytes;
		stats->tx_dropped = priv->tx_dropped;
		stats->rx_packets = priv->rx_packets;
		stats->rx_bytes = priv->rx_bytes;
		stats->rx_dropped = priv->rx_dropped;
	} while (STAGE03_U64_FETCH_RETRY(&priv->syncp, start));
}

static const struct net_device_ops stage03_netdev_ops = {
	.ndo_open = stage03_open,
	.ndo_stop = stage03_stop,
	.ndo_start_xmit = stage03_start_xmit,
	.ndo_get_stats64 = stage03_get_stats64,
};

/* ==================== 第9部分：debugfs 统计 ==================== */
/*
 * stage03_stats_show - debugfs 统计输出
 *
 * 【设计要点】
 *   1. 先完整读取所有统计字段到局部变量（受 u64_stats 保护）
 *   2. 再用 seq_printf 输出（不受锁保护）
 *   → 这样输出过程中字段不会因并发更新而错乱
 *
 * 【与 stage02 的区别】
 *   stage02：关注 copy_built / clone_built / netif_rx_* / loop_injected
 *   stage03：关注 napi_schedule_count / napi_poll_count / pending_* / irq_*
 */
static int stage03_stats_show(struct seq_file *m, void *v)
{
	struct stage03_priv *priv = m->private;
	unsigned int start;
	u64 tx_packets, tx_bytes, tx_dropped, last_tx_len, last_tx_proto;
	u64 rx_packets, rx_bytes, rx_dropped, last_rx_len, last_rx_proto;
	u64 open_count, stop_count, copy_built, clone_built, build_failures;
	u64 direct_inject_count, napi_inject_count, netif_rx_success, netif_rx_drop;
	u64 netif_receive_success, netif_receive_drop, last_inject_rc;
	u64 pending_enqueued, pending_drained, pending_dropped, pending_peak, pending_last_depth;
	u64 irq_raised, irq_masked_count, irq_unmasked_count;
	u64 napi_schedule_count, napi_poll_count, napi_complete_count;
	u64 napi_budget_exhaust_count, napi_work_total, last_poll_budget, last_poll_work;

	do {
		STAGE03_U64_FETCH_BEGIN(&priv->syncp, start);
		tx_packets = priv->tx_packets;
		tx_bytes = priv->tx_bytes;
		tx_dropped = priv->tx_dropped;
		last_tx_len = priv->last_tx_len;
		last_tx_proto = priv->last_tx_proto;
		rx_packets = priv->rx_packets;
		rx_bytes = priv->rx_bytes;
		rx_dropped = priv->rx_dropped;
		last_rx_len = priv->last_rx_len;
		last_rx_proto = priv->last_rx_proto;
		open_count = priv->open_count;
		stop_count = priv->stop_count;
		copy_built = priv->copy_built;
		clone_built = priv->clone_built;
		build_failures = priv->build_failures;
		direct_inject_count = priv->direct_inject_count;
		napi_inject_count = priv->napi_inject_count;
		netif_rx_success = priv->netif_rx_success;
		netif_rx_drop = priv->netif_rx_drop;
		netif_receive_success = priv->netif_receive_success;
		netif_receive_drop = priv->netif_receive_drop;
		last_inject_rc = priv->last_inject_rc;
		pending_enqueued = priv->pending_enqueued;
		pending_drained = priv->pending_drained;
		pending_dropped = priv->pending_dropped;
		pending_peak = priv->pending_peak;
		pending_last_depth = priv->pending_last_depth;
		irq_raised = priv->irq_raised;
		irq_masked_count = priv->irq_masked_count;
		irq_unmasked_count = priv->irq_unmasked_count;
		napi_schedule_count = priv->napi_schedule_count;
		napi_poll_count = priv->napi_poll_count;
		napi_complete_count = priv->napi_complete_count;
		napi_budget_exhaust_count = priv->napi_budget_exhaust_count;
		napi_work_total = priv->napi_work_total;
		last_poll_budget = priv->last_poll_budget;
		last_poll_work = priv->last_poll_work;
	} while (STAGE03_U64_FETCH_RETRY(&priv->syncp, start));

	seq_printf(m, "ifname=%s\n", priv->ndev->name);
	seq_printf(m, "rx_mode=%s\n", stage03_rx_mode_is_napi() ? "napi" : "direct");
	seq_printf(m, "loop_mode=%s\n", stage03_loop_mode_is_clone() ? "clone" : "copy");
	seq_printf(m, "napi_weight=%d\n", napi_weight);
	seq_printf(m, "max_queue_depth=%d\n", max_queue_depth);
	seq_printf(m, "irq_masked=%d\n", priv->irq_masked ? 1 : 0);
	seq_printf(m, "pending_queue_len=%u\n", skb_queue_len(&priv->pending_rxq));
	seq_printf(m, "tx_packets=%llu\n", tx_packets);
	seq_printf(m, "tx_bytes=%llu\n", tx_bytes);
	seq_printf(m, "tx_dropped=%llu\n", tx_dropped);
	seq_printf(m, "last_tx_len=%llu\n", last_tx_len);
	seq_printf(m, "last_tx_proto=0x%04llx\n", last_tx_proto);
	seq_printf(m, "rx_packets=%llu\n", rx_packets);
	seq_printf(m, "rx_bytes=%llu\n", rx_bytes);
	seq_printf(m, "rx_dropped=%llu\n", rx_dropped);
	seq_printf(m, "last_rx_len=%llu\n", last_rx_len);
	seq_printf(m, "last_rx_proto=0x%04llx\n", last_rx_proto);
	seq_printf(m, "open_count=%llu\n", open_count);
	seq_printf(m, "stop_count=%llu\n", stop_count);
	seq_printf(m, "copy_built=%llu\n", copy_built);
	seq_printf(m, "clone_built=%llu\n", clone_built);
	seq_printf(m, "build_failures=%llu\n", build_failures);
	seq_printf(m, "direct_inject_count=%llu\n", direct_inject_count);
	seq_printf(m, "napi_inject_count=%llu\n", napi_inject_count);
	seq_printf(m, "netif_rx_success=%llu\n", netif_rx_success);
	seq_printf(m, "netif_rx_drop=%llu\n", netif_rx_drop);
	seq_printf(m, "netif_receive_success=%llu\n", netif_receive_success);
	seq_printf(m, "netif_receive_drop=%llu\n", netif_receive_drop);
	seq_printf(m, "last_inject_rc=%llu\n", last_inject_rc);
	seq_printf(m, "pending_enqueued=%llu\n", pending_enqueued);
	seq_printf(m, "pending_drained=%llu\n", pending_drained);
	seq_printf(m, "pending_dropped=%llu\n", pending_dropped);
	seq_printf(m, "pending_peak=%llu\n", pending_peak);
	seq_printf(m, "pending_last_depth=%llu\n", pending_last_depth);
	seq_printf(m, "irq_raised=%llu\n", irq_raised);
	seq_printf(m, "irq_masked_count=%llu\n", irq_masked_count);
	seq_printf(m, "irq_unmasked_count=%llu\n", irq_unmasked_count);
	seq_printf(m, "napi_schedule_count=%llu\n", napi_schedule_count);
	seq_printf(m, "napi_poll_count=%llu\n", napi_poll_count);
	seq_printf(m, "napi_complete_count=%llu\n", napi_complete_count);
	seq_printf(m, "napi_budget_exhaust_count=%llu\n", napi_budget_exhaust_count);
	seq_printf(m, "napi_work_total=%llu\n", napi_work_total);
	seq_printf(m, "last_poll_budget=%llu\n", last_poll_budget);
	seq_printf(m, "last_poll_work=%llu\n", last_poll_work);
	return 0;
}

static int stage03_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stage03_stats_show, inode->i_private);
}

static const struct file_operations stage03_stats_fops = {
	.owner = THIS_MODULE,
	.open = stage03_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* ==================== 第10部分：net_device 注册/注销 ==================== */
/*
 * stage03_setup - 初始化 net_device
 *
 * ether_setup()：填充以太网的通用字段
 *   → dev->type = ARPHRD_ETHER
 *   → dev->flags |= IFF_NOARP（无需 ARP）
 *   → dev->mtu = ETH_DATA_LEN
 *
 * napi_weight 参数在 netif_napi_add_weight() 时传入
 * netif_napi_add_weight() = netif_napi_add() + 内部设置 weight
 */
static void stage03_setup(struct net_device *ndev)
{
	ether_setup(ndev);
	ndev->netdev_ops = &stage03_netdev_ops;
	ndev->flags |= IFF_NOARP;
	ndev->features |= NETIF_F_HW_CSUM;
	ndev->mtu = ETH_DATA_LEN;
	eth_hw_addr_random(ndev);
}

/*
 * stage03_init / stage03_exit
 *
 * 【初始化顺序】
 *   1. alloc_etherdev_mqs() → 分配 net_device + priv
 *   2. stage03_setup() → 初始化通用字段
 *   3. u64_stats_init() → 初始化 64位统计同步
 *   4. spin_lock_init() → 初始化状态锁
 *   5. skb_queue_head_init() → 初始化 pending_rxq
 *   6. netif_napi_add_weight() → 注册 poll 函数 ★
 *   7. register_netdev() → 注册 net_device
 *   8. debugfs_create_file() → 创建调试接口
 *
 * 【netif_napi_add_weight 参数】
 *   dev    → net_device
 *   napi   → &priv->napi（NAPI 描述符）
 *   poll   → stage03_napi_poll（poll 回调函数）
 *   weight → napi_weight（budget 上限）
 *
 * 【卸载顺序（必须严格对称）】
 *   1. unregister_netdev() → 注销 net_device
 *   2. 清空 pending_rxq（防止 skb 泄漏）
 *   3. debugfs_remove_recursive() → 移除调试接口
 *   4. netif_napi_del() → 移除 NAPI
 *   5. free_netdev() → 释放内存
 */
static int __init stage03_init(void)
{
	struct net_device *ndev;
	struct stage03_priv *priv;
	int ret;

	if (napi_weight <= 0)
		napi_weight = 8;
	if (max_queue_depth <= 0)
		max_queue_depth = 1024;

	ndev = alloc_etherdev_mqs(sizeof(*priv), 1, 1);
	if (!ndev)
		return -ENOMEM;

	stage03_setup(ndev);
	strscpy(ndev->name, ifname, IFNAMSIZ);
	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	u64_stats_init(&priv->syncp);
	spin_lock_init(&priv->state_lock);
	skb_queue_head_init(&priv->pending_rxq);
	netif_napi_add_weight(ndev, &priv->napi, stage03_napi_poll, napi_weight);

	ret = register_netdev(ndev);
	if (ret) {
		netif_napi_del(&priv->napi);
		free_netdev(ndev);
		return ret;
	}

	priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
	if (!IS_ERR_OR_NULL(priv->dbg_dir))
		debugfs_create_file("stats", 0444, priv->dbg_dir, priv, &stage03_stats_fops);

	stage03_dev = ndev;
	pr_info(DRV_NAME ": registered ifname=%s rx_mode=%s loop_mode=%s napi_weight=%d max_queue_depth=%d\n",
		ndev->name, stage03_rx_mode_is_napi() ? "napi" : "direct",
		stage03_loop_mode_is_clone() ? "clone" : "copy",
		napi_weight, max_queue_depth);
	return 0;
}

static void __exit stage03_exit(void)
{
	struct stage03_priv *priv;
	struct sk_buff *skb;

	if (!stage03_dev)
		return;

	priv = netdev_priv(stage03_dev);
	unregister_netdev(stage03_dev);
	while ((skb = skb_dequeue(&priv->pending_rxq)) != NULL)
		kfree_skb(skb);
	debugfs_remove_recursive(priv->dbg_dir);
	netif_napi_del(&priv->napi);
	free_netdev(stage03_dev);
	stage03_dev = NULL;
	pr_info(DRV_NAME ": unloaded\n");
}

module_init(stage03_init);
module_exit(stage03_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("stage03 NAPI poll teaching net_device");
MODULE_LICENSE("GPL");
