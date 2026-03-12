# 学习进度

## 当前阶段

W1：字符设备驱动基础闭环

## 已完成

- Day01：miscdevice 驱动骨架与生命周期
- Day02：ioctl SET/GET 与用户态测试
- Day03：sysfs 属性接口
- Day04：debugfs 状态导出与日志级别控制
- Day05：waitqueue / workqueue / 上下文
- Day06：回归脚本与压力测试，已完成 500 次装卸与 300 秒并发压测验收
- Day07：README 收敛、环境说明补齐、build.sh 路径整理、W1 总结

## 当前主要收口内容

- 统一仓库目录形态为 `driver-lab/kernel-src + linux-driver-lab`
- 为 GitHub 保留 `kernel-src` 目录骨架与环境安装文档
- build.sh 默认优先支持相对路径，兼容旧绝对路径

## 下一步

- W2：platform + DT + IRQ + regmap

## 后续补充

- Day08：platform_driver + probe/remove + devm 资源管理
- Day09：Device Tree 匹配、reg/irq 解析
- Day10：request_irq + top-half + /proc/interrupts 验证
- Day11：top-half + workqueue bottom-half + 粗略延迟统计
- Day12：regmap 封装寄存器 + debugfs 快照 + regmap 读写路径验证
