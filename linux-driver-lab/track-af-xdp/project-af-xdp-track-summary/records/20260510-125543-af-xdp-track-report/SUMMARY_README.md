# project-af-xdp-track-summary

> AF_XDP track 阶段性总收口项目。

这个目录专门存放 AF_XDP track 的总报告、作品集说明、面试材料、简历素材、backlog 和归档脚本。

设计原则：

```text
track-af-xdp/ 根目录只保留入口、路线、docs 和各 lab/project 目录；
阶段性总结材料不散放在 track-af-xdp/ 根目录；
summary 自己维护 docs/、reports/、records/、scripts/，和其他 lab/project 风格保持一致。
```

## 目录结构

```text
project-af-xdp-track-summary/
├── README.md
├── START_HERE.md
├── docs/
│   ├── 01_TRACK_OVERVIEW.md
│   ├── 02_XDP_TO_AF_XDP_ARCHITECTURE.md
│   ├── 03_LAB_STATUS_MATRIX.md
│   ├── 04_BACKLOG_AND_RETEST_PLAN.md
│   └── 05_INTERVIEW_EXPLANATION.md
├── reports/
│   ├── README.md
│   └── final/
│       ├── AF_XDP_TRACK_REPORT.md
│       ├── AF_XDP_PROJECT_PORTFOLIO.md
│       ├── AF_XDP_INTERVIEW_NOTES.md
│       ├── AF_XDP_RESUME_MATERIAL.md
│       └── AF_XDP_BACKLOG.md
├── scripts/
│   ├── 00_make_track_report_bundle.sh
│   └── 01_collect_status_snapshot.sh
└── records/
```

## 当前状态

```text
AF_XDP_TRACK_REPORT          READY
AF_XDP_PROJECT_PORTFOLIO     READY
AF_XDP_INTERVIEW_NOTES       READY
AF_XDP_RESUME_MATERIAL       READY
AF_XDP_BACKLOG               READY
```

## 快速查看

```bash
cat reports/final/AF_XDP_TRACK_REPORT.md
cat reports/final/AF_XDP_INTERVIEW_NOTES.md
cat reports/final/AF_XDP_RESUME_MATERIAL.md
cat reports/final/AF_XDP_BACKLOG.md
```

## 生成归档 bundle

```bash
cd track-af-xdp/project-af-xdp-track-summary
./scripts/00_make_track_report_bundle.sh
```

生成结果会进入：

```text
project-af-xdp-track-summary/records/<timestamp>-af-xdp-track-report/
```
