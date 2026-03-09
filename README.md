# driver-lab

一个面向初学者的 Linux 驱动学习项目。

仓库分为两部分：

- `kernel-src/`：准备实验环境
- `linux-driver-lab/`：放每天的驱动代码、脚本和文档

---

## 顶层目录

```text
driver-lab/
├── kernel-src/
│   ├── README.md
│   ├── linux-5.15.10/
│   │   ├── src/
│   │   ├── build/
│   │   │   ├── x86/
│   │   │   └── arm64/
│   │   └── output/
│   │       ├── x86/
│   │       └── arm64/
│   └── busybox-1.36.1/
│       ├── src/
│       ├── build/
│       │   ├── x86/
│       │   └── arm64/
│       └── output/
│           ├── x86/
│           └── arm64/
└── linux-driver-lab/
    ├── README.md
    ├── docs/
    ├── day01/
    ├── day02/
    ├── day03/
    ├── day04/
    ├── day05/
    ├── day06/
    ├── day07/
    ├── day08/
    └── day09/
```

---

## 使用顺序

1. 先阅读 `kernel-src/README.md`，准备 Linux 内核和 BusyBox 环境
2. 再阅读 `linux-driver-lab/README.md`
3. 按 day 目录逐步实验

---

## 说明

仓库中不提交 Linux 和 BusyBox 的完整源码内容。

使用时请先把源码压缩包放到 `kernel-src/` 根目录，再按 `kernel-src/README.md` 的说明解压、编译并准备各平台产物。
