# Day20 交付状态与 suite 自检

## 1. 这一步为什么重要

到了 Day20 后半段，重点已经不只是“能不能跑一次”，而是：

- 这套目录现在到底交付到什么程度；
- 哪些是结构性完成；
- 哪些只是因为运行件还没齐，所以暂时不能做真实回归；
- 每次改完脚本后，怎样快速自检，不靠人工回忆。

所以 Day20 新增了一条独立的自检链：

```bash
./run_day20_verify.sh
```

它会生成：

- `output/day20_delivery_status.md`
- `output/day20_delivery_status.env`

---

## 2. 交付状态里看什么

### 2.1 `SUITE_READY`
表示：

- 目录结构齐；
- 核心 README / docs / guest / host 脚本齐；
- shell 脚本 `bash -n` 通过；
- python 脚本 `py_compile` 通过。

### 2.2 `DELIVERY_READY`
表示：

- `SUITE_READY=1`；
- summary / latest / mode summary 等日常入口文件都已经生成。

### 2.3 `RUNTIME_READY`
表示最近一次 `dry-run` 已确认这些输入件都齐：

- `Image`
- `rootfs.img`
- `virt-*.dtb`
- `demo_regmap.ko`

### 2.4 `REGRESSION_PASS`
表示最近一次真实回归已经通过。

---

## 3. 这几个状态不要混

最容易混淆的是下面两组：

- `SUITE_READY=1` 不代表真实回归已经通过；
- `RUNTIME_READY=0` 不代表 Day20 脚本写坏了，很多时候只是当前代码包没带大文件。

所以实际判断要分层：

1. 先看 suite 结构是不是成熟；
2. 再看运行件是不是齐；
3. 最后看真实回归是不是 pass。

---

## 4. 推荐使用方式

每次你改完 Day20 后，至少做一次：

```bash
./run_day20_summary.sh
./run_day20_verify.sh
./run_day20_latest.sh
```

这样就能很快回答：

- 结构有没有坏；
- 输出入口有没有坏；
- 最近一次 record 现在是什么状态。
