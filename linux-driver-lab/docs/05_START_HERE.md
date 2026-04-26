# 05_START_HERE

> 快速入门与 GitHub 使用说明

## 先读这几份

1. `README.md` — 项目总览
2. `docs/01_PROGRAMS.md` — 当前阶段与各 track 定位
3. `docs/02_EXPERT_REVIEW.md` — 专家评审结论
4. `docs/03_PROGRESS.md` — 当前进度总览

## 环境依赖

外部依赖（位于项目根目录的 `../kernel-src/`）：

- x86：`linux-5.15.10/build/x86` + `output/x86/bzImage` + `busybox-1.36.1/output/x86/_install`
- arm64：`linux-5.15.10/build/arm64` + `output/arm64/Image` + `busybox-1.36.1/output/arm64/_install`

---

## 快速导航

### 基础学习（foundation/）

| 周 | 目录 | 入口 |
|----|------|------|
| W1 | `foundation/day01/` ~ `day07/` | `foundation/day07/README.md` |
| W2 | `foundation/day08/` ~ `day14/` | `foundation/day14/README.md` |
| W3 | `foundation/day15/` ~ `day21/` | `foundation/day21/FINAL_SUBMISSION.md` |
| W4 | `foundation/day22/` ~ `day28/` | `foundation/day28/README.md` |
| W5 | `foundation/day29/` ~ `day35/` | `foundation/day35/README.md` |

运行任意 day：
```bash
cd foundation/dayXX
chmod +x build.sh
./build.sh
```

### 第二阶段主线（netdev/）

- `netdev/README.md` — 第二阶段总入口
- `netdev/docs/00_START_HERE.md` — netdev 方向入口

### 第三阶段专题研究

- `track-real-driver/lab-virtio-net-source-dive/` — 当前最推荐的下一个 Lab
- `track-virtual-net/README.md` — 虚拟化网络已完成

---

## 建议阅读顺序

### 情况 A：你想从头学基础

按 `foundation/day01 -> day35` 顺序推进。

### 情况 B：你想快速看完成度

1. `docs/01_PROGRAMS.md`
2. `docs/02_EXPERT_REVIEW.md`
3. `docs/03_PROGRESS.md`
4. `foundation/day21/FINAL_SUBMISSION.md`
5. `foundation/day28/README.md`
6. `foundation/day35/README.md`
7. `netdev/README.md`

### 情况 C：你想开始做代码评审

1. `docs/03_PROGRESS.md` 中的"当前开放项"
2. W4/W5 的 records、脚本、输出物

---

## 项目一句话定位

> 这不是"学几个驱动 API"的目录，而是一套从最小驱动骨架、平台/PCIe/DMA、netdev 主线，一直推进到真实驱动源码专题研究的实验型驱动学习项目。