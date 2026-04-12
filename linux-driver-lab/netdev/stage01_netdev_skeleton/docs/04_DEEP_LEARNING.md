# stage01_netdev_skeleton 深度指南 - net_device 生命周期与最小骨架设计

## 一、stage01 在 netdev 路径中的位置

### 1.1 netdev 全景架构

```
netdev/
├── stage00_bootstrap/        → 基础设施 + 环境验证（已完成）
├── stage01_netdev_skeleton/  → 今天：最小 net_device 生命周期骨架
├── stage02_skb_path/         → skb 生命周期与 TX/RX 软件路径
├── stage03_napi_poll/        → NAPI / poll / 中断抑制
├── stage04_ring_dma/         → ring / DMA / RX replenishment
├── stage05_virtio_param/     → virtio-net 对照 + 平台参数化
└── stage06_arm64_migration/  → ARM64 迁移与跨平台收口
```

### 1.2 stage01 与前后阶段的关系

```
stage00 的目标：环境对不对
stage01 的目标：最小骨架能不能跑通

stage01 vs stage02：
  - stage01：建立 northbound 坐标系，ndo_start_xmit 直接消费 skb
  - stage02：引入 skb 生命周期，TX 时做克隆/复制，理解 skb->data/skb_headroom

stage01 的产出是整个 netdev 的"坐标原点"：
  - ndo_open / ndo_stop / ndo_start_xmit 的最小实现
  - debugfs 统计导出（为 stage02~04 扩展留口子）
  - 用户态发包工具（验证"包确实走到了驱动入口"）
```

### 1.3 两条学习路线的汇合点

```
传统路线：先学设备驱动，再学网络驱动
  → 容易陷入"我写的网卡怎么收不到包"的泥潭

stage01 路线：先建立 northbound 视角，再加复杂度
  → 先理解"用户态 → 内核网络栈 → ndo_start_xmit"这条路径
  → 再加 RX、NAPI、ring、DMA

stage01 是这两条路线的汇合点：它不关心硬件，只关心 net_device 生命周期
```

---

## 二、net_device 生命周期深度解析

### 2.1 分配函数的选择

```
alloc_netdev_mqs() vs alloc_etherdev_mqs()：

stage01 用的是 alloc_netdev_mqs：
  alloc_netdev_mqs(sizeof(*priv), ifname, NET_NAME_UNKNOWN, stage01_setup, 1, 1)

然后在 stage01_setup() 里调用 ether_setup(ndev)：
  void stage01_setup(struct net_device *ndev)
  {
      ether_setup(ndev);   // 做以太网的通用初始化
      ndev->netdev_ops = &stage01_netdev_ops;
      ...
  }

为什么不用 alloc_etherdev_mqs() 直接分配？
  → 因为 stage01 需要先调用 stage01_setup，在 setup 里做 ether_setup
  → alloc_etherdev_mqs() 会直接调用 ether_setup，不够灵活
  → 这个设计为后续 stage02~04 在 setup 里加自定义逻辑留了口子
```

### 2.2 register_netdev 的内部过程

```
register_netdev() 不是一个原子操作，它的内部序列是：

1. alloc_netdev() 分配 struct net_device + priv（已在 stage01_init 里完成）
2. dev_get_valid_name() 确保设备名唯一（如 nds0）
3. register_netdevice() → netdev_run_todo() 走网络设备链表
4. netdev_wait_allrefs() 等待所有引用释放（防止竞争）
5. 触发 uevent (UDEV) 用户态通知
6. dev_open() / dev_close() 由用户态 ip 命令触发

关键：register_netdev() 成功返回 ≠ 设备可以发包
      设备要能发包，必须先经过 ndo_open()
```

### 2.3 ndo_open / ndo_stop 的真正职责

```
ndo_open() 的职责：
  1. 初始化硬件相关状态（本阶段为空）
  2. 开启 TX queue：netif_start_queue(ndev)
  3. 设置载波状态：netif_carrier_on(ndev)
     → stage01 故意在这里直接 carrier_on，因为没有真实链路训练

ndo_stop() 的职责：
  1. 停止 TX queue：netif_stop_queue(ndev)
  2. 关闭载波状态：netif_carrier_off(ndev)
  3. 等待所有 pending NAPI poll 完成（如果有的话）

为什么 netif_stop_queue() 重要？
  → 防止在设备不能发包时，内核继续往该队列塞包
  → 避免 skb 积压导致 OOM

为什么 stage01 要 netif_carrier_on()？
  → 没有真实 PHY，carrier 一直是 off
  → 用户态 send() 会返回 ENETDOWN
  → 所以主动标记 carrier on，让发包流程能跑通
```

### 2.4 ndo_start_xmit 的返回值的含义

