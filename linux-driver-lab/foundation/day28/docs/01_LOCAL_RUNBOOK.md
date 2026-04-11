# day28 本地运行手册

## 1. 适用场景

这个 runbook 适合两类情况：

1. 你已经有 day22~day27 的真实 `records/`，想重新生成 W4 汇总。
2. 你想快速检查当前仓库里的 W4 证据是否完整。

---

## 2. 当前目录

所有命令都在：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day28
```

---

## 3. 先补执行位

因为 zip 解压后，shell 脚本的执行位可能丢失，所以先统一执行：

```bash
chmod +x scripts/*.sh
```

---

## 4. 生成 W4 证据快照

```bash
bash scripts/01_collect_w4_evidence.sh
```

这一步会做两件事：

- 扫描 day22~day27 的 `records/`
- 把一份轻量快照写到 `records/w4-evidence-snapshot/`

注意：

- 它不会伪造 records
- 它只会把当前仓库里已经存在的证据重新整理成“可汇总输入”

---

## 5. 生成 W4 汇总文档

```bash
python3 scripts/02_generate_w4_summary.py
```

这一步会更新：

- `output/day28_w4_summary.md`
- `output/day28_evidence_index.md`

---

## 6. 验证是否生成成功

```bash
ls -l output/day28_w4_summary.md
ls -l output/day28_evidence_index.md
```

然后建议直接看：

```bash
sed -n '1,220p' output/day28_w4_summary.md
sed -n '1,260p' output/day28_evidence_index.md
```

---

## 7. 如何判断 day28 自己通过

只要满足下面这些，就可以认为 day28 通过：

1. `output/day28_w4_summary.md` 已生成且内容不是模板
2. `output/day28_evidence_index.md` 已生成且列出了 day22~day27 的真实证据路径
3. 对 day22 的旧 `run-summary.md` 误判有明确说明
4. 对 day22~day27 的主结论都有清晰归纳
5. 文档里已经把 W5 的输入写清楚

---

## 8. 如果后面又更新了 day22~day27 的 records

只需要重新执行：

```bash
bash scripts/01_collect_w4_evidence.sh
python3 scripts/02_generate_w4_summary.py
```

不需要手工改 day28 文档。
