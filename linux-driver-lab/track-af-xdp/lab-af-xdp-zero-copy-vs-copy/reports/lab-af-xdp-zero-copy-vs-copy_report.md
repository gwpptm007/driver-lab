# lab-af-xdp-zero-copy-vs-copy report

## Status

`READY_TO_TEST`

## Goal

验证 AF_XDP copy mode 与 zero-copy mode 的支持边界，明确当前网卡/驱动组合是否支持 native XDP 和 zero-copy，并形成 fallback 策略。

## Expected result on VMware/vmxnet3

大概率：

```text
skb + copy: pass or best baseline
native + copy: depends on driver/kernel
native + zero-copy: may fail, acceptable if recorded
```

## Next

测试完成后，如果 copy baseline 成功，可以进入：

```text
project-af-xdp-mini-forwarder
```
