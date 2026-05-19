---
name: sdma-tool-compile
description: "将 Ralph 循环开发的 sdma_tool 代码交叉编译为二进制。当用户说编译 sdma_tool、构建 sdma 二进制、编译代码生成可执行文件、构建二进制时务必使用。也用于 Ralph 循环结束后提示用户是否需要编译。注意：仅用于编译构建场景，不用于代码开发或代码审查。"
user-invocable: true
---

# sdma-tool-compile Skill

将项目 `src/` 目录下的 sdma_tool 源码通过 WSL FedoraLinux-44 交叉编译环境编译为二进制，输出到项目 `bin/` 目录。

## 前置条件

- WSL 发行版 `FedoraLinux-44` 已安装且能正常启动
- `/home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/build` 目录已存在且包含有效 Makefile（已通过 `cmake ..` 配置过）
- 交叉编译工具链已就绪（位于 WSL FedoraLinux-44 内）
- 项目根目录存在 `src/` 目录（待编译的 sdma_tool 源码）

## 工作流程

### 第1步：确认编译目标

让用户确认要编译哪个版本目录下的代码。优先从当前工作目录推断（如 `dev_v2`），若无则询问用户。

> 需要用户同意的步骤：**是**——询问版本目录

### 第2步：拷贝源码到 WSL 编译环境

将项目 `src/` 目录下的所有 `.c` 和 `.h` 文件，通过 WSL 的 `/mnt/d/` 路径映射，拷贝到 WSL 内编译环境的 tool 目录：

```
wsl -d FedoraLinux-44 -- cp /mnt/d/Opencode/test_ralph/<version>/src/*.c /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/tool/
wsl -d FedoraLinux-44 -- cp /mnt/d/Opencode/test_ralph/<version>/src/*.h /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/tool/
```

- 只拷贝 `src/` 目录下的 `.c` 和 `.h` 文件
- 拷贝前先清空 tool 目录中的旧文件：
  ```
  wsl -d FedoraLinux-44 -- rm -f /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/tool/*.c
  wsl -d FedoraLinux-44 -- rm -f /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/tool/*.h
  ```

> 此步骤自动执行，无需用户确认

### 第3步：编译

在 WSL 中执行编译：

```
wsl -d FedoraLinux-44 -- make -C /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/build
```

- 编译命令自动执行，无需用户确认
- 捕获编译输出（stdout + stderr）

> 此步骤自动执行，无需用户确认

### 第4步：处理编译结果

#### 编译成功

将生成的 `sdma_tool` 二进制拷贝回项目 `bin/` 目录：

1. 确保项目 `bin/` 目录存在（不存在则创建）
2. 查找二进制（实际路径因 cmake 子目录结构可能不同，通常在 `build/tool/sdma_tool` 或 `build/sdma_tool`）：
   ```
   wsl -d FedoraLinux-44 -- find /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/build -name sdma_tool -type f
   ```
3. 拷贝二进制（以 find 返回的实际路径为准）：
   ```
   wsl -d FedoraLinux-44 -- cp <实际路径>/sdma_tool /mnt/d/Opencode/test_ralph/<version>/bin/sdma_tool
   ```
   wsl -d FedoraLinux-44 -- cp /home/lyx/rpmbuild/BUILD/sdma-dk-1.0.0/build/tool/sdma_tool /mnt/d/Opencode/test_ralph/<version>/bin/sdma_tool
   ```

> 二进制拷贝自动执行，无需用户确认

向用户报告编译成功，包含：
- 版本目录
- 二进制路径
- 文件大小（通过 `wsl ls -l` 获取）

#### 编译失败

1. 向用户展示编译错误输出（关键错误行，非完整输出）
2. 询问用户是否需要自动修复编译错误
3. 如果用户同意：
   - 分析错误原因（缺少头文件、语法错误、链接错误等）
   - 定位到对应的源码文件（通过 WSL 路径映射回 Windows 本地路径）
   - 修改本地源码文件
   - 重复第2步到第4步，直到编译成功或用户要求停止
4. 如果用户拒绝：记录编译失败状态，结束流程

> 修改源码需要先向用户说明修改方案，获得同意后再执行

### 第5步：交叉编译环境维护

如果编译失败的原因是交叉编译环境本身的问题（如缺少依赖库、工具链不完整、CMake 配置需要更新）：

1. 向用户报告环境问题详情
2. 说明需要如何修改环境
3. **必须获得用户明确同意后才能修改交叉编译环境**
4. 修改后重新执行编译

## 错误处理

### 编译错误分类及处理策略

| 错误类型 | 处理方式 | 是否需要用户同意 |
|----------|---------|----------------|
| 语法错误 | 修改源码 → 重新编译 | 需要（说明修改方案后） |
| 缺少头文件 | 检查 include 路径或补充头文件 | 需要（说明修改方案后） |
| 链接错误（undefined reference） | 修改源码或 Makefile | 需要（说明修改方案后） |
| 交叉编译环境缺失依赖 | 安装依赖 | **必须用户同意** |
| WSL 无法启动 | 提示用户检查 WSL 状态 | 报告用户，由用户处理 |
| 磁盘空间不足 | 提示用户清理空间 | 报告用户，由用户处理 |

### 修复循环

修复源码后，重新执行：
1. 清空 tool 目录旧文件
2. 拷贝新源码
3. 重新 make
4. 检查结果

最多重试 5 次，超过后询问用户是否继续。

## 示例

用户输入: "编译 sdma_tool"

```
您: 编译 sdma_tool
AI: 当前工作目录为 dev_v2，确认编译 dev_v2 的 sdma_tool 代码？(Y/n)
您: Y
AI: [自动] 拷贝 src/*.c/*.h → WSL tool/... ✓
AI: [自动] make -C build/... ✓
AI: 编译成功！
    二进制: /mnt/d/Opencode/test_ralph/dev_v2/bin/sdma_tool
    大小: 128KB
```

用户输入: "编译 sdma_tool"（有编译错误）

```
您: 编译 sdma_tool
AI: 当前工作目录为 dev_v2，确认编译 dev_v2 的 sdma_tool 代码？(Y/n)
您: Y
AI: [自动] 拷贝源码 ✓
AI: [自动] make... ✗ 编译出错
AI: === 错误摘要 ===
    tool/case_us-005.c:42:5: error: expected ';' before 'return'
    ...
AI: 需要我自动修复这个编译错误吗？(Y/n)
您: Y
AI: 分析发现 case_us-005.c:42 缺少分号。
    计划在 case_us-005.c:42 添加 ';'，确认修改？(Y/n)
您: Y
AI: [修改] case_us-005.c:42 ✓
AI: [自动] 重新拷贝源码 → make... ✓
AI: 编译成功！
    二进制: /mnt/d/Opencode/test_ralph/dev_v2/bin/sdma_tool
    大小: 128KB
```

## 与 Ralph 循环的集成

Ralph 技能执行完所有用户故事后，会提示用户是否需要调用此技能编译 sdma_tool。
