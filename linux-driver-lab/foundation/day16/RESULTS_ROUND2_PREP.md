# Day16 RESULTS_ROUND2_PREP

## 1. round2 候选确认结果

### 已明确关闭

- 大部分 DRM 平台驱动
- `SOUNDWIRE`
- `I2C_GPIO`
- `USB_CHIPIDEA`

### 依赖坍塌正常

- `USB_CHIPIDEA_TEGRA` 不再出现

### 仍需进一步确认

- `DRM_DW_HDMI=m`
- `I2C_ALGOBIT=m`

### 主链仍完好

- tracing / perf / debug 保留项仍在

---

## 2. 根因分析

### `I2C_ALGOBIT` 仍为 `=m`

已确认：

```text
drivers/gpu/drm/Kconfig:15: select I2C_ALGOBIT
```

也就是说，只要 `CONFIG_DRM` 还活着，`I2C_ALGOBIT` 就可能继续被拉起。

### `DRM_DW_HDMI` 仍为 `=m`

已确认上游来源包括：

- `rcar-du`
- `sun4i`
- `meson`
- `rockchip`
- `imx`

结合当前 `.config`，主要残余来源进一步锁定到：

- `DRM` 顶层
- `DRM_SUN4I`

因此 round2 升级为 round2b。
