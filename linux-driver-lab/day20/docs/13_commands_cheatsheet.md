# Day20 常用命令速查

## 1. 最常用入口

### 只检查运行件是否齐

```bash
./run_day20_suite.sh dry-run
```

### 跑完整回归

```bash
./run_day20_suite.sh all
```

### 分 mode 跑

```bash
./run_day20_suite.sh smoke
./run_day20_suite.sh trace
./run_day20_suite.sh perf
./run_day20_suite.sh stress
```

---

## 2. 看结果

### 最近一次结果

```bash
./run_day20_suite.sh latest
```

### 指定 record

```bash
./run_day20_suite.sh latest 20260315-121428-day20-all-arm64-virt
```

### 汇总视图

```bash
./run_day20_suite.sh summary
```

### suite 自检 / 交付状态

```bash
./run_day20_suite.sh verify
```

---

## 3. 原始入口仍然保留

```bash
./run_day20_regression.sh --dry-run
MODE=all ./run_day20_regression.sh
./run_day20_summary.sh
./run_day20_latest.sh
./run_day20_verify.sh
```

---

## 4. 建议的日常顺序

### 修改脚本后

```bash
./run_day20_suite.sh verify
```

### 准备真实回归前

```bash
./run_day20_suite.sh dry-run
./run_day20_suite.sh latest
```

### 跑完真实回归后

```bash
./run_day20_suite.sh summary
./run_day20_suite.sh latest
./run_day20_suite.sh verify
```
