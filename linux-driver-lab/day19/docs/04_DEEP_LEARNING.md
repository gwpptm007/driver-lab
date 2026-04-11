# Day19 量化对比报告深度指南 - W3 阶段收口

## 一、Day19 是什么？

Day19 是 W3（内核裁剪与移植）的倒数第三天，定位是**量化对比报告**。

**核心目标**：把 D15-D18 已经形成的 baseline、trim1、trim2 三个阶段的关键指标，整理成一份可阅读、可复查、可对比的对比报告。

Day19 不做新的实验。它的重点是：
1. **阶段映射**：D15=baseline、D16=trim1、D18=trim2
2. **指标定义**：统一 size/boot/mem/module 的取数和口径
3. **对比表**：三阶段关键指标同表
4. **风险矩阵**：哪些结论稳、哪些必须带 caveat

---

## 二、W3 学习路径中的位置

### 2.1 W3 整体架构

```
W3 (内核裁剪与移植 - day15-21)
├── day15: baseline 冻结
├── day16: 第一轮粗裁
├── day17-18: 分类裁剪
├── day19: 量化对比报告   ← 今天
├── day20: 自动回归套件
└── day21: 最终总结报告

W4 (PCIe 基础 - day22-28)
W5 (DMA + 性能 - day29-35)
```

### 2.2 Day19 与前后天的关系

```
Day18：分类裁剪验证（证明方法成立）
Day19：量化对比报告（把结果压缩成表）
Day20：自动回归套件（让结果可重复验证）
Day21：最终总结报告（1-2 页交付版）

Day18 vs Day19：
  - Day18 关注"配置表达和验证逻辑"
  - Day19 关注"跨阶段量化对比和口径统一"

Day19 vs Day20：
  - Day19 是"报告型"，关注对比表和数据
  - Day20 是"自动化型"，关注脚本和回归流程
```

---

## 三、为什么需要量化对比报告？

### 3.1 三个阶段的意义

```
D15 baseline：
  - 建立基准：Image=38867 KiB，boot=2008ms
  - 目的是"有一个不变的参照物"

D16 trim1 (round1)：
  - 第一轮粗裁：Image=37237 KiB，下降 1630 KiB (4.2%)
  - 目的是"证明粗裁有收益"

D18 trim2 (classified)：
  - 分类裁剪：Image=27417 KiB，perf=yes/yes
  - 目的是"分类表达更清晰，且最终功能完整"
```

### 3.2 为什么不能只给数字？

```
一次性给数字的问题：
  - D18 rootfs=8128 KiB vs D15 rootfs=1181 KiB
  - 看起来 rootfs 变大了 6 倍
  - 但这是因为 D17 加入了 perf 工具
  - 如果不解释清楚，会误以为"裁剪失败了"

报告必须说明：
  - 哪些字段可以直接比
  - 哪些字段必须带 caveat
  - D18 更适合作为"最终形态"而不是"纯量化排名"
```

---

## 四、核心指标体系

### 4.1 指标优先级

```
第一优先级（最直接对应 W3 目标）：
  - image_kib：内核启动镜像体积
  - boot_ms：从 QEMU 启动到 first shell prompt
  - memfree_kib：运行态 MemFree
  - slab_kib：内核 slab 开销

第二优先级（功能保真）：
  - rootfs_kib：initramfs 打包体积
  - modules_loaded_count：运行时加载模块数
  - function_graph_ok：function_graph tracer 是否可用
  - perf_ok：perf 用户态是否可用

第三优先级（辅助解释）：
  - modules_built_count：构建出的 .ko 数量
  - memtotal_kib / memavailable_kib
```

### 4.2 指标详解

#### Image 大小（image_kib）

```bash
# 含义：内核启动镜像体积
# 最直接反映"裁剪是否让内核主体变小"
# D15 baseline = 38867 KiB
# D16 trim1 = 37237 KiB
# 下降 1630 KiB，约 4.2%
```

#### rootfs 大小（rootfs_kib）

```bash
# 含义：initramfs 打包产物体积
# 问题：D17 之后加入了 perf，rootfs 进入新周期
# D15/D16 rootfs = 1181 KiB（无 perf）
# D18 rootfs = 8128 KiB（含 perf）
# 不能直接用这个数字说"rootfs 变大了"
```

#### boot 时间（boot_ms）

```bash
# 含义：QEMU 启动到 first shell prompt 的时间
# 注意：容易受 prompt 判定、rootfs 行为、采样脚本版本影响
# D15 = 2008ms，D16 = 2021ms，变化 +13ms
# 这个量级更适合解读为"基本持平"
```

#### 内存指标（MemFree / Slab）

```bash
# MemFree：运行态空闲内存
# Slab：内核 slab 开销（更贴近"内核自己占了多少"）
# D15 MemFree = 968564 KiB，D16 MemFree = 969716 KiB（轻微改善）
# D15 Slab = 12252 KiB，D16 Slab = 12108 KiB（下降 144 KiB）
```

#### 模块数量

