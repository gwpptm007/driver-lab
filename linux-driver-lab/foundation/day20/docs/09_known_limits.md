# Day20 当前已知限制

## 1. 当前代码包里未附带大体积运行件

为了避免代码包过大，当前目录通常不会把这些运行件完整打包进去：

- `Image`
- `rootfs.img`
- `virt-*.dtb`

所以 `--dry-run` 很可能会先告诉你这些输入件缺失。
这不是 Day20 脚本本身失败，而是运行环境还没对齐。

## 2. demo 模块通常更容易从 records/build_evidence 找到

当前 Day20 已优先尝试寻找：

- 前序 day 根目录下的 `demo_regmap.ko`
- 或前序 records 里的 `build_evidence/demo_regmap.ko`

但这仍不等价于自动补齐全部运行件。

## 3. `MODE=all` 现在会真正合并 smoke/trace/perf/stress 的状态

这一步已经修正了早期骨架里一个常见坑：

- 虽然顺序执行了多个 guest 脚本
- 但宿主机如果只读第一段 `pass_fail.env`
- 后面的 trace / perf / stress 状态就会丢

当前版本会合并全部 `__DAY20_ENV` 区块。

## 4. stress 仍是“快速压力冒烟”，不是长时间 benchmark

Day20 的 stress 目标是：

- 多次装卸
- 连续触发
- 快速扫 dmesg

它的定位是回归保险丝，不是性能极限测试。


## 5. latest report 只能基于已归档 records 判断

`output/day20_latest_report.md` 很方便，但它不会替代真实的 `records/<record_dir>/serial.log`。
结论层和原始证据层仍然要分开看。
