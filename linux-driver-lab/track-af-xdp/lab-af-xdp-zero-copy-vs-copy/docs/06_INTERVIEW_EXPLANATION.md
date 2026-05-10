# 06_INTERVIEW_EXPLANATION

面试描述可以这样讲：

> 我没有把 AF_XDP zero-copy 简化成一个参数开关，而是单独做了 copy/zero-copy 支持边界实验。实验里区分了 XDP attach mode 和 AF_XDP bind mode：skb/native 是 XDP 程序挂载层面的差异，copy/zero-copy 是 AF_XDP socket 与 UMEM 和驱动交互方式的差异。很多虚拟网卡或未实现 ZC 的驱动无法支持 zero-copy，这时正确做法是记录失败原因并 fallback 到 copy mode，而不是把程序硬写死为 zero-copy。

可强调点：

- 知道 `skb + copy` 是兼容性基线；
- 知道 `native + zero-copy` 依赖驱动；
- 会通过日志和返回码判断支持边界；
- 能解释为什么 VMware/vmxnet3 不一定能跑通 zero-copy；
- 能把 AF_XDP 与 DPDK PMD 模式做对比。