```
NETDEV_TX_OK = 0：包已成功发送（或消费），TX 路径清空
NETDEV_TX_BUSY = 1：设备忙，调用方应重新入队（通常是 qdisc）

stage01 返回 NETDEV_TX_OK，因为：
  1. 我们把 skb 直接消费掉了（dev_consume_skb_any）
  2. TX 队列永远是空的（queue 不会塞住）

生产网卡返回 NETDEV_TX_BUSY 的场景：
  → DMA 映射失败
  → 硬件 TX ring 满了
  → 需要等待链路训练完成
```

---

## 三、skb 消费策略：为什么直接吃掉是对的

### 3.1 dev_consume_skb_any vs dev_kfree_skb_any

```
这两种都是释放 skb，区别在于：
  dev_consume_skb_any()：优先使用 fast path（直接 free pages）
  dev_kfree_skb_any()：优先使用 slow path（归还到 skb 缓存池）

网络驱动在 TX 路径（中断上下文）里用 dev_consume_skb_any()
用户态触发路径里用 dev_kfree_skb_any()

stage01 在 start_xmit（可以理解为软中断上下文）里用 dev_consume_skb_any()
是正确的选择。
```

### 3.2 stage01 为什么不做真实发包

```
阶段目标决定实现策略：

stage01 的目标：证明"用户态发包确实走到了 ndo_start_xmit"
stage01 不做：真实 DMA、真实 PHY 发送、TX 完成中断

如果 stage01 就做了真实发包：
  → 用户需要配置 TAP 设备、bridge、veth pair
  → 环境复杂度会吞噬学习精力
  → 违背了"先建立坐标系"的原则

教学策略：用"消费 skb + 统计"替代"真实发包"
  → 用户看到 tx_packets 增加了 = 路径打通了
  → 后续 stage02~04 会逐步加上真实路径
```

### 3.3 skb 字段的第一次接触

```
stage01 读取了这些 skb 字段：
  skb->len：包长度（用于统计 tx_bytes）
  skb->protocol：ETHERTYPE（用于统计 last_proto）

stage02 会深入：
  skb->data：数据指针
  skb->headroom / skb_tailroom：线性区和非线性区
  skb_shinfo(skb)->nr_frags：分散/聚集页数
  skb_clone()：克隆 skb（TX 完成前保留原始 skb）
```

---

## 四、u64_stats_sync 并发保护机制

### 4.1 为什么统计需要并发保护

```
问题：ndo_start_xmit 在软中断（BH）上下文执行
     ip link show 读取统计在进程上下文执行
     两者同时发生 → 64位统计读写不是原子的

在没有保护的情况下：
  CPU A 在写：tx_packets = 5
  CPU B 在读：读到 0 或 4 或 5（不稳定）

解决方案：u64_stats_sync
  → 用一对 sync 来标记读写事务
  → 写操作：u64_stats_update_begin / u64_stats_update_end
  → 读操作：u64_stats_fetch_begin / u64_stats_fetch_retry
```

### 4.2 u64_stats_sync 的实现原理

```
典型的 reader/writer 问题，用 seqcount 解决：

struct u64_stats_sync {
    unsigned int syncp;
};

写侧（start_xmit）：
  u64_stats_update_begin_irqsave(&priv->syncp, flags);
  priv->tx_packets++;           // 非原子的 64 位写
  priv->tx_bytes += len;
  u64_stats_update_end_irqrestore(&priv->syncp, flags);
  → 写之前让 seqcount 奇数，写之后变偶数

读侧（ip link show）：
  do {
      start = u64_stats_fetch_begin_irq(&priv->syncp);
      packets = priv->tx_packets;  // 原子读
      bytes = priv->tx_bytes;
  } while (u64_stats_fetch_retry_irq(&priv->syncp, start));
  → 读到的 seqcount 是奇数，说明读到了写中间，重试
  → 读到的 seqcount 是偶数，说明读到了完整的写
```

### 4.3 stage01 的统计项设计

```
stage01_priv 里的统计项分两类：

基本统计（内核标准，会出现在 ip -s link 里）：
  struct rtnl_link_stats64 {
      u64 tx_packets;
      u64 tx_bytes;
      u64 tx_dropped;
  }

私有扩展（只有 debugfs 有）：
  open_count    → 接口被打开的次数
  stop_count    → 接口被关闭的次数
  last_len      → 最后一个包的长度
  last_proto    → 最后一个包的 ETHERTYPE

为什么分开？
  → 基本统计是内核标准接口，ip -s link 会读
  → 私有扩展是教学用途，方便调试和理解
```

---

## 五、驱动代码分析

### 5.1 私有数据结构

