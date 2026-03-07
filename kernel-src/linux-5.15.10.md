# linux-5.15.10 环境准备

本目录应放置 Linux 5.15.10 源码树。

GitHub 中只保留本说明文件和占位文件，不提交完整源码。请使用者自行下载并解压到当前目录。

---

## 1. 下载源码

```bash
cd driver-lab/kernel-src
wget https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/linux-5.15.10.tar.xz
tar -xf linux-5.15.10.tar.xz
```

如果解压后不是当前目录名，请整理成：

```text
driver-lab/kernel-src/linux-5.15.10/
```

---

## 2. 生成默认配置

```bash
cd driver-lab/kernel-src/linux-5.15.10
make defconfig
```

---

## 3. 检查模块支持

因为实验需要 `insmod` / `rmmod`，建议确认内核配置中启用了可加载模块支持。虽然很多发行版的 `defconfig` 默认已经打开，但在学习阶段建议还是亲自进一次 `menuconfig` 看清楚。

执行：

```bash
make menuconfig
```

确认以下选项：

- `Enable loadable module support` 必须勾选
- `Module unloading` 必须勾选

图形菜单里通常表现为：

```text
[*] Enable loadable module support
    [*] Module unloading
```

说明：

- 第一项决定能否加载 `.ko`
- 第二项决定能否执行 `rmmod`
- 图形菜单里可用方向键移动，按 `Y` 勾选，按 `Enter` 进入子菜单，退出时保存配置

---

## 4. 编译内核

```bash
make -j$(nproc)
```

编译成功后，至少确认下面文件存在：

```text
arch/x86/boot/bzImage
```

这个文件会被 `linux-driver-lab/day01~day06/build.sh` 用来启动 QEMU。

---

## 5. 用途说明

这个内核源码目录主要提供两类东西：

1. `bzImage`
   - 用于 QEMU 启动实验内核
2. 外部模块编译环境
   - `make -C <kernel_dir> M=$(pwd) modules`
   - 用于编译每天的 `demo.ko`

---

## 6. 验证方法

除了检查 `bzImage`，也可以顺手确认 `make modules_prepare` 相关产物已生成。虽然本实验的 `make -C <kernel_dir> M=$(pwd) modules` 通常在完整编译后就够用，但理解“内核镜像”和“外部模块编译环境”是两件事，会更有帮助。

## 7. 验证方法

可以先进入任意一个 day 目录，执行：

```bash
chmod +x build.sh
./build.sh
```

如果能看到脚本识别到：

```text
Using kernel : .../kernel-src/linux-5.15.10
```

并成功编译 `demo.ko`，说明内核源码路径已准备好。
