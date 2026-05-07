# app - media-gateway-lite

## 构建

```bash
make
# 或
meson setup build --wipe && ninja -C build
```

生成：

```text
app/build/media-gateway-lite
```

## 示例：vdev 双端口

```bash
sudo ./build/media-gateway-lite \
  -l 0-1 -n 4 --no-pci \
  --vdev=net_null0 --vdev=net_null1 \
  -- \
  --run-seconds 10 \
  --rule0 0:1 --rule1 1:0 \
  --udp-only 1
```

## 示例：带 rewrite 规则

```bash
sudo ./build/media-gateway-lite \
  -l 0-1 -n 4 --no-pci \
  --vdev=net_null0 --vdev=net_null1 \
  -- \
  --rule0 0:1 \
  --rule0-dst-port 9000 \
  --rule0-rewrite-dst-ip 10.10.10.20 \
  --rule0-rewrite-dst-port 10000
```