```c
// 驱动的私有数据，挂载在 net_device->priv 上
struct stage01_priv {
    struct net_device *ndev;     // 回指到父设备，debugfs show 里用
    struct dentry *dbg_dir;       // debugfs 目录项，卸载时需要 remove_recursive
    struct u64_stats_sync syncp;  // 并发保护，统计读写的事务标记

    /* 基本统计——内核标准字段，ip -s link 会读到 */
    u64 tx_packets;              // 总发包数
    u64 tx_bytes;                // 总发字节数
    u64 tx_dropped;              // 丢弃计数

    /* 私有扩展——只有 debugfs 有，ip -s link 看不到 */
    u64 open_count;              // ndo_open 被调用次数
    u64 stop_count;              // ndo_stop 被调用次数
    u64 last_proto;              // 最后一个包的 ETHERTYPE（主机序）
    u64 last_len;                // 最后一个包的 length
};
```

```
为什么要 priv->ndev 回指？
  → debugfs show() 函数拿到的是 seq_file->private = priv
  → priv->ndev 用来读取设备名 priv->ndev->name

为什么要 dbg_dir 单独保存？
  → 卸载时需要 debugfs_remove_recursive(dbg_dir)
  → 不能依赖 debugfs_create_dir() 的自动清理

为什么要 syncp？
  → ndo_start_xmit 在软中断上下文写统计
  → ip link show 在进程上下文读统计
  → 两者并发，64 位变量需要 seqcount 保护
```

### 5.2 设备分配与初始化

```c
// 模块入口：alloc_netdev_mqs → register_netdev → debugfs
static int __init stage01_init(void)
{
    struct stage01_priv *priv;
    int ret;

    // 分配 net_device + priv，TX/RX 队列各 1 个
    stage01_dev = alloc_netdev_mqs(
        sizeof(*priv),            // priv 大小
        ifname,                   // 接口名（默认 "nds0"，可模块参数覆盖）
        NET_NAME_UNKNOWN,         // 命名空间类型
        stage01_setup,            // 初始化回调（ether_setup + ops 填充）
        1,                        // txqs
        1                         // rxqs
    );
    if (!stage01_dev)
        return -ENOMEM;

    priv = netdev_priv(stage01_dev);   // 从 net_device 拿到 priv 指针
    memset(priv, 0, sizeof(*priv));    // 清零（alloc_netdev 不保证清零）
    priv->ndev = stage01_dev;          // priv 回指 ndev
    u64_stats_init(&priv->syncp);      // 初始化 seqcount

    ret = register_netdev(stage01_dev); // 注册到内核网络栈
    if (ret) {
        pr_err("register_netdev failed: %d\n", ret);
        free_netdev(stage01_dev);
        stage01_dev = NULL;
        return ret;
    }

    stage01_debugfs_init(priv);         // 创建 debugfs 目录和 stats 文件
    return 0;
}
```

```
为什么用 alloc_netdev_mqs 而不是 alloc_etherdev_mqs？
  → alloc_etherdev_mqs 内部直接调用 ether_setup，不够灵活
  → stage01 需要先调用自定义的 stage01_setup()
  → stage01_setup 里做 ether_setup + 填充 netdev_ops

为什么 priv 要 memset 清零？
  → alloc_netdev() 只保证 net_device 内存被清零
  → priv 区域可能残留旧数据，需要 memset 0

为什么先 register_netdev 再 debugfs_create_file？
  → register_netdev 创建设备节点 /sys/class/net/nds0/
  → debugfs 放在 /sys/kernel/debug/netdev_stage01/（不依赖设备节点）
  → 顺序无所谓，但通常先注册设备
```

### 5.3 stage01_setup 的职责

```c
// net_device 的初始化回调，在 alloc_netdev_mqs 内部被调用
static void stage01_setup(struct net_device *ndev)
{
    ether_setup(ndev);                      // 以太网通用初始化
    ndev->netdev_ops = &stage01_netdev_ops; // 填充驱动操作函数
    ndev->watchdog_timeo = msecs_to_jiffies(5000); // TX 超时 5 秒
    ndev->min_mtu = 68;
    ndev->max_mtu = ETH_DATA_LEN;           // 1500
    ndev->flags |= IFF_NOARP;               // 本设备不做 ARP
    ndev->features |= NETIF_F_SG;           // 开启分散/聚集
    eth_hw_addr_random(ndev);               // 随机生成 MAC 地址
}
```

```
ether_setup() 做了什么？
  → 设置 dev->addr_len = ETH_ALEN (6)
  → 设置 dev->type = ARPHRD_ETHER (1)
  → 设置 dev->tx_queue_len = DEFAULT_TX_QUEUE_LEN
  → 设置 broadcast 地址为全 ff
  → 初始化 dev->dev_addr 为零

IFF_NOARP 的含义？
  → 告诉内核"这个设备不需要 ARP"
  → 用户态发往本设备的包不会触发 ARP 解析
  → virtio-net 默认也有这个标志

NETIF_F_SG 的含义？
  → 开启分散/聚集（Scatter-Gather）
  → 允许一个 skb 使用多个内存页（skb_shinfo->frags）
  → stage01 虽然不深入，但它开启了这个 feature
```

