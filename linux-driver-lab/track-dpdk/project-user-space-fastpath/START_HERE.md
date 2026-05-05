# START_HERE - project-user-space-fastpath

## 目标

这一站不再只是 `testpmd` 或教学 demo，而是落一个可以写进简历/面试讲解的项目型 fastpath：

```text
VMXNET3 / vhost / virtio-user
        ↓
DPDK EAL + mempool + ethdev
        ↓
fastpath-lite
        ↓
ARP / IPv4 / UDP classify
        ↓
UDP-only / rewrite / stats
```

## 推荐执行顺序

```bash
cd track-dpdk/project-user-space-fastpath

./scripts/00_check_env.sh
./scripts/01_build_app.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_fastpath_single_port.sh
./scripts/07_collect_stats.sh
./scripts/08_make_review_bundle.sh
```

## 可选：不碰物理网卡的 vdev smoke

如果只想先验证二端口初始化和 rewrite 参数解析：

```bash
sudo ./scripts/05_run_fastpath_vdev_null_pair.sh
sudo ./scripts/06_run_fastpath_rewrite_demo.sh
./scripts/08_make_review_bundle.sh
```

## 可选：启用 rewrite 示例

```bash
source configs/fastpath-rewrite-example.env
sudo -E ./scripts/03_run_fastpath_single_port.sh
```

## 当前通过标准

### PASS_SMOKE

- `BUILD.log` 成功生成 `fastpath-lite`
- 程序输出 `fastpath-lite config`
- 至少一个 port started
- 进入 `enter fastpath loop`
- 输出 `fastpath-lite software stats`
- 输出 `rte_eth_stats`
- 正常 `bye`

### PASS_PROJECT

在 `PASS_SMOKE` 基础上，额外证明：

- `--udp-only 1` 生效
- `--rewrite` 参数能解析并输出 `rewrite rules`
- records/review bundle 完整

### PASS_FORWARDING

后续接两个物理口、vhost/virtio-user 或外部发包源后：

- RX 计数非 0
- UDP 分类计数非 0
- TX 计数非 0
- rewrite 计数非 0，且抓包验证二/三/四层字段变化
