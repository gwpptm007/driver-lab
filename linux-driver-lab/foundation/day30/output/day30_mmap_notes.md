# day30 mmap 零拷贝笔记

## 一句话结论
Day30 的关键不是“把一个 `mmap` 回调写出来”，而是证明：
**用户态已经能直接看到并操作 coherent DMA buffer，本轮 DMA 的最终数据比对也发生在用户态。**

## 你最该记住的四件事

1. `dma_alloc_coherent()` 分配的 buffer 很适合做当前 day30 的 `mmap` 教学实验。
2. `dma_mmap_coherent()` 是当前设计里最自然的映射接口。
3. `run_ok=1` 只表示 DMA 两段完成，不等于 `verify_ok=1`。
4. 非法 `mmap` 长度与 offset 被拒绝，是 day30 的正式验收项，不是可有可无的补充项。