### 5.4 ndo_open / ndo_stop 的实现

```c
static int stage01_open(struct net_device *ndev)
{
    struct stage01_priv *priv = netdev_priv(ndev);
    unsigned long flags;

    // 更新 open 计数
    u64_stats_update_begin_irqsave(&priv->syncp, flags);
    priv->open_count++;
    u64_stats_update_end_irqrestore(&priv->syncp, flags);

    /*
     * stage01 没有真实链路训练过程。
     * 为了让用户态能更顺滑地做最小 bring-up，
     * 我们直接标记 carrier on。
     */
    netif_carrier_on(ndev);     // 标记载波在线（无真实 PHY）
    netif_start_queue(ndev);    // 开启 TX 队列（允许内核发包）

    netdev_info(ndev, "stage01 open: queue started, carrier on\n");
    return 0;
}

static int stage01_stop(struct net_device *ndev)
{
    struct stage01_priv *priv = netdev_priv(ndev);
    unsigned long flags;

    netif_stop_queue(ndev);     // 停止 TX 队列（禁止内核再发包）
    netif_carrier_off(ndev);    // 标记载波离线

    // 更新 stop 计数
    u64_stats_update_begin_irqsave(&priv->syncp, flags);
    priv->stop_count++;
    u64_stats_update_end_irqrestore(&priv->syncp, flags);

    netdev_info(ndev, "stage01 stop: queue stopped, carrier off\n");
    return 0;
}
```

```
为什么 open 里要 netif_carrier_on()？
  → 没有真实 PHY，载波永远是 off
  → 用户态 sendto() 会返回 ENETDOWN
  → 主动标记 carrier on，让发包流程能跑通
  → 这是 stage01 的教学策略，不是生产网卡的写法

netif_start_queue() vs netif_stop_queue()？
  → netif_start_queue()：告诉内核"TX 队列可用"，可以继续发包
  → netif_stop_queue()：告诉内核"TX 队列满/不可用"，暂停发包
  → 如果 stop 后内核继续发包，内核会等待（不丢包）
  → 真实网卡在 DMA 忙时调用 stop_queue()

netif_carrier_off/on vs netif_stop/start_queue 的区别？
  → carrier：物理链路状态（有/无载波）
  → queue：TX 队列是否接受新包
  → 两者独立，但通常一起操作
```

### 5.5 ndo_start_xmit 的实现

```c
static netdev_tx_t stage01_start_xmit(struct sk_buff *skb,
                                      struct net_device *ndev)
{
    struct stage01_priv *priv = netdev_priv(ndev);

    // 防御性检查：skb 不应该为 NULL
    if (unlikely(!skb)) {
        stage01_update_drop_stats(priv);
        return NETDEV_TX_OK;
    }

    /*
     * 这里故意不做真实发包。
     * stage01 的目标是先证明：
     * 用户态发到该接口的数据，确实走到了 ndo_start_xmit。
     */
    stage01_update_tx_stats(priv, skb->len, skb->protocol);

    netdev_dbg(ndev, "stage01 xmit: len=%u proto=0x%04x\n",
               skb->len, ntohs(skb->protocol));

    dev_consume_skb_any(skb);   // 消费 skb（教学策略）
    return NETDEV_TX_OK;
}
```

```
dev_consume_skb_any() vs dev_kfree_skb_any() 的区别？
  → dev_consume_skb_any()：优先 fast path（直接 free pages）
  → dev_kfree_skb_any()：优先 slow path（归还 skb 缓存池）
  → TX 路径（软中断上下文）用 _any 更快

dev_consume_skb_any() vs kfree_skb()？
  → dev_consume_skb_any() 是对 skb 的正确销毁
  → 直接 kfree() 会绕过 skb 缓存池管理，是错误的

为什么返回 NETDEV_TX_OK 而不是 NETDEV_TX_BUSY？
  → NETDEV_TX_OK：发送成功，TX 路径清空
  → NETDEV_TX_BUSY：设备忙，调用方应重新入队
  → stage01 的 queue 永远不会满，所以返回 OK

ntohs(skb->protocol) 的作用？
  → skb->protocol 是网络字节序（大端）
  → ntohs() 转换为主机字节序
  → printf %llx 显示时用主机序更直观
```

### 5.6 ndo_get_stats64 的实现

