# Day18 需要掌握的知识点

## 1. Kconfig / defconfig / fragment

- 为什么 fragment 适合做 profile 叠加
- 为什么 `olddefconfig` 是必要收敛步骤
- 为什么 `savedefconfig` 能让配置更可读

## 2. 启动链

- `Image` 是怎么被 QEMU 直接加载的
- initramfs 的作用是什么
- `/init` 为什么比 rootfs 磁盘更适合当前实验

## 3. 平台依赖

- PL011 串口
- GIC / IRQ domain
- Device Tree 对 reg/irq 的描述

## 4. 可观测性

- debugfs 的作用
- function_graph 为什么适合讲 IRQ 路径
- perf 在教学实验里的价值是什么

## 5. 工程化能力

- baseline / legacy / classified 三套表达各自解决什么问题
- 为什么 records / compare / diff / sha256 很重要
