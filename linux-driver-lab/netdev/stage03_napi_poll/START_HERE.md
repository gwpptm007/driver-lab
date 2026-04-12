# stage03_napi_poll / START_HERE

建议按下面顺序进入 stage03：

1. 先看 `README.md`
2. 再看 `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
3. 再看 `docs/02_NAPI_MOTIVATION_AND_MODEL.md`
4. 再看 `docs/03_PENDING_QUEUE_AND_POLL_PATH.md`
5. 然后读 `driver/netdev_stage03.c`
6. 最后再看 `docs/05_TEST_AND_ACCEPTANCE.md`

## 这阶段最重要的一句话

> stage03 不是“学会调用 napi API”，而是学会：
> **为什么要把 RX 处理从‘每包立刻处理’切到‘先排队、后 poll 批处理’。**
