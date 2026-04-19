# 04_DRIVER_LAYOUT — 驱动主要结构与初始化流程

## struct stage09_priv — 驱动私有数据

挂在 `netdev->priv` 上，管理所有队列和全局资源：

```c
struct stage09_priv {
    struct net_device *ndev;
    spinlock_t state_lock;             /* 全局状态锁，保护所有队列状态 */
    struct workqueue_struct *backend_wq; /* 统一 backend workqueue */
    struct dentry *dbg_dir;            /* debugfs 目录句柄 */
    u32 num_queues;                    /* 实际启用的队列数 */
    u32 ring_size;                     /* 每个 ring 的 slot 数 */
    u32 napi_weight;                   /* NAPI poll 的 budget */
    u32 rx_buf_size;                   /* RX buffer 大小（字节） */
    u32 backend_delay_us;              /* backend 处理延迟（微秒） */
    u32 backend_batch;                 /* backend 每次最多处理的帧数 */
    atomic64_t rr_counter;             /* round-robin 分发计数器 */
    atomic64_t open_count;            /* 设备 up 次数 */
    atomic64_t stop_count;             /* 设备 down 次数 */
    struct stage09_queue queues[STAGE09_MAX_QUEUES]; /* 队列数组 */
};
```

**state_lock 的必要性**：
- `ndo_start_xmit` / `backend_workfn` / `napi_poll` 三个上下文会并发访问
- 每队列的 `tx_inflight/rx_ready` 等都需要锁保护

**backend_wq 配置**：
- `WQ_UNBOUND`：work 不绑定特定 CPU，调度器决定执行位置
- `WQ_MEM_RECLAIM`：当内存紧张时，workqueue 会参与内存回收

---

## module 参数（可运行时配置）

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ifname` | string | `"nds9"` | 设备名称 |
| `num_queues` | uint | 2 | 启用队列数量，最大 4 |
| `ring_size` | uint | 128 | 每个 TX/RX ring 的 slot 数 |
| `napi_weight` | uint | 64 | NAPI poll 的 budget |
| `rx_buf_size` | uint | 2048 | RX buffer 大小（字节） |
| `backend_delay_us` | uint | 0 | backend 处理每批帧的延迟（微秒），默认无延迟 |
| `backend_batch` | uint | 64 | backend 每次最多处理的帧数 |

使用示例：
```bash
insmod netdev_stage09.ko num_queues=4 backend_delay_us=100
```

`backend_delay_us` 设为 >0 可模拟真实硬件的异步处理延迟，设为 100 可观测到明显的 doorbell→backend 异步阶梯。

---

## stage09_init — 模块初始化（6 步）

```c
static int __init stage09_init(void)
```

**完整初始化流程**：

1. **参数校验 + clamp**
   ```c
   num_queues = clamp_t(unsigned int, num_queues, 1, STAGE09_MAX_QUEUES);
   ring_size = max_t(unsigned int, ring_size, 32);
   ```

2. **alloc_etherdev_mqs**：分配 net_device 和 priv，设置队列数
   ```c
   ndev = alloc_etherdev_mqs(sizeof(struct stage09_priv), num_queues, num_queues);
   ```
   一步到位，自动设置 ETH_HLEN/addr_len/type，设定 TX/RX 队列数。

3. **初始化 priv**：设置参数、初始化 spinlock
   ```c
   priv = netdev_priv(ndev);
   spin_lock_init(&priv->state_lock);
   ```

4. **创建 backend_workqueue**：`WQ_UNBOUND | WQ_MEM_RECLAIM`

5. **初始化每个队列**：分配 ring、注册 NAPI、reset 状态
   ```c
   for (i = 0; i < priv->num_queues; ++i) {
       stage09_alloc_ring(&q->txq, ring_size);
       stage09_alloc_ring(&q->rxq, ring_size);
       STAGE09_NETIF_NAPI_ADD(ndev, &q->napi, stage09_napi_poll, napi_weight);
       stage09_reset_queue(q);
   }
   ```

6. **register_netdev**：向内核注册设备

**err 标号错误处理**：严格按分配顺序反向释放（ring → workqueue → netdev）。

---

## stage09_exit — 模块卸载（4 步逆向）

```c
static void __exit stage09_exit(void)
```

**完整卸载流程**（反向顺序）：

1. `stage09_debugfs_deinit`：删除 debugfs 目录
2. `unregister_netdev`：从内核移除 netdev
3. `flush_workqueue` + `destroy_workqueue`：等待所有 backend work 执行完毕
4. 每个队列：`cancel_work_sync` + `netif_napi_del` + `free_ring`
5. `free_netdev`：释放 net_device 和 priv 内存

**cancel_work_sync vs flush_workqueue**：
- `cancel_work_sync`：取消 work 并等待其执行完毕
- `flush_workqueue`：等待队列中所有 work 执行完毕（不取消）

---

## 网络设备操作表

```c
static const struct net_device_ops stage09_netdev_ops = {
    .ndo_open           = stage09_open,
    .ndo_stop           = stage09_stop,
    .ndo_start_xmit     = stage09_start_xmit,
    .ndo_select_queue   = stage09_select_queue,
    .ndo_get_stats64    = stage09_get_stats64,
};
```

**与 stage08 对比**：stage08 没有 `ndo_select_queue`（单队列不需要）。

---

## stage09_open — 设备 UP

等价于 `ifconfig nds9 up`：

1. 重置所有队列状态（`stage09_reset_queue`）
2. 填充所有 RX buffer（`stage09_refill_rx_all`）
3. 使能所有 NAPI（`napi_enable`）
4. 启动所有 TX 队列（`netif_tx_start_all_queues`）

---

## stage09_stop — 设备 DOWN

等价于 `ifconfig nds9 down`：

1. 停止所有 TX 队列（`netif_tx_disable`）
2. 刷新 backend workqueue（`flush_workqueue`）
3. 禁止所有 NAPI（`napi_disable`）

**flush_workqueue 的必要性**：rmmod 前必须确保没有 work 还在运行，否则可能导致 use-after-free。

---

## 构建（两阶段）

### 标准构建

```bash
./scripts/build.sh
```

**第一阶段：Driver 编译**
```bash
make -C "$KDIR" M="$(pwd)"/driver modules
```
- `-C "$KDIR"`：进入内核源码目录执行 Makefile
- `M="$(pwd)"`：指定模块源码目录（内核 Makefile 的外部模块机制）
- `modules`：目标为编译 .ko 模块文件

**第二阶段：Userspace tools 编译**
```bash
make -C "$KDIR" M="$(pwd)"/tools modules
```
编译 `send_stage09_frame` 和 `recv_stage09_frame` 两个测试工具。

**KDIR 说明**：默认 `../kernel-src/linux-5.15.10/build/x86`，可通过环境变量覆盖。

### 内核 6.8+ 兼容

```c
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE09_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
    netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
