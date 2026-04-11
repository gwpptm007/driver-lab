# Day16 NEXT_STEPS

## 当前立即要做的事

### 执行位置

**宿主机**：

```bash
cd ~/workspace/driver-lab/kernel-src/linux-5.15.10
```

### 当前 round2b 之后的下一步

1. 重新编译内核
2. 同步新的 `Image`
3. 回到 `day15/` 执行 `./build.sh`
4. 进入 guest 运行：

```sh
/bin/day15_guest_collect.sh
cat /tmp/day15-baseline/metrics.env
cat /tmp/day15-baseline/available_tracers.txt
```

5. guest 通过后，宿主机执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day15/collect
PROMPT='~ # ' SCENARIO_ID='day16-round2b-arm64-virt' ./host_collect.sh
```
