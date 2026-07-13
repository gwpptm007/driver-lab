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

- `track-dpdk-advanced/README.md` — DPDK 进阶已收敛
- `track-rdma-core/README.md` — RDMA core Phase 1~8 已完成
- `projects/project-network-acceleration-portfolio/README.md` — 当前作品集入口与真实硬件复验路线
- `projects/project-dpdk-rdma-gateway/README.md` — DPDK + RDMA 综合 capstone，Phase 1-4 当前环境完成

---

## 建议阅读顺序

### 情况 A：你想从头学基础

按 `foundation/day01 -> day35` 顺序推进。

### 情况 B：你想快速看完成度

1. `docs/01_PROGRAMS.md`
2. `docs/02_EXPERT_REVIEW.md`
3. `docs/03_PROGRESS.md`
4. `track-rdma-core/README.md`
5. `track-rdma-core/project-rdma-core-summary/EVIDENCE_INDEX.md`
6. `track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260713_NUMACTL_NODE0.md`
7. `projects/project-network-acceleration-portfolio/README.md`
8. `projects/project-network-acceleration-portfolio/tests/EVIDENCE_INDEX.md`
9. `track-dpdk-advanced/project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md`
10. `projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE1_CONTRACT.md`
11. `projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE2_INGRESS.md`
12. `projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE3_RDMA.md`
13. `projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE4_E2E.md`

### 情况 C：你想开始做代码评审

1. `projects/project-network-acceleration-portfolio/docs/01_PORTFOLIO_MAP.md`
2. `projects/project-network-acceleration-portfolio/tests/EVIDENCE_INDEX.md`
3. `track-rdma-core/project-rdma-core-summary/EVIDENCE_INDEX.md`
4. `track-dpdk-advanced/project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md`

---

## 项目一句话定位

> 这不是"学几个驱动 API"的目录，而是一套从最小驱动骨架、平台/PCIe/DMA、netdev 主线，一直推进到 DPDK、AF_XDP、eBPF、RDMA 和后续 SmartNIC/DPU 的实验型驱动学习项目。
