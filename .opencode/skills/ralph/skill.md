---
name: ralph
description: "自主开发循环，每次迭代使用全新上下文。当用户说开始 Ralph 循环、运行 Ralph、使用 Ralph 开发、启动自主开发循环、开始写代码、启动开发流程、开始实现、执行自动化开发、继续开发、启动开发循环时使用。注意：仅用于执行 prd.json 中的用户故事，不用于生成 PRD 或代码审查。"
user-invocable: true
---

# Ralph Loop Skill for OpenCode

## 概述

Ralph Loop 是一个自主开发循环系统，通过多次迭代完成 PRD 中的所有用户故事。**每次迭代使用全新的上下文**，迭代之间通过状态文件（prd.json、progress.txt、git）传递信息。每轮迭代后由独立的检视 agent 进行代码审查。

## 使用方法

当用户说以下任一内容时，加载此 skill 并开始 Ralph 循环：
- "开始 Ralph 循环"
- "运行 Ralph"
- "使用 Ralph 开发"
- "启动自主开发循环"

## 核心原则

1. **每次迭代 = 全新上下文** - 使用 `task` 工具启动全新子 agent，不保留之前迭代的内存
2. **状态文件是唯一桥梁** - 迭代之间只通过 prd.json、progress.txt、git history 交流
3. **一次一个故事** - 每轮迭代只处理一个用户故事
4. **独立检视** - 每轮完成后由独立检视 agent 审查代码，通过才进入下一轮

## 前置条件检查

在开始循环之前，检查项目根目录是否包含：
1. `prd.json` - 包含用户故事和状态（由 PRD skill 生成）
2. `progress.txt` - 记录学习历史（不存在则自动创建）

### 如果没有 prd.json

提示用户先使用 PRD skill 创建：
```
未找到 prd.json 文件。

请先使用 PRD skill 创建需求文档：
1. 加载 PRD skill（对我说"加载 PRD skill"）
2. 生成 PRD：对我说"创建 PRD：[功能描述]"
3. 转换为 prd.json：对我说"转换为 prd.json"

完成后再次启动 Ralph 循环。
```

### 初始化 progress.txt

如果不存在，自动创建：
```
# Ralph Progress Log
Started: [时间]
---

## Codebase Patterns
（逐步累积可复用模式）

---
```

## 主循环流程

**由主 agent（你）控制循环，每次迭代启动全新子 agent：**

```
迭代计数 = 1
最大迭代次数 = 10（或从用户获取）

WHILE 迭代计数 <= 最大迭代次数:
  1. 读取 prd.json，检查是否所有故事 passes: true
     - 如果是 → 输出 <promise>COMPLETE</promise>，结束
  
  2. 使用 task 工具启动**全新**子 agent (Ralph agent):
     子agent类型: general
     任务描述: "Ralph 迭代 X - 执行单个用户故事"
     任务内容: 见下方"Ralph Agent 任务指令"
     （不指定 task_id，确保全新上下文）
  
  3. 等待 Ralph agent 完成
  
  4. **启动检视流程**
     加载 reviewer skill
     执行检视循环：
       - 启动检视 agent
       - 如果通过 → 继续
       - 如果不通过 → 启动修复 agent → 重新检视（最多 3 次）
       - 如果仍不通过 → 记录问题，强制通过（或跳过该故事）
  
  5. 检查状态:
     - 读取 prd.json 查看故事是否标记为 passes: true
     - 读取 progress.txt 查看进度记录
  
  6. 汇报: "✓ 完成 US-XXX: 标题 (迭代 X) - 检视通过"
  
  7. 迭代计数 += 1
  
END WHILE

如果达到最大迭代次数:
  输出 "达到最大迭代次数，检查 progress.txt 了解状态"
```

## Ralph Agent 任务指令

每次启动 Ralph 子 agent 时，传递以下指令（**子 agent 必须视为全新实例，不依赖任何对话历史**）：

