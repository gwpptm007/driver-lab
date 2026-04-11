# Day18 技术路线

## 1. 总体路线

Day18 采用“骨架复用 + 分类升级”的路线：

- 复用 day17 的执行骨架
- 升级配置表达方式
- 增加分类与等价性检查

## 2. 骨架复用部分

继续复用的能力包括：

- QEMU virt 启动
- BusyBox initramfs
- demo_regmap.ko
- debugfs / tracing / function_graph
- perf 集成
- host / guest 自动采样
- records 归档

## 3. 分类升级部分

### 3.1 baseline

只表达“最小可观测性 baseline”。

### 3.2 round2b_legacy

沿用 day17 的表达：

- trim_round1 => 关 PCI / SCSI
- trim_round2b => 再关 NET

### 3.3 classified

按类别拆：

- required
- platform
- debug
- perf
- trim

## 4. 证据链升级

Day18 新增：

- `kernel.savedefconfig`
- `category_manifest.csv`
- `equivalence` 检查输出

## 5. 为什么要保留 legacy

因为 Day18 不是断代式重做，而是要说明“从 day17 到 day18 的演进”：

- 执行链没变
- 裁剪目标大体没变
- 表达方式变得更可解释了
