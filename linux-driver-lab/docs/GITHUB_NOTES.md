# GitHub 使用说明

## 建议仓库形态

推荐把仓库根目录整理成：

```text
driver-lab/
├── kernel-src/
│   ├── README.md
│   ├── linux-5.15.10/
│   │   ├── README.md
│   │   └── .gitkeep
│   └── busybox-1.36.1/
│       ├── README.md
│       └── .gitkeep
└── linux-driver-lab/
```

其中：

- `linux-driver-lab/` 提交你的项目代码和文档
- `kernel-src/` 只提交目录骨架和安装说明
- 不提交完整内核树和 BusyBox 源码树

## 建议仓库名

- `linux-driver-lab`
- `linux-driver-study-lab`

我更推荐：

```text
linux-driver-lab
```

## 不建议提交的内容

- 完整的 `linux-5.15.10/` 源码树
- 完整的 `busybox-1.36.1/` 源码树
- `rootfs/`
- `rootfs.img`
- `*.ko`
- `*.o`
- `*.mod.c`
- `Module.symvers`
- `modules.order`
- 编译生成的用户态二进制

## 建议提交的内容

- `.c / .h / Makefile / build.sh / README.md`
- `docs/` 下的知识总结与路线图
- 回归脚本
- `kernel-src/README.md`
- `kernel-src/README.md`
- `kernel-src/README.md`
- `.gitkeep`

## build.sh 路径建议

建议所有 `build.sh` 优先支持相对路径：

```text
../kernel-src/linux-5.15.10/build/x86
../kernel-src/busybox-1.36.1/output/x86
```

同时兼容你历史上的旧路径：

```text
/home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
/home/wq7/workspace/kernel-src/busybox-1.36.1/output/x86
```

这样别人 clone 到任何目录后都更容易复用。

## Day07 文档补齐项

当前仓库已经把 Day07 的收口内容也纳入提交范围，建议一并提交：

- `day07/README.md`
- `docs/W1_REVIEW.md`

它们分别用于说明“仓库怎么整理”和“W1 这一阶段学到了什么、有什么风险、如何回归”。
