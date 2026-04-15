# STAGE06_MIGRATION_MAPPING

## 文档定位

这份文档用于讲清一个核心问题：

> stage06 到底迁移了什么？哪些东西不变？哪些东西是平台差异？哪些东西已经被抽成了可复用兼容层？

stage06 的价值不在于“又写了一份新驱动”，而在于：

- 保留 stage04 的 netdev 主逻辑
- 把平台差异从主逻辑中剥离
- 用脚本、env、兼容层把迁移方法沉淀下来

---

## 一、保持不变的核心逻辑

stage06 不是另起炉灶，而是以 stage04 为主迁移对象。  
因此下面这些逻辑原则上保持不变：

### 1. netdev 主体结构不变
- `net_device` 生命周期
- `ndo_open` / `ndo_stop`
- `ndo_start_xmit`
- stats 更新逻辑

### 2. NAPI 主逻辑不变
- irq 负责触发 schedule
- poll 按 budget 消费
- 消费完成后决定是否 `napi_complete_done`
- 再决定是否恢复 irq

### 3. ring / descriptor 的教学模型不变
- owner 流转
- posted / done 语义
- TX→RX 闭环的演示逻辑
- refill 的基本思路

### 4. 数据面核心意图不变
- 重点仍是讲清 TX、RX、NAPI、DMA、refill 的关系
- 不是追求极限吞吐或高级 offload

---

## 二、发生变化的平台差异点

这些内容不是驱动主逻辑变化，而是迁移过程中必须处理的平台差异。

### 1. 编译器与工具链
- host：native gcc
- arm64：`aarch64-linux-gnu-*`

### 2. 内核产物形式
- host / x86_64：可能直接用本机或 x86 QEMU 产物
- arm64：通常使用 `Image`

### 3. QEMU 启动参数
- machine
- cpu
- memory
- kernel image
- rootfs
- append 参数

### 4. rootfs 准备方式
- busybox 位置
- `/init` shebang
- `/proc` `/sys` `/dev` 等基础目录
- 需要的模块注入方式

### 5. kernel config 差异
至少已经踩过：
- `CONFIG_NET`
- `CONFIG_PACKET`

---

## 三、由兼容层承担的内容

stage06 的关键不是只有脚本，`include/` 下的兼容层同样重要。

### 1. `include/netdev_kcompat.h`
职责：
- NAPI 兼容包装
- `u64_stats` 兼容包装
- 常用平台/版本判断宏
- 减少驱动主逻辑里到处出现版本差异分支

### 2. `include/netdev_stage_port_profile.h`
职责：
- 迁移阶段相关 profile 的抽象
- 帮助区分平台参数与驱动主逻辑参数

### 3. env + scripts + Makefile glue
职责：
- 把平台差异收敛到统一入口
- 避免把平台判断散在驱动源码和命令行里

---

## 四、由脚本层承担的内容

### `resolve_platform_env.sh`
负责：
- 解析 profile
- 绑定 kernel / rootfs / qemu / toolchain
- 生成稳定的 resolved env

### `check_platform_env.sh`
负责：
- 基础依赖检查
- 发现明显缺失项

### `build_stage04_for_target.sh`
负责：
- 针对目标平台组织构建

### `dryrun_arm64_qemu.sh`
负责：
- 生成 ARM64 QEMU 命令草案

### `smoke.sh`
负责：
- 将环境解析、矩阵、差异报告、阶段报告串起来

---

## 五、什么没有迁移，或者说暂时不在 stage06 范围内

为了避免误判 stage06 的边界，下面这些内容当前并不属于 stage06 的主要完成标准：

- 新开一套独立的 stage06 驱动主代码分支
- 多队列
- RSS / offload / GRO / GSO
- 更真实后端 transport
- 高强度性能 benchmark
- 直接重写成 virtio-net 形态

这些内容更适合放在后续阶段，尤其是 stage07。

---

## 六、可被 future stage 复用的资产

stage06 最有价值的部分之一，就是把下面这些东西沉淀成 future stage 的基座：

### 1. profile 解析方法
后续 stage07 / stage08 都可以直接继承：
- host
- qemu-x86_64
- qemu-arm64

### 2. 平台矩阵生成方法
继续复用：
- `platform_matrix.md`
- 环境解析快照

### 3. 差异报告框架
复用：
- stageX ↔ stageY diff
- 阶段报告生成

### 4. 兼容层头文件
复用：
- 常用内核版本兼容
- NAPI / stats 兼容包装

---

## 七、与 stage07 的边界关系

stage06 结束后，下一阶段应该进入：

> 更接近真实网卡/virtio 的队列模型，而不是继续在 stage06 里加功能。

也就是说：

- stage06 解决 **平台迁移与收口**
- stage07 解决 **更真实的 queue / completion / notify 模型**

这两者是连续关系，但不能混成一个阶段。
