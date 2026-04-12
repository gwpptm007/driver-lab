# stage03_napi_poll / TASKS

## 已落地
- [x] README / START_HERE / 目录骨架补齐
- [x] `driver/netdev_stage03.c`：教学型 NAPI 驱动
- [x] `rx_mode=direct|napi` 两模式切换
- [x] pending RX queue + `napi_struct` + poll drain
- [x] build/load/unload/smoke/report 脚本
- [x] sender / receiver 工具
- [x] debugfs 统计项与 report 模板

## 还需要你在测试机验证
- [ ] `make build-module`
- [ ] `sudo make load`
- [ ] `sudo make smoke`
- [ ] 检查 `debugfs` 统计是否与日志一致
- [ ] 对比 `rx_mode=direct` 与 `rx_mode=napi`
- [ ] 确认 `budget exhausted` 在 burst 场景下可观测
