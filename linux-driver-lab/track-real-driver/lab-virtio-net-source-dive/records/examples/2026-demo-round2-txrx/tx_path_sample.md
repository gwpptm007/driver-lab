# TX path sample

> 这是示范写法，不是源码逐行解析。

建议最终 TX 路径图至少包含：

1. netdev 发包入口
2. queue 选择/组织
3. 向 virtqueue 提交
4. notify / kick
5. completion / reclaim
6. stats 或状态回收点

并在每个点后面补一句：
- 这个点和自己哪个 stage 最像
- 这个点在真实驱动里比教学驱动多了什么复杂度