```c
static void stage01_get_stats64(struct net_device *ndev,
                struct rtnl_link_stats64 *stats)
{
    struct stage01_priv *priv = netdev_priv(ndev);
    unsigned int start;
    u64 tx_packets, tx_bytes, tx_dropped;

    // 读侧用 u64_stats_fetch_begin/retry 处理并发
    do {
        start = u64_stats_fetch_begin_irq(&priv->syncp);
        tx_packets = priv->tx_packets;
        tx_bytes   = priv->tx_bytes;
        tx_dropped = priv->tx_dropped;
    } while (u64_stats_fetch_retry_irq(&priv->syncp, start));

    stats->tx_packets = tx_packets;
    stats->tx_bytes   = tx_bytes;
    stats->tx_dropped = tx_dropped;
}
```

```
为什么用 do-while 而不是直接赋值？
  → 如果在读取过程中 ndo_start_xmit 写入了统计
  → u64_stats_fetch_retry 会检测到 seqcount 变化
  → 整个 do-while 循环重新读一遍，直到读到一致的快照

为什么要返回 stats 而不是直接填充全局变量？
  → ndo_get_stats64 是内核网络栈调用的查询接口
  → 每次调用都实时读取 priv 中的值
  → 不需要维护额外的"上次快照"

struct rtnl_link_stats64 是谁定义的？
  → include/linux/rtnl.h
  → 包含所有标准统计字段（tx_packets/rx_packets/tx_bytes/rx_bytes...）
  → ip -s link show 就是读取这个结构
```

### 5.7 模块卸载路径

```c
static void __exit stage01_exit(void)
{
    struct stage01_priv *priv;

    if (!stage01_dev)
        return;

    priv = netdev_priv(stage01_dev);
    stage01_debugfs_exit(priv);        // 先清理 debugfs
    unregister_netdev(stage01_dev);    // 再注销设备
    free_netdev(stage01_dev);          // 最后释放内存
    stage01_dev = NULL;
}
```

```
为什么顺序是 debugfs → unregister_netdev → free_netdev？
  → debugfs_remove_recursive()：清理 debugfs 目录
  → unregister_netdev()：从网络栈注销设备（等所有引用消失才返回）
  → free_netdev()：释放 net_device 和 priv 内存

unregister_netdev() 会阻塞等待引用消失：
  → 正在调用 ndo_start_xmit 的软中断需要先返回
  → 持有 net_device 引用的其他模块需要先释放
  → 这保证了卸载时的安全性

为什么 stage01_exit 里要先判断 stage01_dev？
  → 防止重复卸载（rmmod 两次）
  → 第二次卸载时 stage01_dev 已经是 NULL
```

---

## 六、debugfs 导出设计

### 6.1 为什么用 debugfs 而非 sysfs

```
sysfs vs debugfs：

sysfs：
  → 有严格属性规范（show/store 函数指针）
  → 用于导出标准化设备属性
  → 路径：/sys/class/net/eth0/...

debugfs：
  → 没有格式限制，直接暴露 seq_file
  → 用于教学调试、导出复杂内部状态
  → 路径：/sys/kernel/debug/netdev_stage01/stats

stage01 用 debugfs 的原因：
  → 教学阶段需要导出非标准统计（open_count, last_proto 等）
  → 不需要遵守 sysfs 的设备模型规范
  → seq_file 接口比 sysfs 的 show/store 更灵活
```

### 6.2 seq_file 的工作原理

```
seq_file 是内核提供的一种迭代器接口，用于导出变长或复杂结构：

struct seq_file {
    struct file *file;
    void *private;        // 传给 show() 的数据指针
    // ...
};

stage01_stats_show() 的流程：
  1. single_open(file, stage01_stats_show, inode->i_private)
     → 把 priv（struct stage01_priv*）存到 seq_file->private
  2. seq_file->private 就是 stage01_stats_show 的 m->private
  3. show() 函数读取所有统计项，用 seq_printf() 输出

为什么 debugfs 用 seq_file 而不是直接 read()？
  → read() 无法处理大数据集（需要循环）
  → seq_file 支持 lseek()、支持大数据多页
  → 内核调试接口的标准选择
```

---

## 七、用户态发包工具分析

### 7.1 原始套接字（SOCK_RAW）的工作原理

```
send_stage01_frame.c 用的是：

socket(AF_PACKET, SOCK_RAW, htons(DEFAULT_ETHERTYPE))

AF_PACKET 的层次：
  Level 2（链路层）直接发包，不需要 TCP/UDP 协议栈
  ETHERTYPE 0x88B5：IEEE 静默帧（不会被标准协议处理）

socket 创建后的流程：
  1. 用 SIOCGIFINDEX ioctl 获取 ifindex
  2. 填充 sockaddr_ll：sll_ifindex = ifindex
  3. sendto() 直接把 ETH_FRAME 送到网络栈
  4. 内核网络栈根据 ETHERTYPE 找到注册的协议处理函数
  5. 最终走到 ndo_start_xmit()

关键点：AF_PACKET/SOCK_RAW 跳过了路由决策，
       直接把帧送到指定接口的 TX 队列
```

