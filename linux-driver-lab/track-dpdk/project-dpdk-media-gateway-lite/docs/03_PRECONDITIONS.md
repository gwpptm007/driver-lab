# 03_PRECONDITIONS

开始本项目之前必须满足：

- `project-user-space-fastpath` 至少 `PASS_SMOKE`
- `project-fastpath-traffic-test` 至少 `PASS_TRAFFIC`
- 明确真实流量拓扑
- 明确是否具备两个 DPDK 端口或 vhost/virtio-user 拓扑

不满足时，不建议开始编码，否则会变成“功能堆叠但没有流量证据”。
