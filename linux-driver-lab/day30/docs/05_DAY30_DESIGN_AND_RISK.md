# day30 设计取舍与风险说明

## 1. 为什么 day30 先做整页映射

因为当前 DMA buffer 本来就是 4KB 单页，整页映射有三个明显优势：

- 页对齐简单
- guest 用户态使用简单
- 故障定位简单

如果一开始就做部分页映射，会把 day30 的主题稀释成大量 VMA 边界细节。

---

## 2. 为什么非法 `mmap` 路径必须纳入验收

因为 Day30 和 Day29 最大的不同就是：  
**Day30 开始把内核管理的资源直接暴露给用户态。**

只证明“能映射成功”是不够的，还要证明：

- 非法长度会被拒绝
- 非法 offset 会被拒绝

这样 day30 才是一个合格的“用户态共享 DMA buffer”学习包，而不是“碰巧映射成功的一次实验”。

---

## 3. 当前实现选择的代价

### 优点
- 主链路最短
- 现象最好解释
- records 最容易读
- 和 day29 的演进关系很清楚

### 代价
- 不够泛化
- 还没有处理更复杂的同步问题
- 还不适合作为通用框架

这是一个有意为之的取舍。

---

## 4. day30 当前最主要风险

### 风险 1：用户态并没有真的通过 mmap 操作 DMA buffer
应对：
- tool 里只使用映射区进行 pattern write/read
- 文档明确禁止引入额外 staging buffer 作为主路径

### 风险 2：`run_ok=1` 被误判成“已经通过”
应对：
- records 里同时保留 `run-result.txt` 与 `mmap-verify.txt`
- 文档明确两者含义不同

### 风险 3：rootfs 打进旧模块或旧工具
应对：
- run_all 里始终重编工具和模块
- rootfs 每次重建

### 风险 4：guest `/init` 失败后直接 panic
应对：
- busybox applet 链接补齐
- QEMU 使用 `-no-reboot`
- 宿主侧有 timeout 收尾

---

## 5. Day30 完成后你真正学到的东西

- `dma_alloc_coherent()` 分配的 buffer 如何安全地映射给用户态
- 为什么 `dma_mmap_coherent()` 是当前最合适的接口
- 字符设备 `mmap` 的边界检查应该放在哪里
- 用户态直接操作 DMA buffer 后，驱动职责应该怎样收缩
