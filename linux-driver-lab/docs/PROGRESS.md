# 学习进度

## 当前阶段

W2：platform + DT + IRQ + regmap

## 已完成

- Day01：miscdevice 驱动骨架与生命周期
- Day02：ioctl SET/GET 与用户态测试
- Day03：sysfs 属性接口
- Day04：debugfs 状态导出与日志级别控制
- Day05：waitqueue / workqueue / 上下文
- Day06：回归脚本与压力测试，已完成 500 次装卸与 300 秒并发压测验收
- Day07：README 收敛、环境说明补齐、build.sh 路径整理、W1 总结
- Day08：platform_driver + probe/remove + devm 资源管理

## 当前主要推进内容

- 启动 W2 第一课，先把 platform 总线模型跑通
- probe 中打印 MEM / IRQ / platform_data 资源
- remove 路径验证 devm 自动清理顺序

## 下一步

- Day09：Device Tree + of_match_table
- Day10：IRQ + regmap
