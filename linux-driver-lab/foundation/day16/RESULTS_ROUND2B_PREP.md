# Day16 RESULTS_ROUND2B_PREP

## 1. round2b 触发原因

在 round2 之后，仍然存在：

- `CONFIG_DRM_DW_HDMI=m`
- `CONFIG_I2C_ALGOBIT=m`

进一步排查确认：

- `CONFIG_DRM=m`
- `CONFIG_DRM_KMS_HELPER=m`
- `CONFIG_DRM_FBDEV_EMULATION=y`
- `CONFIG_DRM_SUN4I=m`
- `CONFIG_DRM_BRIDGE=y`

因此可以判断：

- `DRM_DW_HDMI` 仍然被 `DRM_SUN4I` 这一残余显示链拉着
- `I2C_ALGOBIT` 仍然被 `CONFIG_DRM` 顶层拉着

---

## 2. round2b 当前配置检查结果

### 已确认

```text
# CONFIG_DRM is not set
```

并且在配置检查中，以下项不再出现：

- `CONFIG_DRM_KMS_HELPER`
- `CONFIG_DRM_FBDEV_EMULATION`
- `CONFIG_DRM_BRIDGE`
- `CONFIG_DRM_SUN4I`
- `CONFIG_DRM_DW_HDMI`
- `CONFIG_I2C_ALGOBIT`

这说明：

> `CONFIG_DRM` 被关掉后，整条显示栈和它拉起的依赖一起坍塌了。

---

## 3. round2b 编译前主链检查结果

以下项仍然保留：

- `CONFIG_KALLSYMS=y`
- `CONFIG_KALLSYMS_ALL=y`
- `CONFIG_PERF_EVENTS=y`
- `CONFIG_HW_PERF_EVENTS=y`
- `CONFIG_DEBUG_FS=y`
- `CONFIG_TRACEPOINTS=y`
- `CONFIG_TRACING=y`
- `CONFIG_FTRACE=y`
- `CONFIG_FUNCTION_TRACER=y`
- `CONFIG_FUNCTION_GRAPH_TRACER=y`
- `CONFIG_DYNAMIC_FTRACE=y`

结论：

> Day15/Day16 所需的 tracing / function_graph / perf 内核侧主链未被 round2b 打坏。

---

## 4. 当前状态

round2b 在配置层已经通过。

当前下一步：

1. 在宿主机重新编译内核
2. 同步新的 `Image`
3. 回到 `day15/` 执行 `./build.sh`
4. 在 guest 内执行 `day15_guest_collect.sh` 做手工验证
5. 再运行宿主机 `host_collect.sh` 做自动采集
