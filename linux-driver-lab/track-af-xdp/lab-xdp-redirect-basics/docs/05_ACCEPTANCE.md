# 05_ACCEPTANCE

## PASS_BASIC

满足以下条件即可判定第一站基本通过：

```text
BUILD.log 中 BPF object 编译成功
BUILD.log 中 xdp_loader 编译成功
XDP_PASS.log 中 attach 成功
XDP_PASS.log 中 stats 周期输出
XDP_PASS.log 中 detach 成功
REVIEW_BUNDLE.md 生成
```

## PASS_ACTION

在 PASS_BASIC 基础上：

```text
XDP_DROP.log 存在
XDP_DROP.log 中 action=drop
drop/pass stats 有输出
```

## REDIRECT_MODEL_READY

```text
BPF 程序包含 xsks_map
XDP_REDIRECT_DRYRUN.log 存在
文档说明没有 AF_XDP socket 时不判定 PASS_AF_XDP
```

## 不允许夸大

当前 lab 不能写成：

```text
AF_XDP socket 收包成功
zero-copy 成功
用户态 forwarder 成功
```

这些是后续 lab/project 的目标。
