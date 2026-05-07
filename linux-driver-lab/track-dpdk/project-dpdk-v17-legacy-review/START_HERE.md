# START_HERE - project-dpdk-v17-legacy-review

## 目标

这不是跑流量的 lab，而是一次“旧项目经验复盘 + 现代 DPDK 对照 + 面试表达整理”。

你要把过去的 DPDK v17 媒体面项目经验整理成下面这条线：

```text
DPDK v17 实战经验
  -> 现代 DPDK API/工程方式对照
  -> 当前 track-dpdk 代码和记录佐证
  -> 面试可讲清楚的数据面项目
  -> 简历可写的项目描述
```

## 执行

```bash
cd track-dpdk/project-dpdk-v17-legacy-review

./scripts/00_make_review_bundle.sh
./scripts/01_generate_portfolio_summary.sh
```

## 验收

通过条件不是 RX/TX 数字，而是材料完整度：

```text
1. 能讲清楚 DPDK v17 项目数据路径
2. 能讲清楚 hugepage / mempool / mbuf / PMD / burst 的关系
3. 能讲清楚 KNI、UIO、VFIO、vhost-user、virtio-user 的位置
4. 能把旧项目里的 UDP 媒体面映射到当前 media-gateway-lite
5. 能形成简历 bullet 和面试回答
```

## 后续

本项目完成后，建议再回到：

```text
track-dpdk/project-dpdk-media-gateway-lite
```

补真实流量、转发和 rewrite 验证。