### 7.2 帧格式

```
以太网帧结构：
  [0-5]   dst MAC:   ff ff ff ff ff ff（广播）
  [6-11]  src MAC:   12 12 12 12 12 12（合成）
  [12-13] EtherType: 88 B5（自定义静默协议）
  [14+]   payload:  用户数据

为什么 EtherType 用 0x88B5？
  → IEEE 保留的实验协议，不被标准栈处理
  → 不会触发 ARP、IPv4、IPv6 等标准解析
  → 包会直接被提交到 ndo_start_xmit
```

---

## 八、smoke 测试的验证逻辑

### 8.1 smoke.sh 的三段式验证

```
第一段：环境检查
  - 用户态工具存在且可执行
  - 模块已加载（lsmod | grep netdev_stage01）

第二段：接口状态
  - ip link set up（触发 ndo_open）
  - ip link show（验证 state UP、carrier UNKNOWN）

第三段：功能验证
  - sendto() 发送原始帧（触发 ndo_start_xmit）
  - ip -s link show（对比 TX 前后的 packets/bytes）
  - debugfs 读统计（验证 open_count/last_proto）
```

### 8.2 缺少的验证项

```
当前 smoke.sh 缺少的：
  1. 没有 assert tx_packets 增量 == 1
  2. 没有验证 carrier 状态变化
  3. 没有 stress 测试（多次 sendto）
  4. 没有卸载后 stats 清零的验证

这些会在后续阶段补全，当前 stage01 聚焦于"能跑通"
```

---

## 九、面试要会讲的五句话

1. **"stage01 的核心目标是建立 northbound 视角的坐标系：通过 alloc_netdev_mqs + register_netdev 把 net_device 注册到内核网络栈，通过 ndo_open/stop/start_xmit 维护最小生命周期，用 dev_consume_skb_any() 证明用户态发包确实走到了驱动入口"**
   → 理解 stage01 的教学定位和"故意不完整"的设计选择

2. **"ndo_start_xmit 直接消费 skb 而不是做真实发送，是 stage01 的教学策略：先打通'用户态→驱动入口'这条路径，建立统计和 debugfs 可观测性，再在 stage02~04 逐步加上 skb 生命周期、TX 完成、NAPI 等复杂度"**
   → 理解"先坐标系，后复杂度"的学习路径

3. **"u64_stats_sync 用 seqcount 解决 64 位统计在软中断和进程上下文并发读写的问题：写侧用 u64_stats_update_begin/end 标记奇偶，读侧用 u64_stats_fetch_begin/retry 保证读到的是完整的 64 位值"**
   → 理解内核网络驱动统计并发保护的标准做法

4. **"debugfs 用于导出私有扩展统计（open_count、last_proto、last_len），因为这些不是内核标准字段，无法通过 ip -s link 读取；seq_file 接口比 sysfs 更适合教学调试场景"**
   → 理解 debugfs vs sysfs 的选择依据和 seq_file 原理

5. **"AF_PACKET/SOCK_RAW 原始套接字工作在链路层，sendto() 直接把 ETH_FRAME 送到指定接口的 TX 队列，跳过路由决策；EtherType 0x88B5 是 IEEE 静默协议，不会触发标准协议解析，适合教学验证"**
   → 理解用户态触发驱动的完整路径

---

## 十、验收标准

### 10.1 环境验收

- [ ] `make report` 执行成功，输出 stage01_report.md
- [ ] `make build-userspace` 成功生成 `tools/send_stage01_frame`
- [ ] 内核头文件可用时 `make build-module` 成功
- [ ] `make smoke` 能跑通

### 10.2 驱动验收

- [ ] `insmod netdev_stage01.ko` 成功，无 Oops
- [ ] `ip link show` 能看到 `nds0` 设备
- [ ] `ip link set nds0 up` 触发 ndo_open，dmesg 有 "stage01 open" 日志
- [ ] `ip link set nds0 down` 触发 ndo_stop，dmesg 有 "stage01 stop" 日志

### 10.3 功能验收

- [ ] `./send_stage01_frame nds0 test` 发送成功
- [ ] `ip -s link show nds0` 的 tx_packets 增加
- [ ] `cat /sys/kernel/debug/netdev_stage01/stats` 显示正确统计

### 10.4 扩展验收（stage02 时回看）

- [ ] stage02 能复用 stage01 的 debugfs 扩展机制
- [ ] stage02 加入 skb 克隆/复制时，统计逻辑不受影响
- [ ] 平台可配置变量（TARGET_ARCH / QEMU_BIN）全程有效