#else
#define STAGE09_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
    netif_napi_add((ndev), (napi), (pollfn), (weight))
#endif
```

---

## 运行

### 启动 QEMU 并加载

```bash
./scripts/run.sh reload
```

等价手动步骤：
```bash
insmod driver/netdev_stage09.ko ifname=nds9 num_queues=2
ifconfig nds9 up
```

### smoke test（快速验证）

```bash
./scripts/smoke.sh
```

**Before/After 差分验证**：发送前后各记录一次 `debugfs/stats`，快照相减得到本次增量。
**并发收发**：`recv &` 先启动后台监听，`sleep 0.5` 等待就绪，发送，`wait` 等待结束。

---

## debugfs 观测接口

挂载点：`/sys/kernel/debug/netdev_stage09/`

### stats — 设备统计

```bash
sudo cat /sys/kernel/debug/netdev_stage09/stats
```
每行 `key=value`，便于脚本解析。`test_tx/test_rx` 是测试帧专用计数器。

### queues — ring index 和 slot 状态

```bash
sudo cat /sys/kernel/debug/netdev_stage09/queues
```
每队列显示 6 个 TX index、6 个 RX index、3 个状态标志、以及前 8 个 slot 状态。

### timeline — per-queue 时间戳链

```bash
sudo cat /sys/kernel/debug/netdev_stage09/timeline
```
4 个 delta：`submit_to_doorbell` / `doorbell_to_backend` / `backend_to_irq` / `irq_to_poll`。
`doorbell_to_backend_ns > 0` 是异步的核心证明。

---

## 验证脚本

| 脚本 | 验证内容 | 通过标准 |
|------|----------|----------|
| `queue_dist_check.sh` | 多队列分布 | >= 2 个队列有 `tx_submit > 0` |
| `timeline_check.sh` | 异步链路 | >= 1 个队列 `doorbell_to_backend > 0` |
| `trace_smoke.sh` | dmesg 收集 | 输出包含 `stage09` 日志 |
