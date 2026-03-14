# Day16 - 第一轮与第二轮候选裁剪分析

> 先看 `START_HERE.md`。
>
> 如果你觉得 day16 文件太多，不要从头到尾乱翻，按 `START_HERE.md` 给出的阅读顺序看。

## 1. Day16 的目标

Day16 的目标不是一步裁到最小，而是分轮次完成：

- round1：先去掉当前 baseline 明显无关的驱动大块
- round2 / round2b：继续收残余的显示/I2C/USB 平台项
- 同时保持 Day15 baseline 主链路不被破坏

当前 Day16 已完成：

- 候选项确认
- round1 应用、编译、运行时回归
- round1 结果对比
- round2 候选确认
- round2b 准备分析

详细见：

- `LEARNING_PATH_01.md`
- `ANALYSIS_02_CANDIDATES.md`
- `CONFIG_CONFIRMATION_03.md`
- `RESULTS_ROUND1.md`
- `APPLY_ROUND2_05.md`
- `RESULTS_ROUND2_PREP.md`
- `RESULTS_ROUND2B_PREP.md`

---

## 2. Day16 当前的输入基线

Day16 不是从空白开始，而是继承 Day15 的 baseline：

- `kernel-src/linux-5.15.10/build/arm64/.config`
- `kernel-src/linux-5.15.10/output/arm64/Image`
- `linux-driver-lab/day15/rootfs.img`
- `linux-driver-lab/day15/virt-day15.dtb`
- `linux-driver-lab/day15/RESULTS.md`
- 最近一次成功的 `linux-driver-lab/day15/records/<timestamp>/baseline.csv`

---

## 3. Day16 当前判断

- round1 已通过：裁掉一批网络/声音/部分 USB 项后，baseline 仍成立
- round2 已发现残余显示链与 `I2C_ALGOBIT` 问题
- round2b 已定位到：`DRM` 顶层和 `DRM_SUN4I` 是主要上游来源

下一步：

- 按 `RESULTS_ROUND2B_PREP.md` 的状态，继续进入 round2b 编译验证


## 4. 最终结果与阶段总结

- `RESULTS_ROUND2B.md`：round2b 结果与三轮对比
- `RESULTS_SUMMARY.md`：Day16 完成度总结，以及与原始 D16 需求的对应关系
