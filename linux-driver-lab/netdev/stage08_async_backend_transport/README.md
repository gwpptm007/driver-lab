# stage08_async_backend_transport

> stage07 解决"队列模型"，stage08 解决"设备边界与异步完成模型"。

## 本阶段要解决什么

stage07 里的 backend 仍然偏同步：
- `ndo_start_xmit()` 提交后，几乎立刻触发 backend 处理
- backend 更像"驱动里的一个函数角色"

stage08 的目标：把"同步伪 backend"推进成"前端驱动 + 后端 worker"分离的异步 transport 模型。

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与模型
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准

## 推荐先看

```bash
cat START_HERE.md
```
