# 05_manual_test_flow - guest 手工验证流程

## 1. 为什么 Day17 还要先手工验证

因为 Day17 虽然已经是独立目录，但第一次跑时仍然有多个环节：

- 内核 profile 是否真的生效
- rootfs 是否正确生成
- guest_collect.sh 是否真的被打进 rootfs
- DTB 注入是否成功
- demo_regmap.ko 是否能加载
- tracing 路径到底挂在哪

第一次先手工看一遍，后面再上自动采样会更稳。

## 2. guest 里建议顺序

```sh
ls -l /bin/day17_guest_collect.sh
ls -l /demo_regmap.ko
insmod /demo_regmap.ko
cat /sys/kernel/debug/demo_regmap/snapshot
echo 1 > /sys/kernel/debug/demo_regmap/trigger
/bin/day17_guest_collect.sh
cat /tmp/day17-baseline/metrics.env
```


## 补充

如果你想按最终收口后的完整顺序走一遍，请再看：

- `docs/11_day17_full_test_checklist.md`
