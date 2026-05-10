# project-dpdk-track-summary

> DPDK track 阶段性总收口项目。

这个目录专门存放 DPDK track 的总报告、作品集说明、面试材料、简历素材、backlog 和归档脚本。

设计原则：

```text
track-dpdk/ 根目录只保留入口、路线、docs 和各 lab/project 目录；
阶段性总结材料不再散放在 track-dpdk/ 根目录；
summary 自己维护 reports/、records/、scripts/，和其他 project 风格保持一致。
```

## 目录结构

```text
project-dpdk-track-summary/
├── README.md
├── START_HERE.md
├── reports/
│   ├── README.md
│   └── final/
│       ├── DPDK_TRACK_REPORT.md
│       ├── DPDK_PROJECT_PORTFOLIO.md
│       ├── DPDK_INTERVIEW_NOTES.md
│       ├── DPDK_RESUME_MATERIAL_FINAL.md
│       └── DPDK_BACKLOG.md
├── scripts/
│   ├── 00_make_track_report_bundle.sh
│   └── 01_collect_status_snapshot.sh
└── records/
```

## 当前状态

```text
DPDK_TRACK_REPORT              READY
DPDK_PROJECT_PORTFOLIO         READY
DPDK_INTERVIEW_NOTES           READY
DPDK_RESUME_MATERIAL_FINAL     READY
DPDK_BACKLOG                   READY
```

## 快速查看

```bash
cat reports/final/DPDK_TRACK_REPORT.md
cat reports/final/DPDK_INTERVIEW_NOTES.md
cat reports/final/DPDK_RESUME_MATERIAL_FINAL.md
cat reports/final/DPDK_BACKLOG.md
```

## 生成归档 bundle

```bash
cd track-dpdk/project-dpdk-track-summary
./scripts/00_make_track_report_bundle.sh
```

生成结果会进入：

```text
project-dpdk-track-summary/records/<timestamp>-dpdk-track-report/
```
