# Day16 第一轮粗裁深度指南 - W3 内核裁剪起点

## 一、Day16 是什么？

Day16 是 W3（内核裁剪与移植）的第三天，定位是**第一轮粗裁**。

**核心目标**：在 Day15 baseline 基础上，去掉当前实验路径明显无关的驱动与子系统，验证裁剪后主链路仍然完好。

Day16 不做精细裁剪。它的重点是：
1. **round1**：去掉网络/声音/部分 USB 大块
2. **round2b**：在 round1 基础上继续收显示/I2C 栈
3. **三轮对比**：baseline / round1 / round2b 的量化对比

---

## 二、W3 学习路径中的位置

### 2.1 W3 整体架构

```
W3 (内核裁剪与移植 - day15-21)
├── day15: baseline 冻结
├── day16: 第一轮粗裁     ← 今天
├── day17: perf 集成 + 第二轮裁剪
├── day18: 分类裁剪
├── day19: 量化对比报告
├── day20: 自动回归套件
└── day21: 最终总结报告
```

### 2.2 Day16 与前后天的关系

```
Day15 vs Day16：
  - Day15：建立 baseline（Image=38867 KiB）
  - Day16：在 baseline 基础上做第一轮粗裁

Day16 vs Day17：
  - Day16：只做内核裁剪
  - Day17：引入 perf 工具 + rootfs 工具链

Day16 vs Day18：
  - Day16：粗粒度裁剪（网络/声音/USB/显示）
  - Day18：分类裁剪（required/platform/debug/perf/trim）
```

---

## 三、三轮 profile 设计

### 3.1 为什么需要三轮？

```
baseline：
  - 所有对比的起点
  - 验证"裁剪前"的主链路完好

round1：
  - 在 baseline 基础上裁剪
  - 去掉网络/声音/部分 USB 大块

round2b：
  - 在 round1 基础上继续
  - 收显示栈上层（DRM 相关）
  - 解决 I2C_ALGOBIT 依赖问题
```

### 3.2 三轮对比数据

```
| 指标 | Day15 baseline | Day16 round1 | Day16 round2b |
|---|---:|---:|---:|
| image_kib | 38867 | 37237 | 37105 |
| boot_ms | 2008 | 2021 | 2013 |
| memfree_kib | 968564 | 969716 | 970408 |
| slab_kib | 12252 | 12108 | 12056 |

Image 变化：
  baseline → round1：-1630 KiB (4.2%)
  baseline → round2b：-1762 KiB (4.5%)
```

---

## 四、round1 裁剪详解

### 4.1 round1 裁剪了什么？

```bash
# trim_round1.fragment

# 网络驱动
CONFIG_THUNDER_NIC_PF=n
CONFIG_THUNDER_NIC_BGX=n
CONFIG_HNS3=n
CONFIG_HNS3_ENET=n
CONFIG_E1000=n
CONFIG_E1000E=n
CONFIG_IGB=n
CONFIG_IGBVF=n
CONFIG_SKY2=n

# 声音
CONFIG_SOUND=n
CONFIG_SND=n

# USB/存储/主机控制器
CONFIG_USB_STORAGE=n
CONFIG_USB_EHCI_HCD=n
CONFIG_USB_EHCI_HCD_PLATFORM=n
CONFIG_USB_OHCI_HCD=n
CONFIG_USB_OHCI_HCD_PLATFORM=n
CONFIG_USB_HID=n
```

### 4.2 为什么裁剪这些？

```
为什么裁剪网络驱动？
  - 当前 QEMU virt 实验不挂载真实网卡
  - demo_regmap / tracing / perf 都不依赖网络栈

为什么裁剪声音？
  - 实验不需要音频输出
  - CONFIG_SOUND 是顶层开关，关闭后整棵子树消失

为什么裁剪 USB 相关？
  - guest 内不需要 USB 存储
  - 当前实验路径是 initramfs，不走 USB 启动
```

### 4.3 round1 结果

