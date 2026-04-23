# FUNCTION NOTE

## 基本信息
- 函数名：`virtnet_probe`（示范名）
- 所属分组：设备与注册组
- 关注轮次：Round1

## 这段函数负责什么
作为 `virtio_net` 的核心 probe 入口，负责把 virtio 设备逐步接入 netdev 视图。

## 它的上游是谁
- virtio driver 注册后的 probe 触发链

## 它的下游是谁
- netdev 分配/初始化
- queue / napi 初始化
- feature 协商相关流程

## 和自己项目的哪个 stage 最像
- `stage01_netdev_skeleton`
- `stage03_napi_poll`
- `stage09_multi_queue_scaling`

## 这段代码中最容易忽略的点
probe 不只是“把网卡注册出来”，还承担了设备模型、能力协商、队列资源建立等职责。

## 下一步还要追哪些函数
- 与 queue 初始化相关的 helper
- poll / TX / RX 入口函数
