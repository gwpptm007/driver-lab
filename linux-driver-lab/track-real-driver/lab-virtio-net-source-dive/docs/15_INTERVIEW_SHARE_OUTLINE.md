# 15_INTERVIEW_SHARE_OUTLINE

> 当你完成这个 Lab 后，可以用这份提纲做组内分享、复盘或面试表达。

## 主题一：为什么 stage14 后不再继续 stage15
- 线性 stage 已经完成教学驱动主线
- 继续 stage15 会把“课程推进”和“真实驱动专题”混在一起
- 因此切到 `track / lab / project`

## 主题二：为什么真实驱动第一站选 virtio_net
- 与 `netdev/stage00~stage14` 连续性最强
- 虚拟网卡驱动比复杂物理 NIC 更适合作为第一站
- 方便后面过渡到 `track-virtual-net`

## 主题三：我用什么方法读 virtio_net
- Round1：骨架与 probe
- Round2：TX/RX 路径
- Round3：feature / ethtool / XDP
- 脚本辅助：symbol / snippets / trace / mapping report

## 主题四：我学到了什么
- 教学驱动与真实驱动的共同点
- 教学驱动与真实驱动的差异点
- 下一步最适合做什么 patch/trace 实验
