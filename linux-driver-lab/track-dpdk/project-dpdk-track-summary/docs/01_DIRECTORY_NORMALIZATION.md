# 01_DIRECTORY_NORMALIZATION

## 本次整理目标

上一版 `track-dpdk/` 根目录下存在：

```text
records/
reports/
scripts/
```

这些目录和各个 `lab-*` / `project-*` 的自包含风格不一致。

本次调整为：

```text
track-dpdk/
├── README.md
├── ROADMAP_NEXT.md
├── docs/
├── lab-*/
└── project-*/
```

阶段性收口材料统一归入：

```text
track-dpdk/project-dpdk-track-summary/
├── reports/
├── records/
└── scripts/
```

## 这样做的好处

```text
1. track-dpdk 根目录只承担导航职责
2. 每个 lab/project 都自带 docs/scripts/records/reports
3. 总结材料也作为一个 project 管理
4. 后续扩展不会继续污染 track 根目录
```
