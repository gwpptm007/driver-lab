# Day19 指标定义

## 1. 指标定义的目的

Day19 的任务不是简单贴一个表，而是要让后面读报告的人知道：

- 每个字段代表什么
- 它是从哪里取的
- 它是否能直接跨阶段比较

所以这一页要先把指标口径固定下来。

---

## 2. 核心字段

### 2.1 `Image` 大小

- 字段：`image_bytes` / `image_kib`
- 含义：内核启动镜像体积
- 作用：最直观地观察裁剪是否让内核主体变小
- 说明：这是 Day19 最重要的量化字段之一

### 2.2 `rootfs.img` 大小

- 字段：`rootfs_bytes` / `rootfs_kib`
- 含义：initramfs 打包产物体积
- 作用：观察 rootfs 工具集变化对整体实验环境的影响
- 说明：D17 之后加入 perf，会显著影响该值的可比性

### 2.3 `boot_ms`

- 字段：`boot_ms`
- 含义：从 QEMU 进程启动到 first shell prompt 的时间
- 作用：观察裁剪后启动路径是否有明显收益或回退
- 说明：该值很容易受 prompt 判定、rootfs 行为、采样脚本版本影响

### 2.4 `MemTotal` / `MemFree` / `MemAvailable`

- 字段：`memtotal_kib` / `memfree_kib` / `memavailable_kib`
- 含义：guest 运行态内存视角
- 作用：观察运行态开销是否下降
- 说明：Day19 草稿版重点解读 `MemFree`，`MemAvailable` 作为辅助

### 2.5 `Slab`

- 字段：`slab_kib`
- 含义：内核 slab 开销
- 作用：辅助判断内核态内存占用是否下降
- 说明：相对 `MemFree` 更贴近“内核自己占了多少”这个问题

### 2.6 模块数量

Day19 当前区分两个概念：

#### A. `modules_built_count`

- 含义：宿主机侧构建出来的 `.ko` 数量
- 作用：观察构建产物范围
- 问题：D15 / D16 的结果文档里没有把它显式落表

#### B. `modules_loaded_count`

- 含义：guest 实际运行时加载的模块数量
- 作用：观察当前验证链路是否仍然按预期加载 demo 模块
- 当前建议：在 Day19 草稿里，优先把它当作“module 数”的主可读字段

### 2.7 `function_graph_ok`

- 含义：`function_graph` tracer 是否仍可用
- 作用：确认裁剪没有破坏 Day13 / W3 的关键观测能力

### 2.8 `perf_ok`

- 含义：`perf` 用户态与基础 smoke 是否可用
- 作用：确认 D17 之后的 perf 路线是否成立
- 说明：D15 里它是 `no`，D18 已进入 `yes/yes`

---

## 3. Day19 报告中的解读优先级

Day19 报告建议按下面的优先级读表：

### 第一优先级

- `image_kib`
- `boot_ms`
- `memfree_kib`
- `slab_kib`

这四个字段最直接对应 D19 的“size / boot / mem”。

### 第二优先级

- `rootfs_kib`
- `modules_loaded_count`
- `function_graph_ok`
- `perf_ok`

这些字段帮助判断：

- 当前环境是不是已经换周期
- 功能有没有掉
- 可观测性有没有掉

### 第三优先级

- `modules_built_count`
- `memtotal_kib`
- `memavailable_kib`

这些字段对解释有帮助，但不是当前草稿版最核心的结论字段。

---

## 4. 哪些字段可以直接比较

### 4.1 直接比较较稳的字段

在 D15 baseline 与 D16 round1 之间，以下字段可以相对直接地读：

- `image_kib`
- `boot_ms`
- `memfree_kib`
- `slab_kib`

### 4.2 需要带 caveat 的字段

在 D18 进入新周期后，以下字段必须带说明来读：

- `rootfs_kib`
- `boot_ms`
- `perf_ok`

### 4.3 当前草稿版不强行给结论的字段

- `modules_built_count`（D15 / D16 文档未显式记录）
- D15/D16 vs D18 的“绝对 boot 排名”

---

## 5. 这版指标口径的结论

Day19 草稿版的指标策略可以概括成一句话：

> **核心收益看 Image / boot / mem / slab；功能保真看 module / function_graph / perf；跨周期字段一律带 caveat。**
