# Day18 代码与脚本布局说明

Day18 沿用 Day17 的可运行骨架，但把“第二轮分类裁剪”独立落在 day18/ 自己目录里。

## 目录分层

- `config/`
  - baseline / legacy / classified 相关 fragment
- `collect/`
  - host / guest 采样脚本
- `docs/`
  - 原始需求、技术路线、知识点、验收说明
- `records/`
  - 每轮测试结果
- `output/`
  - perf、savedefconfig 等中间产物

## 重点文件

- `apply_config.sh`
  - profile 选择器 + fragment 叠加器
- `run_profile_collect.sh`
  - 单轮 profile 收口入口
- `run_compare_profiles.sh`
  - 三轮 profile 一键对照
- `compare_results.py`
  - compare csv/md/diff 生成
- `check_profile_equivalence.sh`
  - legacy 与 classified 等价性检查
- `export_category_view.py`
  - 分类总览表生成器
