# sdma-dk 自动化开发与质量保障系统

## 项目概述

这是一个为华为鲲鹏 SDMA 用户态驱动开发的自动化开发与质量保障系统，核心解决的是嵌入式驱动开发中长链推理任务容易因上下文碎片化导致逻辑断裂、人工审查遗漏缺陷、多轮迭代缺乏记录回溯的痛点。系统基于 OpenCode 插件平台构建了 7 个自定义技能组成的多 Agent 协作流水线：prd 技能先将模糊需求转化为结构化用户故事和验收标准，ralph 技能随后启动自主开发循环，每次迭代使用全新上下文启动子 Agent 实现单个故事，完成后自动触发 reviewer 技能进行语义驱动式代码审查——reviewer 先扫描代码特征动态激活 user-mode-rules.md（11 维度 70+ 项）或 kernel-mode-rules.md（8 维度）中的匹配规则，再由独立挑战 Agent 逐条验证证据链真实性，审查不通过时启动修复 Agent 重试最多 3 次，形成"需求分解→自主实现→语义审查→挑战验证→闭环修复"的长链推理协作流程，最后通过 sdma-tool-compile 交叉编译为 aarch64 二进制、git-push 推送远端、worklog 归档完整快照，使每个开发决策和代码变更全程可追溯。

## 核心痛点

嵌入式驱动开发中，长链推理任务（如跨多文件理解硬件接口规范、推演多进程多线程并发路径、验证 SDMA 通道生命周期与数据拷贝的交互正确性）容易因大模型上下文的碎片化导致逻辑断裂。同时，手动审查缺陷遗漏率高、多轮迭代缺乏可回溯的工作记录，导致开发效率和质量保障难以兼顾。

## 核心逻辑流

系统采用多 Agent 协作的长链推理流水线：

1. **prd 技能** — 将模糊需求转化为结构化 PRD，包含用户故事（US-001~US-004）和验收标准
2. **ralph 技能** — 启动自主开发循环，每轮迭代使用全新上下文启动子 Agent，聚焦单个故事的实现
3. **reviewer 技能** — 语义驱动的代码审查关卡，先扫描代码变更特征动态激活 user-mode-rules.md（11 维度 70+ 检查项）或 kernel-mode-rules.md（8 维度，条件激活）中的匹配规则，构建 4 点证据链（存在性/影响性/触发条件/保护排除），然后由独立挑战 Agent 逐条验证每条发现的真实性，审查不通过时启动修复 Agent 重试（最多 3 次）
4. **sdma-tool-compile 技能** — 将开发完成的 sdma_tool 交叉编译为 aarch64 二进制，处理 GLIBC 2.34 兼容问题
5. **git-push 技能** — 经用户授权后将代码提交推送到远端仓库
6. **worklog 技能** — 在阶段性任务完成后自动询问用户是否保存完整上下文快照到 log/ 目录

## 构建成果

### 自定义技能（7 个）

| 技能 | 功能 | 触发词 |
|------|------|--------|
| prd | 生成结构化 PRD 并转为 prd.json | 生成 PRD、写需求 |
| ralph | 自主开发循环，每次迭代全新上下文 | 开始 Ralph、开始写代码 |
| reviewer | 语义驱动代码审查 + 挑战验证 + 修复闭环 | 审查代码、review 代码 |
| sdma-tool-compile | sdma_tool 交叉编译为 aarch64 二进制 | 编译 sdma_tool |
| git-push | 代码提交推送工作流 | 提交代码、上传代码 |
| skill-creator | 创建/修改/改进自定义技能 | 创建技能、写技能 |
| worklog | 工作状态快照到 log/ 目录 | 保存记录、存档 |

### 代码开发

- **dev_v2/** — 重构版本的混合场景测试用例 `case9_mixed_scenario.c`，支持：
  - fork 多进程（send/recv 双进程架构）
  - pthread 多线程（线程一一对应，分别绑定 CPU 核）
  - stride 步长模式（通过 src_stride/dst_stride/stride_num 参数控制）
  - 独占/共享通道类型控制（sdma_alloc_chn + copy_data 或 sdma_init_chn + icopy_data）
  - NUMA 节点绑定和 CPU 亲和性设置
- 累计 **19 次 commit**，包含功能开发、8 项缺陷修复、GLIBC 兼容适配、编译优化

### 审查体系

- `reviewer-workspace/` — 4 轮迭代评估：synthetic 测试用例 → dev_v2 真实代码 → 3 个 gap 修复 → 第 4 轮优化验证
- `references/user-mode-rules.md` — 11 维度（D1-D11），70+ 检查项，含 SDMA 特有模式和代码示例
- `references/kernel-mode-rules.md` — 8 内核维度（K1-K8），条件激活（检测到内核代码模式时触发）

### 工作记录归档

- `log/log-index.json` — 会话索引，支持快速查找
- `log/2026-05-21/session-full-session-163657/` — reviewer 增强 + worklog 创建的全量快照
- `log/2026-05-21/session-dev_v2-fix-review-171203/` — dev_v2 修复审查循环的快照

## 质量保障机制

- 每轮代码变更必须经过 reviewer 审查才能进入下一轮
- 每条审查发现必须包含 4 点证据链（存在性/影响性/触发条件/保护排除）
- 独立挑战 Agent 验证每条发现的真实性，减少误报
- 审查不通过时自动启动修复 Agent，最多重试 3 次
- 工作记录归档确保开发决策和代码变更全程可追溯
