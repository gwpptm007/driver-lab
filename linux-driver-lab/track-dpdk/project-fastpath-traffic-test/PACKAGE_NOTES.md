# PACKAGE_NOTES

本目录是 `project-user-space-fastpath` 后续真实流量验证框架。

它不会复制一份新的 fastpath C 代码，而是复用：

```text
../project-user-space-fastpath/app/build/fastpath-lite
```

这样可以避免测试工程和数据面工程发生代码分叉。

## 当前版本重点

- 明确 `PASS_SMOKE` 与 `PASS_TRAFFIC` 区别
- 当前 pcap 结果统一解释为 `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE`，真实 NIC 另行验收
- 提供外部发包说明
- 提供 stats 对照和 review bundle
- 为后续 `project-dpdk-media-gateway-lite` 做前置验收
