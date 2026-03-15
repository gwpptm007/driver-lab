# Day18 FIRST_RUN

Day18 的第一次执行，建议不要一上来就三轮全跑。

推荐顺序：

## 1. 先生成分类总览

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day18
python3 export_category_view.py
```

## 2. 先验证 classified 的 fragment 能不能收敛到 `.config`

```bash
PROFILE=classified BUILD_KERNEL=no ./apply_config.sh
```

重点看：

- `CONFIG_PCI`
- `CONFIG_SCSI`
- `CONFIG_NET`
- `CONFIG_DEBUG_FS`
- `CONFIG_FUNCTION_GRAPH_TRACER`
- `CONFIG_PERF_EVENTS`

## 3. 再跑单轮 classified

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh classified
```

## 4. 再跑 baseline / round2b_legacy / classified 三轮对照

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./run_compare_profiles.sh
```

## 5. 最后看 legacy 与 classified 是否等价

```bash
./check_profile_equivalence.sh
```

## 6. 每一步分别是在验证什么

- `python3 export_category_view.py`
  - 验证分类元数据能否导出成人可读矩阵
- `PROFILE=classified BUILD_KERNEL=no ./apply_config.sh`
  - 验证分类 fragment 能否收敛成合法 `.config`
- `PROFILE=classified BUILD_KERNEL=yes ./apply_config.sh`
  - 验证分类裁剪后的配置能否真正把内核编出来
- `PROFILE=classified ./build.sh`
  - 验证 rootfs、模块、DT、perf 等运行物料是否组装完整
- `PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh classified`
  - 验证 classified 能否完成一次真实启动、采集和 records 归档
- `PERF_REQUIRED=yes PERF_MODE=auto ./run_compare_profiles.sh`
  - 验证 baseline / round2b_legacy / classified 三组对照能否完整跑通
- `./check_profile_equivalence.sh`
  - 验证 legacy 与 classified 是否属于“表达升级但结果近似等价”

更详细的解释见：

- `docs/07_execution_steps_and_validation.md`
- `docs/08_perf_build_analysis.md`