```bash
# 量化结果
Image: 38867 → 37237 KiB（-1630 KiB, -4.2%）
boot: 2008 → 2021 ms（+13ms，基本持平）
Slab: 12252 → 12108 KiB（-144 KiB）

# 功能回归
debugfs_ok=yes
tracing_ok=yes
function_graph_ok=yes
trace_smoke_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
dmesg_warn=no
```

---

## 五、round2b 裁剪详解

### 5.1 为什么需要 round2b？

```
round2 的发现：
  - round1 后发现 DRM_DW_HDMI 和 I2C_ALGOBIT 仍有上游依赖
  - 直接关闭这些叶子项会导致编译错误

round2b 的解决：
  - 发现 CONFIG_DRM 顶层会拉起 I2C_ALGOBIT
  - DRM_SUN4I 等残余显示链会拉住 DRM_DW_HDMI
  - 解决方案：继续收显示栈上层，让 DRM 链路整体退出
```

### 5.2 round2b 裁剪了什么？

```bash
# trim_round2.fragment

# DRM / 显示平台驱动
CONFIG_DRM_NOUVEAU=n
CONFIG_DRM_EXYNOS=n
CONFIG_DRM_ROCKCHIP=n
CONFIG_DRM_RCAR_DW_HDMI=n
CONFIG_DRM_MSM=n
CONFIG_DRM_LONTIUM_LT9611=n
CONFIG_DRM_LONTIUM_LT9611UXC=n
CONFIG_DRM_SII902X=n
CONFIG_DRM_I2C_ADV7511=n
CONFIG_DRM_DW_HDMI=n
CONFIG_DRM_DW_HDMI_CEC=n
CONFIG_DRM_HISI_HIBMC=n
CONFIG_DRM_MEDIATEK=n
CONFIG_DRM_MESON=n

# soundwire
CONFIG_SOUNDWIRE=n

# I2C 辅助驱动
CONFIG_I2C_ALGOBIT=n
CONFIG_I2C_GPIO=n

# USB chipidea 平台驱动
CONFIG_USB_CHIPIDEA=n
CONFIG_USB_CHIPIDEA_TEGRA=n
```

### 5.3 round2b 结果

```bash
# 量化结果
Image: 37237 → 37105 KiB（-132 KiB）
boot: 2021 → 2013 ms（-8ms）
Slab: 12108 → 12056 KiB（-52 KiB）

# 功能回归
function_graph 仍包含：function_graph wakeup_dl wakeup_rt wakeup irqsoff function nop
其他功能状态同 round1
```

---

## 六、裁剪方法论

### 6.1 裁剪决策流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day16 裁剪决策流程                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  第1步：识别当前实验路径                                             │
│  ──────────────────────────                                          │
│  QEMU virt + initramfs + demo_regmap + tracing/perf                │
│                                                                      │
│  第2步：识别不依赖的子系统                                           │
│  ─────────────────────────────                                      │
│  网络驱动 / 声音 / USB 存储 / 显示栈                                 │
│                                                                      │
│  第3步：生成候选裁剪项                                               │
│  ────────────────────────                                           │
│  基于经验和对 .config 的分析                                         │
│                                                                      │
│  第4步：应用 fragment 并编译验证                                     │
│  ─────────────────────────────────                                  │
│  ./apply_config.sh && make ...                                      │
│                                                                      │
│  第5步：运行时回归验证                                               │
│  ───────────────────────                                            │
│  guest 内验证 debugfs/tracing/insmod 等功能                         │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.2 依赖分析

```
为什么不能直接关闭叶子项？

问题：CONFIG_I2C_ALGOBIT 依赖 CONFIG_I2C
问题：CONFIG_DRM_DW_HDMI 依赖 CONFIG_DRM

解决方案：
  - 分析上游依赖链
  - 如果上游依赖整体不需要，继续向上裁
  - 最终让整棵依赖树退出
```

### 6.3 回归验证

```bash
# 每次裁剪后必须验证
debugfs_ok=yes         # debugfs 可用
tracing_ok=yes        # tracing 可用
function_graph_ok=yes # function_graph tracer 可用
insmod_ok=yes         # demo_regmap.ko 可加载
snapshot_ok=yes       # snapshot 节点可读
trigger_ok=yes        # trigger 节点可写
dmesg_warn=no         # 无 Oops/BUG/Warning
```