```bash
# modules_loaded_count：运行时加载的模块数
#   - 三阶段都能读到，当前都为 1
#   - 这是"module 数"的主可读字段
#
# modules_built_count：构建出的 .ko 数量
#   - D18 明确为 2
#   - D15/D16 在结果文档里未显式列出
```

---

## 五、三阶段对比表

### 5.1 完整对比表

| 阶段 | Image KiB | ΔImage | rootfs KiB | Δrootfs | boot ms | Δboot | MemFree KiB | ΔMemFree | Slab KiB | ΔSlab | function_graph | perf | 备注 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| D15 baseline | 38867 | 0 | 1181 | 0 | 2008 | 0 | 968564 | 0 | 12252 | 0 | yes | no | 起点基线 |
| D16 trim1 (round1) | 37237 | -1630 | 1181 | 0 | 2021 | +13 | 969716 | +1152 | 12108 | -144 | yes | kernel-side-kept | 第一轮粗裁 |
| D18 trim2 (classified) | 27417 | -11450 | 8128 | +6947 | 2054 | +46 | 961808 | -6756 | 8236 | -4016 | yes | yes/yes | 新周期，含 perf |

### 5.2 哪些可以直接比

```
可以直接相对直接地比较的字段（D15 vs D16）：
  - image_kib：下降 1630 KiB，约 4.2%
  - boot_ms：变化 +13ms，基本持平
  - memfree_kib：轻微改善
  - slab_kib：下降 144 KiB
```

### 5.3 哪些必须带 caveat

```
必须带说明才能比较的字段（D18 vs D15/D16）：
  - rootfs_kib：D17 之后含 perf，新周期
  - boot_ms：随 rootfs 周期变化
  - perf_ok：D15 无，D18 有

D18 在报告中的定位：
  "当前最终形态 + 方法旁证"
  而不是"跨周期纯量化排名"
```

---

## 六、核心结论解读

### 6.1 size：第一轮粗裁已有明确收益

```
D15 Image = 38867 KiB
D16 Image = 37237 KiB
下降 1630 KiB，约 4.2%

结论：第一轮粗裁不是"配置看起来少了"，
      而是已经反映到启动镜像体积上。
```

### 6.2 boot：收益不大，但没有明显回退

```
D15 boot_ms = 2008
D16 boot_ms = 2021
变化 +13ms

解读："基本持平"
当前裁剪带来了镜像缩小，但没有换来启动链路不稳。
```

### 6.3 mem：方向正确

```
D15 MemFree = 968564 KiB
D16 MemFree = 969716 KiB  (+1152)

D15 Slab = 12252 KiB
D16 Slab = 12108 KiB       (-144)

解读：内存侧变化不大，但方向是正向的。
这与"先去掉无关驱动与子系统"的目标一致。
```

### 6.4 D18：结论成立，但要带 caveat

```
D18 可以确认：
  - function_graph = yes
  - perf = yes/yes（用户态 + kernel-side）
  - pass_status = PASS

D18 不能简单拿来和 D15/D16 做"谁更优"的纯量化排名。
因为 rootfs 已进入带 perf 的新周期。

D18 的正确表达方式：
  "当前最终阶段状态成立，且方法表达更清晰；
   但跨周期数字比较必须带说明。"
```

---

## 七、风险矩阵

### 7.1 五大风险

```
1. 口径统一风险
   D15/D16 来自结果文档，D18 来自结构化 records
   → 某些字段在前两阶段缺省

2. rootfs/perf 周期变化风险
   D18 的 rootfs 与 boot 已处于新周期
   → 不能无条件和 D15/D16 做纯收益排名

3. 平台扩展风险
   当前结论只在 arm64 + QEMU virt + 当前 demo 下成立
   → 扩到 PCIe/真板时某些配置可能需要补回

4. 观测能力保留风险
   如果只追求更小镜像，可能先伤到 ftrace/function_graph/perf
   → W3 的目标是"保留调试能力的最小镜像"

5. module 数字段不完整风险
   D15/D16 缺少显式的 modules_built_count
   → 当前只能优先用 modules_loaded_count
```

### 7.2 报告必须表达的风险摘要

```
1. D15 baseline 与 D16 round1 的量化关系最稳
2. D18 classified 可以入表，但 boot/rootfs/perf 必须带 caveat
3. module 数更稳的字段是 modules_loaded_count
4. 以 QEMU virt 成立的结论，不自动等价于其他平台可直接照搬
```

---

## 八、数据来源与取数原则

### 8.1 主要数据来源

```
D15 baseline：
  优先来源：day15/RESULTS.md
  可抽取：Image、rootfs、boot、MemTotal/MemFree/Slab、模块加载状态

D16 trim1：
  优先来源：day16/RESULTS_ROUND1.md、day16/RESULTS_SUMMARY.md
  可抽取：第一轮裁剪后的体积变化、boot 与内存结果

D18 trim2：
  优先来源：day18/records/compare-*.csv、day18/records/*/metrics.env
  可抽取：第二轮分类裁剪后的量化结果、各 profile 对比结论
```

