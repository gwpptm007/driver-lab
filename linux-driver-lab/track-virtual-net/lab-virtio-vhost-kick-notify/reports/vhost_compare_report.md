# vhost_compare_report

## 目标

这是 `lab-virtio-vhost-kick-notify` 的总报告入口。

## 建议结构

1. 背景：为什么从 tap/bridge 进入 vhost
2. 基础拓扑
3. `vhost=off` 实验
4. `vhost=on` 实验
5. userspace backend vs vhost backend 对照
6. kick/notify/eventfd 模型
7. 当前证据
8. 问题与下一步

## 一句话定位

这个 Lab 把前面的 guest virtio_net + tap/bridge 路径，推进到 host kernel vhost backend 视角。