---

## 附录 A：目录结构

```
stage01_netdev_skeleton/
├── driver/
│   ├── Makefile
│   └── netdev_stage01.c      # 核心内核模块
├── tools/
│   ├── Makefile
│   ├── README.md
│   ├── send_stage01_frame.c # 用户态原始套接字发包工具
│   └── send_stage01_frame   # 编译产物
├── scripts/
│   ├── check_host_env.sh
│   ├── generate_stage01_report.sh
│   ├── load_module.sh
│   ├── unload_module.sh
│   ├── read_debugfs_stats.sh
│   └── smoke.sh             # 冒烟测试
├── docs/
│   ├── 01_STAGE_GOAL_AND_BOUNDARY.md
│   ├── 02_DRIVER_DESIGN.md
│   ├── 03_TEST_AND_ACCEPTANCE.md
│   ├── PLAN.md
│   └── 04_DEEP_LEARNING.md  # 本文件：深度学习指南
├── env/
│   └── stage01_netdev_skeleton.env
├── output/
│   ├── host_env_stage01.env
│   └── stage01_report.md
├── records/                  # smoke 测试记录
├── workdir/                  # 临时工作目录
├── Makefile
├── README.md
├── START_HERE.md
└── DIRECTORY_TREE.md
```

## 附录 C：简化流程图

```
用户态进程
     │
     ▼
socket(AF_PACKET, SOCK_RAW)
     │
     ▼
sendto(ifindex, frame)
     │
     ▼
内核网络栈（协议查找）
     │
     ▼
ndo_start_xmit(skb, ndev)      ← netdev_stage01.c:115
     │
     ├─→ stage01_update_tx_stats()
     │
     └─→ dev_consume_skb_any(skb)
              │
              ▼
         返回 NETDEV_TX_OK
```

## 附录 D：完整调用链图

### D.1 用户态发包 → ndo_start_xmit 全链路

```
用户态（send_stage01_frame）               内核网络栈                         netdev_stage01.c
================================         ================================     ==================

socket(AF_PACKET, SOCK_RAW, 0x88B5)
  │                                         
  │  socket fd                              
  ▼                                         
sendto(fd, frame, frame_len, 0,            
       (struct sockaddr *)&addr,           
       sizeof(addr))                       
  │                                         
  │  ──────────────────────────────────────► sys_sendto()
  │                                              │
  │                                              ▼
  │                                         sock_sendmsg()
  │                                              │
  │                                              ▼
  │                                         sock->ops->sendmsg()
  │                                              │
  │                                              ▼
  │                                         inet_sendmsg()
  │                                              │
  │                                              ▼
  │                                         udp_sendmsg()  （ETHERTYPE 查表）
  │                                              │
  │                                              ▼
  │                                         __dev_queue_xmit()    ← 关键入口
  │                                              │
  │                                              ▼
  │                                         validate_xmit_skb()
  │                                              │
  │                                              ▼
  │                                         netdev_start_xmit()
  │                                              │
  │                                              ▼
  │                                         ops->ndo_start_xmit(skb, dev)
  │                                              │
  │                                              ▼
  │                                         stage01_start_xmit(skb, ndev)  ──────► [驱动层]
  │                                              │
  │                                         ▲
  │                                         │
dev_consume_skb_any(skb)  ← skb 在此被消费（stage01 教学策略）
  │
  ▼
返回 NETDEV_TX_OK
```

### D.2 ip link up → ndo_open 全链路

```
用户态                              内核 netdev 层                    netdev_stage01.c
=========                           =============================     ==================

ip link set nds0 up
  │
  │  ──────────────────────────────────────────────► rtnetlink_rcv()
  │                                                    │
  │                                                    ▼
  │                                               do_setlink()
  │                                                    │
  │                                                    ▼
  │                                               dev_change_flags()
  │                                                    │
  │                                                    ▼
  │                                               dev_open()
  │                                                    │
  │                                                    ▼
  │                                          遍历 ops 找到 ndo_open
  │                                                    │
  │                                                    ▼
  │                                          ndev->netdev_ops->ndo_open(ndev)
  │                                                    │
  │                                                    ▼
  │                                          stage01_open(ndev)  ─────────────────► [驱动层]
```

### D.3 ip link down → ndo_stop 全链路

