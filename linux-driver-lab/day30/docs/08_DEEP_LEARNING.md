# Day30 深度学习指南 - mmap 零拷贝访问

## 一、Day30 是什么？

Day30 是 W5 的第二天，承接 Day29 的 coherent DMA 基础，引入 **mmap 零拷贝** 访问。

**核心目标**：把 coherent DMA buffer 通过字符设备 `mmap()` 映射到用户态，让用户态直接读写 buffer，内核退居"DMA 发起者 + 边界守门员"。

---

## 二、Day29 → Day30 的核心变化

### 2.1 主角切换

| 角色 | Day29 | Day30 |
|------|-------|-------|
| 谁填充 src | 内核 `fill_pattern()` | 用户态 `fill_pattern()` |
| 谁清空 dst | 内核 `memset()` | 用户态 `memset()` |
| 谁发起 DMA | 内核 `day29_do_verify()` | 用户态 `ioctl(RUN_DMA)` |
| 谁比较结果 | 内核逐字节比较 | 用户态逐字节比较 |
| 主角 | **内核** | **用户态** |

### 2.2 新增系统调用

```
mmap(NULL, map_bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)
    ↓
dma_mmap_coherent(&pdev->dev, vma, dma_virt, dma_handle, dma_bytes)
    ↓
用户态直接访问 dma_virt 对应的物理内存（零拷贝）
```

### 2.3 为什么要做 mmap 零拷贝？

**没有 mmap 时**，用户态想访问 DMA buffer 必须：
```
用户态 → 内核缓冲区 → DMA buffer（两次 copy）
```

**有了 mmap 后**：
```
用户态 ←→ DMA buffer（零次 copy，直接访问）
```

---

## 三、mmap 零拷贝原理

### 3.1 什么是 mmap？

`mmap()` 是一个系统调用，把一个文件或设备的内存区域映射到用户进程的虚拟地址空间。

```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

映射后，用户态访问 `addr` 就等于直接访问底层物理内存。

### 3.2 dma_mmap_coherent() 的作用

```c
ret = dma_mmap_coherent(&pdev->dev, vma,
                        d->dma_virt, d->dma_handle, d->dma_bytes);
```

这是内核提供的专门用于 coherent DMA buffer 的 mmap API：
- `dma_virt`：CPU 访问用的虚拟地址（`dma_alloc_coherent` 返回的）
- `dma_handle`：设备 DMA 地址
- 底层建立页表，使用户态能直接访问 coherent buffer

### 3.3 为什么不能用普通 mmap？

| 类型 | 适用场景 | 能否用于 DMA buffer |
|------|----------|-------------------|
| 普通 `mmap(file)` | 文件映射 | 否（文件 backed） |
| `dma_mmap_coherent()` | coherent DMA buffer | **是**（专为 DMA 设计） |

普通 `mmap` 映射的是文件-backed 的虚拟内存，而 `dma_mmap_coherent` 映射的是专门为 DMA 分配的 coherent 内存，确保 cache 一致性。

---

## 四、Day30 与 Day29 的结构体对比

### 4.1 day30_dev 新增字段

```c
struct day30_dev {
    /* === 与 day29 相同 === */
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    unsigned int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;

    void *dma_virt;
    dma_addr_t dma_handle;
    size_t dma_bytes;
    u32 dma_mask_bits;

    u32 last_run_len;
    u32 last_run_seed;
    s32 last_run_error;
    u32 last_run_ok;
    u32 last_irq_delta;
    u32 last_dma_cmd;

    /* === Day30 新增：mmap 结果记录 === */
    u32 last_mmap_ok;        /* 最近一次 mmap 是否成功 */
    s32 last_mmap_error;     /* mmap 错误码 */
    u32 last_mmap_len;       /* mmap 请求的长度 */
    u32 last_mmap_pgoff;     /* mmap 请求的页偏移 */

    struct mutex op_lock;

