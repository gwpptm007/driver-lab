# linux-driver-lab

Linux 驱动学习实验主目录。

---

## 目录结构

```
linux-driver-lab/
├── README.md
├── START_HERE_CURRENT.md             当前仓库总入口
├── EXTENSION_ROADMAP.md             扩展学习路线图（后续新建主题）
├── NEXT_PHASE.md                    下一阶段方向说明
├── POST_DAY35_MASTER_REVIEW_AND_ROADMAP.md  ⭐ 评审结论 + 执行路线总文档
├── docs/EXPERT_REVIEW_CURRENT_BASELINE.md   ⭐ 最新专家评审报告
├── docs/ARCHITECTURE_LAYERING_EXPERT.md     ⭐ 架构分层图
├── docs/COMPLETION_MATRIX_EXPERT.md         ⭐ 完成度矩阵
├── docs/NEXT_STEP_EXECUTION_PLAN_EXPERT.md  ⭐ 下一步执行计划
├── docs/                              项目文档
├── foundation/                        ⭐ 第一阶段基础学习区（day01~day35 已收拢）
│   ├── README.md                      foundation 学习路径说明
│   ├── day01/ ~ day07/               W1：字符设备基础闭环
│   ├── day08/ ~ day14/               W2：platform / DT / IRQ / regmap / ftrace
│   ├── day15/ ~ day21/               W3：baseline / 裁剪 / perf / 回归 / 提交收口
│   ├── day22/ ~ day28/               W4：PCIe 基本功作品线
│   └── day29/ ~ day35/               W5：DMA / mmap / bench / perf / ftrace / stability
└── netdev/                            ⭐ 第二阶段主线（stage00~stage06）
    ├── README.md                      netdev 路线入口
    ├── docs/                          总体设计、里程碑、风险、平台策略
    ├── stage00_bootstrap/            启动与环境检查
    ├── stage01_netdev_skeleton/      最小 net_device 骨架
    ├── stage02_skb_path/             skb 收发闭环
    ├── stage03_napi_poll/            NAPI / poll / irq 模型
    ├── stage04_ring_dma/             ring / DMA / replenishment
    ├── stage05_virtio_param/         virtio-net 对照 + 平台参数化
    └── stage06_arm64_migration/      ARM64 迁移与跨平台收口
```

---

## 先看哪里

1. `START_HERE_CURRENT.md`
2. `POST_DAY35_MASTER_REVIEW_AND_ROADMAP.md` — 当前最完整的评审与路线建议
3. `docs/EXPERT_REVIEW_CURRENT_BASELINE.md` — 最新专家评审报告
4. `docs/ARCHITECTURE_LAYERING_EXPERT.md` — 架构分层图
5. `docs/COMPLETION_MATRIX_EXPERT.md` — 当前完成度矩阵
6. `docs/NEXT_STEP_EXECUTION_PLAN_EXPERT.md` — 下一步执行计划
7. `foundation/README.md` — 基础学习区入口
8. `docs/CURRENT_PROJECT_REVIEW.md`
9. `docs/PROGRESS.md`

如果你是第一次接触本仓库，再补：

10. `../kernel-src/README.md`
11. 本文件（`README.md`）

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
- `netdev/stage04_ring_dma/` — 当前最有代表性的代码高点
- `netdev/stage06_arm64_migration/` — ARM64 迁移与平台收口

### 后续扩展（规划中）

- `w06_spi_i2c/` — SPI / I2C 总线驱动
- `w07_gpio/` — GPIO subsystem
- 更多主题后续添加...

---

## 环境依赖

外部依赖（位于项目根目录的 `../kernel-src/`）：

- x86：`linux-5.15.10/build/x86` + `output/x86/bzImage` + `busybox-1.36.1/output/x86/_install`
- arm64：`linux-5.15.10/build/arm64` + `output/arm64/Image` + `busybox-1.36.1/output/arm64/_install`

---

## 建议阅读顺序

### 情况 A：你想从头学基础
按 `foundation/day01 -> day35` 顺序推进。

### 情况 B：你想快速看完成度
1. `START_HERE_CURRENT.md`
2. `POST_DAY35_MASTER_REVIEW_AND_ROADMAP.md`
3. `docs/EXPERT_REVIEW_CURRENT_BASELINE.md`
4. `docs/ARCHITECTURE_LAYERING_EXPERT.md`
5. `docs/COMPLETION_MATRIX_EXPERT.md`
6. `foundation/day21/FINAL_SUBMISSION.md`
7. `foundation/day28/README.md`
8. `foundation/day35/README.md`
9. `netdev/README.md`

### 情况 C：你想开始做代码评审
- `docs/PROGRESS.md` 中的"当前开放项"
- W4/W5 的 records、脚本、输出物

---

## 项目一句话定位

> 这不是"学几个驱动 API"的目录，而是一套从最小驱动骨架，一直走到 PCIe / DMA / perf / 稳定性报告的实验型驱动学习项目。


## 最新补充

- `netdev/stage07_real_queue_model/`：已加入下一阶段的第一版真实代码 + 执行文档，用于承接 stage06 收口之后的准真实队列驱动建模。
