# project-dpdk-v17-legacy-review

> 把过去 DPDK v17 媒体面经验，与当前 `track-dpdk` 现代 DPDK 实验链路做一次系统对照、迁移复盘和面试表达整理。

## 当前状态

```text
READY_TO_REVIEW
```

说明：`project-dpdk-media-gateway-lite` 当前先收到了 `PASS_SMOKE`，真实 UDP / forwarding / rewrite 闭环后面再补。本项目先继续推进“经验复盘与作品化表达”，不阻塞主线。

## 为什么要做这一站

前面的 lab 证明了现代 DPDK 环境、vhost/virtio-user、L2 forwarding、fastpath、media-gateway-lite 的实现能力。

这一站要回答面试里更关键的问题：

```text
你以前做过 DPDK v17 项目，那它和现在这套 modern DPDK track 怎么对应？
旧项目里的 UDP 媒体面、KNI、UIO、ARP/IP/UDP 改写，如何迁移成现在的实现？
你到底掌握的是 demo，还是数据面系统设计能力？
```

## 输出内容

```text
project-dpdk-v17-legacy-review/
├── README.md
├── START_HERE.md
├── docs/
│   ├── 01_GOAL_AND_SCOPE.md
│   ├── 02_V17_PROJECT_RECONSTRUCTION.md
│   ├── 03_V17_TO_MODERN_MAPPING.md
│   ├── 04_KNI_UIO_VFIO_VHOST_REVIEW.md
│   ├── 05_MEDIA_GATEWAY_MIGRATION.md
│   ├── 06_MIGRATION_CHECKLIST.md
│   ├── 07_INTERVIEW_EXPLANATION.md
│   └── 08_RESUME_MATERIAL.md
├── scripts/
│   ├── 00_make_review_bundle.sh
│   └── 01_generate_portfolio_summary.sh
├── records/
│   └── templates/
└── reports/
    ├── DPDK_V17_LEGACY_REVIEW.md
    ├── DPDK_INTERVIEW_ANSWER_CARD.md
    └── DPDK_RESUME_BULLETS.md
```

## 推荐执行

```bash
cd track-dpdk/project-dpdk-v17-legacy-review

./scripts/00_make_review_bundle.sh
./scripts/01_generate_portfolio_summary.sh
```

生成后重点看：

```text
records/<timestamp>-v17-legacy-review/REVIEW_BUNDLE.md
reports/DPDK_V17_LEGACY_REVIEW.md
reports/DPDK_INTERVIEW_ANSWER_CARD.md
reports/DPDK_RESUME_BULLETS.md
```

## 与后续关系

这一站完成后，后面再回头补：

```text
project-dpdk-media-gateway-lite
  -> PASS_TRAFFIC
  -> PASS_FORWARDING
  -> PASS_REWRITE
```

之后再做整个 `track-dpdk` 的总结包和简历作品化收口。
