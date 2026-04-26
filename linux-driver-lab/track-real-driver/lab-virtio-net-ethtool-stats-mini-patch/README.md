# lab-virtio-net-ethtool-stats-mini-patch

> 所属：`track-real-driver/`

## 一句话定位

这是 `lab-virtio-net-runtime-observe/` 之后的第二个真实实验：

> **不先碰重的 TX/RX 主路径语义，而是先在 `virtio_net` 的 ethtool / stats / control-plane 这一层做一个低风险、易验证、能体现真实驱动理解的小 patch。**

## 为什么先做这个

在当前项目推进顺序里，这个实验排在：

1. `lab-virtio-net-source-dive/`
2. `lab-virtio-net-runtime-observe/`
3. **`lab-virtio-net-ethtool-stats-mini-patch/`**

原因很明确：

- `source-dive` 已经建立了对 `virtio_net` 的分层、主路径、事件推进和能力边界理解
- `runtime-observe` 已经提供了运行期 baseline 和日志/trace 证据
- 现在最稳的下一步，不是直接改重主路径，而是先在 `ethtool/stats` 这一层做一个低风险 patch

## 这个 Lab 的核心目标

1. 选一个合适的 `ethtool / stats` 切点
2. 在不破坏主路径的前提下做一个小 patch
3. 建立 before / after 对照
4. 形成：
   - patch
   - records
   - report
   - review notes

## 不建议的方向

当前这一步不建议直接做：

- 重 TX/RX 语义 patch
- 大范围 feature/offload/XDP 语义变更
- 并行改多个真实 NIC 驱动
- 一上来就把 `virtio_net` 扩展成过大的实现工程

## 推荐推进顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_PATCH_POINT_SELECTION.md`
4. `docs/03_CODE_PATH_AND_STATS_SURFACE.md`
5. `docs/04_BASELINE_AND_BEFORE_AFTER_PLAN.md`
6. `docs/05_PATCH_EXECUTION_FLOW.md`
7. `docs/06_VALIDATION_AND_REVIEW.md`