```
你是一个自主编码 agent，正在执行 Ralph 循环的第 N 次迭代。

## 重要：你是全新实例
- 你没有任何之前迭代的记忆
- 只通过以下文件获取上下文：
  - prd.json（任务列表和状态）
  - progress.txt（学习历史，重点关注 Codebase Patterns 部分）
  - git log（之前的提交）

## 你的任务

1. **读取状态文件**
   - 读取 prd.json，找到 passes: false 且优先级最高的故事
   - 读取 progress.txt，重点关注 "## Codebase Patterns" 部分
   - 确认当前 git 分支与 prd.json 中的 branchName 一致

2. **实现故事**
   - 根据故事的 description 和 acceptanceCriteria 实现功能
   - 只实现这一个故事，不要做其他事
   - 遵循现有代码模式
   - 如有浏览器验证要求，使用可用工具验证

3. **更新状态文件**
   - 更新 prd.json：将该故事的 passes 设为 true
   - 追加进度到 progress.txt（格式见下）

4. **更新 AGENTS.md（可选）**
   - 如果发现可复用模式，更新相关目录的 AGENTS.md
   - 只添加通用知识，不是故事特定细节

5. **检查停止条件**
   - 检查 prd.json 中所有故事的 passes 字段
   - 如果全部为 true，在你的回复中输出：<promise>COMPLETE</promise>
   - 否则，正常结束即可

## 进度记录格式（追加到 progress.txt）

```
## [时间] - [故事ID]
- 实现内容：[简述]
- 修改文件：[列表]
- **学习点：**
  - 发现的模式：[如有]
  - 注意事项：[如有]
---

```

## 更新 Codebase Patterns（progress.txt 顶部）

如果发现可复用模式，更新 progress.txt 顶部的 "## Codebase Patterns" 部分：
```
## Codebase Patterns
- 模式1：描述
- 模式2：描述
```

## 重要提醒
- 一次只做一个故事
- 保持改动最小化
- 你完成后会被关闭，下一个迭代会启动新的 agent
```

## 进度汇报格式

每完成一个故事后（包括检视通过），向用户简要汇报：
```
✓ 完成 US-001: 添加优先级字段到数据库 (迭代 1)
  状态: 已实现，检视通过
  剩余: 3 个故事待完成
```

## 停止条件

以下任一情况停止循环：
1. prd.json 中所有故事 passes: true → 输出 `<promise>COMPLETE</promise>`
2. 达到最大迭代次数 → 提示用户检查进度
3. 用户主动要求停止

## 边界情况处理

- **没有未完成任务**：输出完成信息
- **检视连续失败**：记录问题到 progress.txt，跳过该故事
- **分支不存在**：从 main/master 创建
- **prd.json 格式错误**：提示用户并停止
- **子 agent 异常**：捕获输出，记录到 progress.txt，继续下一迭代

## 工作流程图

```
[Ralph 迭代 N] 
    ↓
[实现故事] → [更新 prd.json] → [记录 progress.txt]
    ↓
[检视 Agent] → 通过？ → 是 → [下一轮迭代 N+1]
    ↓ 否
[修复 Agent] → [重新检视] → 通过？ → 是 → [下一轮迭代 N+1]
    ↓ 否（3次后）
[记录问题] → [强制通过/跳过] → [下一轮迭代 N+1]
```

## 与原版 Ralph 的差异

| 特性 | 原版 Ralph | 本实现 |
|------|-----------|--------|
| 每次迭代全新上下文 | ✓ bash 循环启动新进程 | ✓ task 工具启动新子 agent |
| 状态文件传递 | ✓ prd.json/progress.txt/git | ✓ 相同 |
| 单次迭代执行 | ✓ 一个故事 | ✓ 一个故事 |
| 代码提交 | ✓ git commit | ✗ 不涉及 |
| 质量检查 | ✓ agent 内部检查 | ✓ 独立检视 agent |
| 检视时机 | 提交前 | 迭代后 |
| 修复流程 | 无 | ✓ 检视 → 修复 → 重新检视 |
| 知识沉淀 | ✓ progress.txt/AGENTS.md | ✓ 相同 |

## 编译集成

所有用户故事完成后，主动询问用户："所有用户故事已完成。是否需要编译 sdma_tool 为二进制？" 如果用户确认，调用 sdma-tool-compile 技能进行编译。

## Git 上传集成

编译完成后，主动询问用户："是否将当前代码提交并推送到远端 git 仓库？" 如果用户确认，调用 git-push 技能进行提交推送。
