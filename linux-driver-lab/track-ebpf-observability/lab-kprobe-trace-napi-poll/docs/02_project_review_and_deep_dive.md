# 项目复盘与深度理解

## 项目收敛后的设计

当前 lab 刻意保留少量文件：

```text
README.md                 入口和运行方式
docs/01_learning_notes_and_principles.md  学 NAPI/softirq/kprobe 原理
docs/02_project_review_and_deep_dive.md   解释项目为什么这样设计
docs/03_test_record_20260518_vm.md        保留真实测试过程
scripts/                  可复跑脚本
records/<本轮目录>/        当前实测证据
reports/report.md         最终报告
```

删除 `START_HERE.md`、旧拆分 docs、静态 `probes/` 和旧 records，是为了让学习重点集中在“观测链路”和“证据判断”上。

## 为什么动态生成 bpftrace 脚本

一开始固定写：

```text
kprobe:napi_poll
kretprobe:napi_poll
```

看起来直观，但真实内核上可能失败。测试机就出现过：

```text
napi_poll is not traceable
ERROR: Error attaching probe
RC=255
```

如果仍然把这种日志判为 PASS，就是实验设计失败。

现在脚本改成：

```text
先列出当前内核可见符号
再从候选符号里选择第一个可用 probe
最后把实际运行的 .bt 脚本写入 records/<本轮目录>/
```

这样每次复盘都能回答：

```text
这次到底挂了哪个函数？
为什么不是 napi_poll？
脚本运行时生成的真实 bpftrace 程序是什么？
```

## 为什么 softirq 和 NAPI 要解耦

旧版把 softirq tracepoint 和 `kprobe:napi_poll` 放进同一个 bpftrace 程序。问题是：只要 `napi_poll` attach 失败，整个程序退出，连 softirq 证据也丢了。

当前版本的原则是：

```text
softirq tracepoint 是基础证据，必须尽量保留。
NAPI poll kprobe 是增强证据，能挂就一起观察。
不能因为增强证据失败，把基础证据也打掉。
```

这就是观测工具设计里的一个重要经验：把稳定观测点和高风险观测点分层。

## review bundle 的判定逻辑

`07_make_review_bundle.sh` 不再只看文件是否存在，而是识别日志内容：

```text
YES                 正常运行或超时结束，且没有错误特征
NO_MISSING          证据文件缺失
NO_ERROR            bpftrace 不存在、语法错误、未知符号等硬错误
NO_ATTACH_FAILED    kprobe attach 失败，例如 RC=255
WARN_NOT_TRACEABLE  当前内核无可用符号或符号不可 trace
```

其中最关键的是：

```text
RC=124 是 timeout 正常结束
RC=255 是 attach 失败
```

这两个不能混淆。

## 当前版本的学习价值

这个 lab 最值得吸收的不是某个命令，而是排障思路：

```text
1. 先确认环境，不急着解释内核行为。
2. 先列符号，不假设函数名一定可挂。
3. 先证明 attach 成功，再谈观测结果。
4. softirq 和 NAPI 分层观测，减少单点失败。
5. 结论必须能从 records 里的原始日志反推出来。
```

这套方法可以迁移到后续 driver、XDP、AF_XDP、DPDK 与内核网络性能分析中。

## 下一站学习建议

这个 lab 到这里可以收尾。它已经把 `NET_RX softirq -> NAPI poll` 这段路径观测清楚了。

按照 `track-ebpf-observability/README.md` 的阶段顺序，下一站是：

```text
Phase 3: lab-tracepoint-skb-path
```

原因是 Phase 1/2 已经用 bpftrace 和 kprobe 证明了 RX/TX、softirq、NAPI poll 会发生；Phase 3 要把观察点推进到 skb 路径，并优先使用更稳定的 tracepoint。

```text
driver RX / NAPI poll
  -> skb allocation / receive path
  -> skb tracepoint
  -> protocol stack / TX path
```

建议下一站重点回答：

```text
1. 哪些 skb 相关 tracepoint 对 RX/TX 路径最有用？
2. tracepoint 和 kprobe 在稳定性、字段、可移植性上有什么差异？
3. 如何从 tracepoint 字段里读出 device、skb length、protocol 等信息？
4. 如何把 softirq/NAPI 的证据和 skb 层证据串成一条完整路径？
```

`XDP / AF_XDP` 仍然值得后续学习，但它不是这条 track 里紧接本 lab 的下一站。
