# PACKAGE_NOTES

本次把 `project-dpdk-media-gateway-lite` 从规划目录升级为可执行项目。

新增内容：

```text
app/      media-gateway-lite C 程序，拆分 config/port/packet/rule/stats 模块
configs/ 规则配置示例
scripts/ 构建、vmxnet3、vdev、双口、rewrite、记录收集脚本
tools/   stats 解析工具
docs/    架构、规则、验收、面试讲法
reports/ 执行看板与报告模板
```

当前版本目标是 `PASS_PROJECT_SMOKE`，真实流量证据仍建议复用前一站 `project-fastpath-traffic-test` 的拓扑推进。