    dev_t devt;
    struct cdev cdev;
    struct device *device;
};
```

**为什么需要 mmap 结果字段？**
- 驱动需要记录 mmap 的边界校验结果
- 用户态需要知道 mmap 是成功还是失败、失败原因
- 这些信息通过 `GET_INFO` 和 `GET_RESULT` 返回给用户态

---

## 五、mmap 边界校验

### 5.1 为什么需要严格校验？

Day30 只支持"整页映射 + offset=0"：
- 简化主链路，把注意力集中在零拷贝访问本身
- 非法长度/offset 必须变成可验证的失败路径

### 5.2 校验规则

```c
/* 只允许 offset == 0 */
if (vma->vm_pgoff != 0) {
    day30_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
    return -EINVAL;
}

/* 只允许长度 == PAGE_ALIGN(dma_bytes) */
if (len != map_bytes) {
    day30_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
    return -EINVAL;
}
```

### 5.3 页对齐的陷阱

```
mmap() 的 length 会在建立 VMA 时按页向上取整
例如：在 4KB 页环境下，请求 2048 字节会被扩成 4096

如果驱动允许 len == 4096，那么 2048 反而会"变成合法"
这就是为什么 invalid-length 测试要用 4097 这类跨页长度
```

---

## 六、mmap-verify 完整流程

### 6.1 用户态触发

```bash
day30_edu_mmap_tool /dev/day30_edu0 mmap-verify 256 0x41
```

### 6.2 完整调用链

```
用户态 mmap-verify:
    │
    ├─ 1. ioctl(GET_INFO)  获取 map_bytes/src_off/dst_off
    │
    ├─ 2. mmap(NULL, map_bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)
    │       ↓
    │     day30_mmap()
    │       ├→ 边界校验（pgoff==0, len==map_bytes）
    │       ├→ dma_mmap_coherent() 建立映射
    │       └→ 返回用户态
    │
    ├─ 3. 用户态直接写 src
    │     src = map + src_off
    │     fill_pattern(src, 256, 0x41)
    │
    ├─ 4. 用户态清空 dst
    │     dst = map + dst_off
    │     memset(dst, 0, 256)
    │
    ├─ 5. ioctl(RUN_DMA) 触发两段 DMA
    │       ↓
    │     day30_ioctl(RUN_DMA)
    │       ↓
    │     day30_do_run_dma(len=256, seed=0x41)
    │       ├→ program_dma(src_dma, 0x40000, 256, START|IRQ)
    │       ├→ program_dma(0x40000, dst_dma, 256, START|DIR|IRQ)
    │       └→ 等 DMA 完成
    │
    ├─ 6. 用户态直接读 dst 并比较
    │     for i in 0..255:
    │         if src[i] != dst[i]:
    │             记录 mismatch
    │
    └─ 7. munmap() 解除映射
```

### 6.3 数据流对比

**Day29（内核主导）**：
```
用户态                    内核                    EDU
  │                        │                       │
  │  ioctl(VERIFY)         │                       │
  │───────────────────────→│                       │
  │                        │  fill_pattern(src)    │
  │                        │  memset(dst)           │
  │                        │                       │
  │                        │  DMA: RAM→EDU         │
  │                        │──────────────────────→│
  │                        │                       │
  │                        │  DMA: EDU→RAM         │
  │                        │──────────────────────→│
  │                        │                       │
  │                        │  memcmp(src, dst)      │
  │                        │                       │
  │  return result         │                       │
  │←───────────────────────│                       │
```

**Day30（用户态主导）**：
```
用户态                    内核                    EDU
  │                        │                       │
  │  mmap()                │                       │
  │───────────────────────→│                       │
  │  return mapped addr    │                       │
  │←───────────────────────│                       │
  │                        │                       │
  │  fill_pattern(src)     │                       │（直接在用户态）
  │  memset(dst)           │                       │
  │                        │                       │
  │  ioctl(RUN_DMA)         │                       │
  │───────────────────────→│                       │
  │                        │  DMA: RAM→EDU         │
  │                        │──────────────────────→│
  │                        │                       │
  │                        │  DMA: EDU→RAM         │
  │                        │──────────────────────→│
  │                        │                       │
  │  memcmp(src, dst)      │                       │（直接在用户态）
  │                        │                       │
  │  munmap()              │                       │
