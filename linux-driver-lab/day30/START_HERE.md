# day30 START_HERE

建议按下面顺序阅读和执行：

1. 看 `README.md`
   - 先明确：day30 的主角已经从“内核独占 buffer”切换成“用户态直接看到 buffer”。

2. 看 `docs/01_plan.md`
   - 这里写了 day30 为什么单独做、主线怎么落、哪些先不做。

3. 看 `docs/03_MMAP_DESIGN.md`
   - 这里把 DMA buffer 布局、`mmap` 边界、ioctl 边界说清楚了。

4. 跑 day30：
   ```bash
   source env/day30.env
   make check
   make kernel-module-tree
   make build-tools
   make module
   sudo -E make rootfs
   make backend
   sudo -E make run
   ```

5. 看 `records/<RUN_ID>/`
   - 核心先看：
     - `mmap-verify.txt`
     - `run-result.txt`
     - `dmesg-driver.txt`
     - `run-summary.md`

6. 看 `docs/07_TEST_RESULT_ANALYSIS.md`
   - 这里已经把当前 records 里哪些是明确通过项、哪些是测试样例问题拆开写清楚了。

## 今天最重要的一句话

**不要把 day30 理解成“day29 再加一个 mmap 接口”，而要把它理解成“用户态正式接管 DMA buffer 的第一天”。**

## 今天不要做过头的点

- 不急着做复杂 offset/多页映射
- 不急着做并发和性能评估
- 先把“整页 `mmap` + 一轮零拷贝 round-trip”做通并留证
