# START_HERE

## 这个 Project 什么时候开始

建议在以下三个 Lab 都至少完成一轮后开始：

- `../lab-virtio-tap-bridge-path/`
- `../lab-virtio-vhost-kick-notify/`
- `../lab-two-guest-bridge-flow/`

## 最小收口流程

```bash
cd track-virtual-net/project-virtual-net-end-to-end

./scripts/check_env.sh
REC=$(./scripts/bootstrap_project_record.sh)

./scripts/collect_input_lab_refs.sh "$REC"
./scripts/generate_final_topology_stub.sh "$REC"
./scripts/generate_review_bundle.sh "$REC"
```

然后人工补：

- `FINAL_PROJECT_REPORT.md`
- `SHARE_SCRIPT.md`
- `EVIDENCE_INDEX.md`

## 当前项目的核心问题

最后要回答：

1. guest virtio_net 到 host bridge 的路径是什么？
2. vhost=on 和 vhost=off 差异是什么？
3. guest-to-guest 的 L2 路径是什么？
4. 哪些证据证明这些路径跑通过？
5. 做完这条线后，为什么下一步自然接 DPDK/vhost-user？