```

---

## 七、mmap 页对齐陷阱详解

### 7.1 问题描述

mmap 的 length 参数在 4KB 页环境下会被向上取整到页边界。

### 7.2 示例

| 请求 length | 系统实际分配 | 驱动检查 len |
|-------------|-------------|-------------|
| 2048 | 4096 (1页) | `4096 != 4096` → **通过** |
| 4096 | 4096 (1页) | `4096 == 4096` → **通过** |
| 4097 | 8192 (2页) | `8192 != 4096` → **拒绝** |
| 0 | 0 (失败) | 根本到不了驱动 |

### 7.3 为什么 invalid-length 测试用 4097？

因为要确保长度**跨页**且**不等于 map_bytes**，这样驱动才能稳定拒绝。

---

## 八、mmap 状态追踪

### 8.1 为什么需要 last_mmap_* 字段？

```
用户态 mmap() 失败可能有多种原因：
- pgoff != 0          → -EINVAL
- len != map_bytes    → -EINVAL
- dma_mmap_coherent 内部失败
```

驱动需要记录并返回给用户态：
- mmap 是否成功
- 失败时的错误码
- 请求的长度
- 请求的页偏移

### 8.2 状态返回路径

1. `day30_mmap()` 调用 `day30_record_mmap_result()`
2. `day30_record_mmap_result()` 写入 `last_mmap_*` 字段
3. 用户态通过 `GET_INFO` 或 `GET_RESULT` 获取

---

## 九、与 Day31 的关系

| 特性 | Day30 | Day31 |
|------|-------|-------|
| mmap 接口 | 整页映射 | 可能支持分段映射 |
| buffer 管理 | 单一 4KB buffer | 多个 buffer 或更大 buffer |
| 同步模型 | mutex 保护 | 可能的复杂同步 |
| 学习重点 | mmap 零拷贝基础 | 更真实的共享场景 |

---

## 十、验收标准

### 10.1 必须满足

- `mmap()` 成功返回用户态可访问的地址
- 用户态能直接读写 mmap 返回的地址（零拷贝）
- `mmap-verify` 返回 `verify_ok=1`
- `mmap-invalid-offset` 稳定返回错误
- `mmap-invalid-length` 稳定返回错误（用 4097 测试）

### 10.2 关键证据

```
mmap-verify.txt:
  verify_ok=1           ← mmap 验证成功
  mismatch_index=-1    ← src == dst
  run_ok=1             ← DMA 成功
  irq_delta=2          ← 两次 DMA 各触发一次 IRQ
  mmap_ok=1            ← mmap 成功
```

---

## 十一、面试要会讲的五句话

1. **"Day30 把零拷贝的主角从内核切到用户态"**
   → Day29 是内核填充/比较，Day30 是用户态通过 mmap 直接填充/比较，内核退居 DMA 编程者

2. **"mmap 的 length 会被页对齐，这是个容易踩的坑"**
   → 在 4KB 页下请求 2048 字节实际会分配 4096，导致 invalid-length 测试必须用 4097

3. **"dma_mmap_coherent() 和普通 mmap 的区别"**
   → 普通 mmap 映射文件，dma_mmap_coherent 映射专为 DMA 设计的 coherent buffer，保证 cache 一致性

4. **"mmap 零拷贝的价值在于减少一次内存 copy"**
   → 没有 mmap 时用户态访问 DMA buffer 需要 copy 两次，有了 mmap 就可以零次 copy 直接访问

5. **"Day30 的 mmap 边界检查是故意做严格的"**
   → 只允许 offset=0 和 length=map_bytes，把 VMA 切片的问题留给 Day31 以后处理
