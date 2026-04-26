# 04_TRACE_PLAN

## 这个项目为什么还需要 trace

因为 patch 不是只要“能编译、能跑”就够了。  
你还需要回答：

- 这次 patch 在运行期到底意味着什么
- patch 前后的现象，是否能和源码路径解释对上
- 哪些证据来自 stats，哪些证据来自 trace，哪些只是推断

## 当前建议的 trace 角色

trace 在这个项目里不是主角，它的定位是：

- 帮 before/after 解释“为什么变了”
- 帮 review 时说明“改动点在运行期处于什么位置”
- 帮后续更深 patch 提供选点依据

## 当前建议

- 保持 trace 点少而准
- 优先围绕：
  - poll
  - RX 上送相关证据
  - queue/poll 事件推进关键点

## 不建议
- 一上来搞很重的 tracepoint 套餐
- 让 trace 复杂度压过 patch 本身
