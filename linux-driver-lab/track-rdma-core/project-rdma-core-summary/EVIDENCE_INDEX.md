# Evidence Index

| 主题 | 源码 | 原理 | 测试证据 |
| --- | --- | --- | --- |
| verbs 对象生命周期 | `../lab-rdma-verbs-object-lifecycle/src/` | `../lab-rdma-verbs-object-lifecycle/docs/VERBS_OBJECT_MODEL.md` | `../lab-rdma-verbs-object-lifecycle/tests/TEST_RECORD_20260701.md` |
| MR flags 与 key | `../lab-rdma-memory-region-deep-dive/src/` | `../lab-rdma-memory-region-deep-dive/docs/MEMORY_REGION_MODEL.md` | `../lab-rdma-memory-region-deep-dive/tests/TEST_RECORD_20260701.md` |
| QP 状态机 | `../lab-rdma-qp-state-machine/src/` | `../lab-rdma-qp-state-machine/docs/QP_STATE_MODEL.md` | `../lab-rdma-qp-state-machine/tests/TEST_RECORD_20260701.md` |
| RC SEND/RECV | `../lab-rdma-rc-pingpong/src/main.c` | `../lab-rdma-rc-pingpong/docs/RC_DATA_PATH.md` | `../lab-rdma-rc-pingpong/tests/TEST_RECORD_20260701.md` |
| one-sided | `../lab-rdma-one-sided-read-write/src/main.c` | `../lab-rdma-one-sided-read-write/docs/ONE_SIDED_MODEL.md` | `../lab-rdma-one-sided-read-write/tests/TEST_RECORD_20260701.md` |
| UD/RoCEv2 | `../lab-rdma-ud-rocev2-model/src/main.c` | `../lab-rdma-ud-rocev2-model/docs/UD_TRANSPORT_MODEL.md` | `../lab-rdma-ud-rocev2-model/tests/TEST_RECORD_20260701.md` |

## 最终复现

```bash
base=/home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core
for lab in \
  lab-rdma-verbs-object-lifecycle \
  lab-rdma-memory-region-deep-dive \
  lab-rdma-qp-state-machine \
  lab-rdma-rc-pingpong \
  lab-rdma-one-sided-read-write \
  lab-rdma-ud-rocev2-model; do
    make -C "$base/$lab" test || exit 1
done
```
