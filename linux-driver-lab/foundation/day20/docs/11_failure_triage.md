# Day20 失败排查顺序

## 1. 先不要一上来就读全部原始日志

Day20 的目标不是把问题藏起来，而是把问题分层：

- 输入件没齐
- 启动没起来
- guest 检查失败
- 回收结果失败

所以排查顺序也应该分层。

## 2. 第一步：看最新结论

```bash
./run_day20_latest.sh
```

如果是：

- `MISSING_INPUTS`：先补运行件
- `READY`：说明 dry-run 已经可以，下一步跑真实回归
- `FAIL`：再继续下面排查

## 3. 第二步：看总摘要

优先看：

- `records/<record_dir>/summary.txt`
- `records/<record_dir>/pass_fail.env`

这里能先告诉你：

- 哪些键失败了
- 哪些键根本没产生

## 4. 第三步：区分宿主机侧还是 guest 侧

### 宿主机侧
看：

- `records/<record_dir>/host_runner.log`

它能帮助判断：

- QEMU 是否启动
- 是否等到 prompt
- guest 脚本是否已上传
- 哪一步命令超时或异常

### guest 侧
看：

- `records/<record_dir>/serial.log`

它是最完整的原始控制台输出。

## 5. 第四步：看专项原始文本

根据失败项再看：

- `trace_excerpt.txt`
- `perf_stat.txt`
- `snapshot_before.txt`
- `snapshot_after.txt`
- `dmesg_tail.txt`

## 6. 建议记住的原则

先看结论，再看阶段，再看原始文本。不要一开始就被几百行串口日志淹掉。
