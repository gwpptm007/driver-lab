# 06_ARM64_PORTING_NOTES

## 工具链相关

ARM64 常见需要：
- `aarch64-linux-gnu-gcc`
- 对应内核 build tree
- ARM64 `Image`
- ARM64 rootfs
- `qemu-system-aarch64`

## 常见问题

### 1. 只有源码树，没有 build tree
这时无法直接外部模块编译，需要：
- 先完成内核编译
- 或至少准备好 `make modules_prepare`

### 2. 有 Image，没有 rootfs
这时只能做 dry-run / 命令生成，无法完成真正 QEMU 运行。

### 3. 有 toolchain，但路径写死在脚本里
这正是 stage06 要解决的问题之一：
- 统一从 env / resolve 输出读取
- 不把路径写死在脚本正文里

## 推荐最小收口方式

1. 解析出 `resolved_arm64_qemu-arm64.env`
2. 用它成功完成 `build-stage04-arm64`
3. 再生成 `arm64_qemu_dryrun.sh`
4. 在真实测试机执行并写入 `records/`
