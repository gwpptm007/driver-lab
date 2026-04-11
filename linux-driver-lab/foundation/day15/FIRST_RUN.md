# Day15 FIRST_RUN - 第一次执行手册

这份文件只解决一件事：

> **第一次把 Day15 跑起来时，最稳的执行顺序是什么。**

---

## 1. 先做什么

先在宿主机进入：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day15
```

然后按下面顺序走：

1. `./apply_config.sh`
2. `./build.sh`
3. 进 guest 后执行 `/bin/day15_guest_collect.sh`
4. guest 手工验证通过后，再跑 `collect/host_collect.sh`

---

## 2. 为什么不能直接先跑 host_collect.sh

因为 `host_collect.sh` 是自动化汇总脚本。

第一次如果直接跑它，一旦失败，你很难第一时间判断问题是在：

- 内核 config 没补好
- 内核没重编成功
- rootfs 里没带进 guest 采样脚本
- QEMU 没正确启动
- tracing 目录没起来

所以第一次一定是：

> **先手工 guest 验证，再上自动化。**

---

## 3. 手工 guest 阶段最关键看什么

在 guest 里执行：

```sh
/bin/day15_guest_collect.sh
cat /tmp/day15-baseline/metrics.env
```

理想上先看到：

```text
tracing_ok=yes
function_graph_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
```

如果这一步不通，就先别跑 host 自动采集。

---

## 4. 自动采集通过后看什么

自动采集完成后，去看：

```text
day15/records/<timestamp>-day15-baseline-arm64-virt/
```

里面最关键的是：

- `baseline.csv`
- `metrics.env`
- `serial.log`

`serial.log` 是排错第一现场。