### 8.2 取数原则

```
原则1：优先复用已有沉淀，不重复发明新口径

原则2：如果不同阶段不是完全同口径，要明确写出边界

原则3：区分"已验证的结论"和"需要带边界说明的结论"
```

---

## 九、与 Day18 的关系

### 9.1 继承关系

```
Day18：分类裁剪验证
  - 关注：配置表达方式和验证逻辑
  - 输出：pass/fail 结论，等价性检查结果

Day19：量化对比报告
  - 关注：跨阶段量化对比和口径统一
  - 输出：对比表、风险矩阵、结论解读
```

### 9.2 核心区别

```
Day18：回答"配置表达是否成立"
Day19：回答"量化收益到底有多少"

Day18 的价值：
  - classified 与 round2b_legacy 的 kernel.config sha256 相同
  - 说明分类表达不是"换写法"，而是"结果不变但组织更清晰"

Day19 的价值：
  - 把三个阶段压缩到同一张对比表
  - 明确哪些可以直接比，哪些必须带 caveat
```

---

## 十、与 Day20/Day21 的关系

### 10.1 Day19 → Day20

```
Day19：报告型目录
  - 关注：对比表、量化数据、风险矩阵
  - 输出：day19_compare_report_final.md

Day20：回归型目录
  - 关注：脚本、自动化、pass/fail
  - 输出：回归套件、归档 records

Day19 给 Day20 提供数据基础：
  - 哪些指标是核心监控指标
  - 哪些边界条件需要回归验证
```

### 10.2 Day19 → Day21

```
Day19 是 Day21 的重要数据来源：

Day21 最终报告中的数据：
  - baseline vs trim1 vs trim2 对比
  - 镜像大小 / 启动时间 / 内存占用
  - 来自 Day19 的对比表

Day21 的压缩原则：
  - 优先用 D15 vs D16 的直接对比数字
  - D18 作为"最终形态"带 caveat 表达
```

---

## 十一、面试要会讲的五句话

1. **"Day19 的任务是把 D15 baseline、D16 trim1、D18 trim2 三个阶段的关键指标整理成同一张对比表，并明确哪些结论稳、哪些必须带 caveat"**
   → 理解 Day19 的核心目标

2. **"D15 到 D16 的第一轮粗裁收益最稳：Image 下降 1630 KiB（约 4.2%），boot 基本持平，内存有轻微改善"**
   → 理解核心量化结论

3. **"D18 的 rootfs 变大是因为 D17 之后加入了 perf 工具进入了新周期，不能简单用这个数字说'裁剪失败'"**
   → 理解口径问题

4. **"W3 的目标不是做极限最小镜像，而是做'还能调、还能 trace、还能 perf'的最小实验平台"**
   → 理解观测能力保留风险

5. **"Day19 训练的是信息压缩和结论表达能力：把三个阶段压缩成一张表，区分主线和旁支"**
   → 理解 Day19 的学习价值

---

## 十二、验收标准

### 12.1 报告完整性

- [ ] 有独立 day19/ 目录
- [ ] 有数据来源说明（D15/D16/D18 各从哪里取数）
- [ ] 有完整的三阶段对比表
- [ ] 有核心结论解读（size/boot/mem/module）
- [ ] 有风险矩阵（至少 5 条）
- [ ] 有"哪些可以比、哪些必须带 caveat"的明确说明

### 12.2 报告质量自查

```
1. 目标明确：
   - 开头说清楚 D15/D16/D18 各代表什么
   - 结尾回答量化收益到底有多少

2. 数据量化：
   - 有具体数字（image/boot/mem/slab）
   - 有变化量（ΔImage、Δboot 等）
   - 有百分比（4.2% 等）

3. 结论诚实：
   - 区分已验证结论和带边界结论
   - D18 不拿来和 D15/D16 做纯量化排名

4. 风险明确：
   - 5 条风险都明确表达
   - 不是只藏在表格备注里
```

---

## 附录：指标字段速查表

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day19 指标字段速查                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  image_kib          内核启动镜像体积（最重要）                        │
│  rootfs_kib         initramfs 打包体积（受周期影响）                  │
│  boot_ms            启动到 shell prompt 的时间                        │
│  memfree_kib        运行态 MemFree                                   │
│  slab_kib           内核 slab 开销                                   │
│  memtotal_kib       总内存（辅助）                                   │
│  memavailable_kib   可用内存（辅助）                                  │
│  modules_built_count    构建出的 .ko 数量（D15/D16 缺省）            │
│  modules_loaded_count   运行时加载的模块数（主读数字段）              │
│  function_graph_ok  function_graph tracer 是否可用                    │
│  perf_ok            perf 用户态是否可用                              │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

读表优先级：
  1. image_kib / boot_ms / memfree_kib / slab_kib
  2. rootfs_kib / modules_loaded_count / function_graph_ok / perf_ok
  3. modules_built_count / memtotal_kib / memavailable_kib
```
