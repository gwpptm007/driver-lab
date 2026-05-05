# REVIEW_BUNDLE - project-user-space-fastpath

## Record directory

`/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/project-user-space-fastpath/records/20260505_202300-user-space-fastpath`

## Checklist

| Item | Status |
|---|---|
| ENV_CHECK.txt | DONE |
| BUILD.log | DONE |
| PREPARE_VMXNET3.txt | DONE |
| FASTPATH_SINGLE_PORT.log | DONE |
| FASTPATH_TWO_PORT.log | MISSING |
| FASTPATH_VDEV_NULL_PAIR.log | MISSING |
| FASTPATH_REWRITE_DEMO.log | MISSING |
| COLLECT_STATS.txt | DONE |

## Evidence grep

| Evidence | Found |
|---|---|
| fastpath-lite config | YES |
| policy: promisc | YES |
| rewrite rules | YES |
| port started | YES |
| available/initialized ports | YES |
| enter fastpath loop | YES |
| fastpath-lite software stats | YES |
| rte_eth_stats | YES |
| bye | YES |

## Suggested verdict

- `PASS_SMOKE`: BUILD + one of SINGLE_PORT/VDEV_NULL_PAIR succeeds and logs contain init/stats/bye.
- `PASS_PROJECT`: above plus UDP-only/rewrite demo logs show policy and rewrite rules.
- `PASS_FORWARDING`: two physical/vhost/virtio ports receive external traffic and tx/rx counters increase.

## Reviewer notes

1. 当前 VMware 测试机只有一个专用 VMXNET3 DPDK 口时，先按 `PASS_SMOKE` 验收。
2. 有两个 DPDK 端口或接入 vhost/virtio-user 后，再按 `PASS_FORWARDING` 验收。
3. `rx=0/tx=0` 不自动判失败；没有外部发包源时，只能证明初始化和 loop，不证明转发吞吐。
