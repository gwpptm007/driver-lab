# day28 证据索引说明

## 1. 这个文件回答什么问题

很多时候“结论写得对”还不够，必须回答：

> 这条结论到底是从哪个文件里来的？

因此 day28 会生成一份：

- `output/day28_evidence_index.md`

这份索引不是总结，它是“证据定位表”。

---

## 2. 如何使用证据索引

### 场景 A：你只想快速知道 W4 做成了什么
先看：

- `output/day28_w4_summary.md`

### 场景 B：你想核对某个结论是不是有原始证据
再看：

- `output/day28_evidence_index.md`

举例：

- 想确认 day25 中断有没有真的增长
  - 就去索引里找 `day25` 的：
    - `irq-count-before.txt`
    - `irq-count-after.txt`
    - `proc-interrupts-before.txt`
    - `proc-interrupts-after.txt`

- 想确认 day27 是否真的 200 次全通过
  - 就去索引里找：
    - `loop-summary.txt`
    - `run-summary.md`
    - `serial.log`

---

## 3. day22 为什么要单独说明

当前仓库里的 day22 有一个特殊情况：

- `run-summary.md` 旧版写成了 `否`
- 但 `serial.log` 中已经明确有：
  - `1af4:1110`
  - `LSPCI_VV_NN`
  - `DMESG_PCI`
  - `===DAY22:COMPLETE===`

所以在 day28 的证据索引中：

- day22 的“主结论”必须以 `serial.log` 为准
- 旧版 `run-summary.md` 只作为“历史误判样本”保留

---

## 4. 证据索引生成方式

不是手写整理，而是通过：

```bash
bash scripts/01_collect_w4_evidence.sh
python3 scripts/02_generate_w4_summary.py
```

自动扫描 day22~day27 的真实 `records/` 生成。

这样后续你有新的 run id，也能重新生成。