---

## 七、与 Day15 baseline 的关系

### 7.1 Day15 baseline 提供了什么

```
Day15 baseline 建立了：
  - 可用的 kernel .config
  - 可启动的 Image
  - 可工作的 demo_regmap.ko
  - 可用的 tracing/perf 环境
  - 标准化的 records 结构

Day16 在此基础上做减法
```

### 7.2 Day15 vs Day16 的核心差异

```
Day15：回答"怎么建立一条可工作的裁剪实验链路"
Day16：回答"在这条链路基础上，哪些可以继续裁掉"

Day15 是"加法"（建立链路）
Day16 是"减法"（去掉多余）
```

---

## 八、与 Day17 的关系

### 8.1 Day17 要解决什么问题

```
Day16 完成时：
  - 内核裁剪路线已建立
  - perf_bin_ok=no（没有 perf 工具）

Day17 要解决：
  - 把 perf 工具集成到 rootfs
  - 建立 rootfs 工具链
```

### 8.2 为什么 Day16 不包含 perf

```
Day16 的焦点是内核裁剪：
  - round1/round2b 都是 kernel .config 的变化
  - perf 工具属于 rootfs 层

perf 集成需要：
  - build_perf.sh 编译 arm64 perf
  - rootfs 内有动态库依赖
  - 这超出了 Day16 "内核裁剪"的范围
```

---

## 九、面试要会讲的五句话

1. **"Day16 的目标是在 Day15 baseline 基础上做第一轮粗裁，去掉网络/声音/USB 等明显无关的驱动大块"**
   → 理解 Day16 的定位

2. **"round1 裁剪后 Image 从 38867 KiB 降到 37237 KiB（约 4.2%），但 boot 时间和功能链基本不变"**
   → 理解 round1 的量化结果

3. **"round2b 在 round1 基础上继续收显示栈上层（DRM 相关），解决 I2C_ALGOBIT 依赖问题，Image 再降 132 KiB"**
   → 理解 round2b 的裁剪逻辑

4. **"裁剪方法论是先识别实验路径，再找不依赖的子系统，最后分析依赖链、向上收割整棵依赖树"**
   → 理解裁剪方法

5. **"Day16 完成了'内核裁剪'，Day17 要在此基础上解决'perf 工具集成'，属于不同的工程问题"**
   → 理解 Day16 和 Day17 的边界

---

## 十、验收标准

### 10.1 裁剪结果验收

- [ ] round1 后 Image 下降至少 1500 KiB
- [ ] round2b 后 Image 继续下降
- [ ] 三轮对比：baseline / round1 / round2b 的 Image 各不相同

### 10.2 功能回归验收

- [ ] debugfs_ok=yes
- [ ] tracing_ok=yes
- [ ] function_graph_ok=yes
- [ ] insmod_ok=yes
- [ ] snapshot_ok=yes
- [ ] trigger_ok=yes
- [ ] dmesg_warn=no

### 10.3 文档验收

- [ ] 有 trim_round1.fragment
- [ ] 有 trim_round2.fragment（或 trim_round2b）
- [ ] 有三轮对比数据
- [ ] 有裁剪决策理由

---

## 附录：round1 和 round2b 裁剪对比

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day16 两轮裁剪对比                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  round1：网络/声音/USB 大块                                         │
│  ────────────────────────────                                       │
│  裁剪目标：与当前实验无关的驱动大块                                  │
│  裁剪方法：经验判断 + 逐项验证                                      │
│  结果：Image -1630 KiB                                              │
│                                                                      │
│  round2b：显示栈上层                                                │
│  ─────────────────────                                              │
│  裁剪目标：解决 round2 发现的 DRM/I2C 依赖                          │
│  裁剪方法：向上追溯依赖链，收割整棵树                                │
│  结果：Image 再 -132 KiB                                             │
│                                                                      │
│  共同点：                                                           │
│    - 不破坏 Day15 baseline 主链路                                   │
│    - 功能回归全部通过                                               │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```
