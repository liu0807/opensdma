# sdma_tool

sdma-dk 用户态测试工具，aarch64 交叉编译二进制。

## 系统要求

| 依赖 | 说明 | 获取方式 |
|------|------|---------|
| **glibc >= 2.34** | GNU C 库，部分静态链接（移除 GLIBC_2.38 依赖） | 系统自带 |
| **libnuma.so.1** | NUMA 库 | `dnf install numactl` 或 `apt install libnuma1` |
| **libsdma_dk.so** | sdma 用户态驱动库 | 编译产物，需与 sdma_tool 同目录或加入 LD_LIBRARY_PATH |
| **ld-linux-aarch64.so.1** | aarch64 动态链接器 | glibc 自带 |
| **hisi_sdma 内核驱动** | 鲲鹏 SDMA 硬件驱动 | 内核模块，需已加载 |

## 使用说明

将 `sdma_tool` 和 `libsdma_dk.so` 放在同一目录，执行：

```bash
./sdma_tool -h
```

## 构建信息

- 架构: ARM aarch64
- 链接: 动态链接（glibc 部分静态链接）
- 构建系统: WSL FedoraLinux-44 交叉编译
- 工具链: `/usr/sbin/aarch64-linux-gnu-gcc`
