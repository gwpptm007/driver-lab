# Day17 round compare 证据链增强版

## 1. 这版解决了什么问题

前一版 round compare 只能告诉我们：

- baseline / round1 / round2b 是否 PASS
- boot_ms / image_kib / rootfs_kib 有没有变化

但如果三轮数字一样，我们并不能马上知道：

1. fragment 根本没落到最终 `.config`
2. `.config` 变了，但 `Image` 没变
3. `Image` 变了，但 rootfs 没变
4. 构建流程没有真正按 profile 做干净重建

这版补上的就是“证据链”。

## 2. 每轮 records 目录会新增什么

每次 `run_profile_collect.sh <profile>` 跑完后，会在该轮 `records/<timestamp>-<scenario>/build_evidence/` 下新增：

- `kernel.config`
- `kernel.config.focus.txt`
- `applied_fragments.txt`
- `fragments/*.fragment`
- `Image`
- `Image.sha256`
- `rootfs.img`
- `rootfs.img.sha256`
- `demo_regmap.ko`
- `demo_regmap.ko.sha256`
- `modules.order`
- `modules.builtin`
- `perf_bundle_manifest.txt`
- `artifact_evidence.env`

## 3. compare_results.py 新增什么输出

现在批量跑完后会自动生成：

- `compare-<timestamp>.csv`
- `compare-<timestamp>.md`
- `compare-<timestamp>-baseline_vs_round1.diff`
- `compare-<timestamp>-round1_vs_round2b.diff`
- `compare-<timestamp>-baseline_vs_round2b.diff`

其中 `.md` 会直接给出：

- `kernel_config_sha256`
- `kernel_image_sha256`
- `rootfs_img_sha256`
- `config_diff_vs_baseline`
- `image_hash_vs_baseline`
- `rootfs_hash_vs_baseline`

## 4. 怎么判断 fragment 是否真正生效

### 情况 A：config sha 都一样
优先判断为：fragment 没有真正落到 `.config`。

### 情况 B：config sha 不一样，但 image sha 一样
通常说明：关掉的 symbol 不在当前 `virt + arm64` 产物路径里，或本来就是 `n`。

### 情况 C：image sha 变了，但 boot/image_kib 变化很小
说明裁剪确实进了内核产物，但收益暂时不大。

## 5. 推荐排查顺序

1. 看 `compare-*.md`
2. 看 `compare-*-*.diff`
3. 打开每轮 `build_evidence/kernel.config`
4. 对照 `build_evidence/applied_fragments.txt`
5. 必要时直接比较三轮 `Image.sha256`


## 2026-03-14 fix3 说明

- 重新挑选了 round1 / round2b 的 trim symbol，确保它们在当前 baseline `.config` 里确实为 y/m，而不是本来就关闭。
- `apply_config.sh` 现在优先使用内核自带 `scripts/config` 改写 `.config`，并支持 y/m/n 三种值。
- 这样重新跑 `./run_compare_rounds.sh` 后，理论上至少应看到 `kernel_config_sha256` 变化；若仍不变，就需要继续看具体 symbol 依赖是否把选项重新拉回来了。
