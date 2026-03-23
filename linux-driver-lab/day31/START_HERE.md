# day31 START_HERE

建议按下面顺序阅读和执行：

1. 先看 `README.md`，理解 day31 为什么从“功能验证”切到“路径量化”
2. 再看 `docs/01_plan.md`，确认三条 bench 路径与统计口径
3. 看 `docs/03_BENCH_DESIGN.md`，理解当前 tool 的基准定义
4. 第一次进入目录先执行：`chmod +x scripts/*.sh && chmod +x guest/init.day31`
5. 如未准备 `pciutils`，先执行 `bash scripts/01_fetch_pciutils.sh`
6. 执行 `make check && make build-tools && make module && sudo -E make run`
7. 把原始证据沉淀到 `records/<RUN_ID>/`
8. 最后把汇总结论写到 `output/` 模板里

## 今天最重要的一句话

**Day31 的目标不是新增一个花哨能力，而是把 day30 已经跑通的能力，量化成吞吐、延迟和 CPU 占用。**

## 今天不要做过头的点

- 先把 `ioctl / mmap / dma` 三条路径的最小 bench 做通，不要一上来做太多模式
- 先留 raw records，再写总结
- 所有“通过”都要能落到 `records/` 的原始文件中


## 关于 DMA bench 超时预算

当前代码默认已经把：
- `QEMU_TIMEOUT_SEC` 调整为 `360`
- `DAY31_BENCH_ITER / DAY31_BENCH_WARMUP` 调整为 `200 / 20`
- `DAY31_RUN_BENCH_ALL` 调整为 `0`

所以第一次直接按默认流程跑即可。若后续需要完整矩阵，再显式设置 `DAY31_RUN_BENCH_ALL=1`。


## 当前包内 records 的结论

当前包内 `records/day31-local-001` 已经对应一轮默认主链路通过的结果。

第一次阅读时，建议按下面顺序核对：

1. `records/day31-local-001/run-summary.md`
2. `records/day31-local-001/mmap-verify.txt`
3. `records/day31-local-001/bench-ioctl.txt`
4. `records/day31-local-001/bench-mmap.txt`
5. `records/day31-local-001/bench-dma.txt`

看完这五个文件，再回头看 `docs/02_acceptance.md` 与 `docs/07_TEST_RESULT_ANALYSIS.md`，会更容易把“为什么判定 day31 通过”串起来。