```
用户态                              内核 netdev 层                    netdev_stage01.c
=========                           =============================     ==================

ip link set nds0 down
  │
  │  ──────────────────────────────────────────────► rtnetlink_rcv()
  │                                                    │
  │                                                    ▼
  │                                               do_setlink()
  │                                                    │
  │                                                    ▼
  │                                               dev_change_flags()
  │                                                    │
  │                                                    ▼
  │                                               dev_close()
  │                                                    │
  │                                                    ▼
  │                                          遍历 ops 找到 ndo_stop
  │                                                    │
  │                                                    ▼
  │                                          ndev->netdev_ops->ndo_stop(ndev)
  │                                                    │
  │                                                    ▼
  │                                          stage01_stop(ndev)  ──────────────────► [驱动层]
```

### D.4 cat /sys/kernel/debug/.../stats → debugfs 全链路

```
用户态                              内核 VFS 层                       debugfs / seq_file
=========                           =============================     =================

cat /sys/kernel/debug/netdev_stage01/stats
  │
  │  ───────────────────────────────────────────────► sys_open()
  │                                                    │
  │                                                    ▼
  │                                               do_dentry_open()
  │                                                    │
  │                                                    ▼
  │                                               debugfs_open()
  │                                                    │
  │                                                    ▼
  │                                          debugfs_create_file() 注册的
  │                                          struct file_operations
  │                                                    │
  │                                                    ▼
  │                                               stage01_stats_open()
  │                                               = single_open()
  │                                                    │
  │  ◄────────────────────────────────────────────── 返回 struct seq_file*
  │
  ▼
seq_file->show() 循环调用：
  │
  │  ───────────────────────────────────────────────► stage01_stats_show()
  │                                                    │
  │                                                    ▼
  │                                          读取 priv->tx_packets
  │                                          u64_stats_fetch_begin()
  │                                                    │
  │                                          seq_printf(m, "tx_packets=%llu\n")
  │                                                    │
  │  ◄────────────────────────────────────────────── 返回格式化输出
  │
read() 返回 /sys/kernel/debug/netdev_stage01/stats 内容
```

### D.5 模块加载/卸载链路

```
用户态                              内核模块层                       netdev_stage01.c
=========                           =============================     ==================

insmod netdev_stage01.ko
  │
  │  ───────────────────────────────────────────────► sys_init_module()
  │                                                    │
  │                                                    ▼
  │                                          copy_module_from_user()
  │                                                    │
  │                                                    ▼
  │                                          module_sig_check()
  │                                                    │
  │                                                    ▼
  │                                          do_init_module()
  │                                                    │
  │                                                    ▼
  │                                          do_initcalls()
  │                                                    │
  │                                                    ▼
  │                                          stage01_init()  ──────────────────────► [驱动层]
  │                                          = module_init()
  │                                                    │
  │                                                    ▼
  │                                          alloc_netdev_mqs()
  │                                                    │
  │                                                    ▼
  │                                          register_netdev()
  │                                                    │
  │                                                    ▼
  │                                          debugfs_create_dir()
  │                                                    │
  │                                                    ▼
  │                                          debugfs_create_file()
  │
rmmod netdev_stage01
  │
  │  ───────────────────────────────────────────────► sys_delete_module()
  │                                                    │
  │                                                    ▼
  │                                          try_stop_module()
  │                                                    │
  │                                                    ▼
  │                                          free_module()
  │                                                    │
  │                                                    ▼
  │                                          stage01_exit()  ───────────────────────► [驱动层]
  │                                          = module_exit()
  │                                                    │
  │                                                    ▼
  │                                          debugfs_remove_recursive()
  │                                                    │
  │                                                    ▼
  │                                          unregister_netdev()
  │                                                    │
  │                                                    ▼
  │                                          free_netdev()
```

## 附录 E：stage01 vs stage02~04

| 特性 | stage01 | stage02 | stage03 | stage04 |
|------|---------|---------|---------|---------|
| 核心焦点 | net_device 生命周期 | skb 生命周期 | NAPI/poll | ring/DMA |
| TX 路径 | 直接消费 skb | skb_clone + 真实发送 | NAPI 聚合发送 | DMA 映射 |
| RX 路径 | 无 | 简单接受 | NAPI poll | RX replenishment |
| 统计 | u64_stats_sync | 加 skb 字段 | 加中断计数 | 加 DMA 映射统计 |
| debugfs | 基础统计 | skb 详情 | poll 详情 | ring 详情 |

| 特性 | stage01 | stage02 | stage03 | stage04 |
|------|---------|---------|---------|---------|
| 核心焦点 | net_device 生命周期 | skb 生命周期 | NAPI/poll | ring/DMA |
| TX 路径 | 直接消费 skb | skb_clone + 真实发送 | NAPI 聚合发送 | DMA 映射 |
| RX 路径 | 无 | 简单接受 | NAPI poll | RX replenishment |
| 统计 | u64_stats_sync | 加 skb 字段 | 加中断计数 | 加 DMA 映射统计 |
| debugfs | 基础统计 | skb 详情 | poll 详情 | ring 详情 |
