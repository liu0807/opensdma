---
name: worklog
description: "保存当前工作状态快照到 log/ 目录，防止工作记录丢失。当用户说保存记录、记录进度、记录工作、存档、checkpoint、快照、记录当前状态、保存现场、做记录、写日志时务必使用。也用于在阶段性任务（Ralph 循环、代码审查、Git 推送）完成后主动询问用户是否需要保存。"
user-invocable: true
---

# Worklog Skill — 工作记录保存

## 概述

此 skill 将当前工作状态完整快照到 `log/` 目录，按日期 + 功能标签组织，防止因会话中断、上下文丢失、token 压缩导致的工作记录丢失。

### 保存到 log 目录的内容

每次触发会创建独立会话目录，保存以下内容：

| 文件 | 内容 | 来源 |
|------|------|------|
| `summary.md` | 会话摘要：做什么、改了什么、关键决策 | 用户提供 + 自动生成 |
| `context.json` | prd.json + progress.txt 的当前内容 | 读取项目文件 |
| `changes.diff` | git diff（HEAD 对比工作区） | git diff |
| `timeline.json` | 最近 git log（--oneline -10） | git log |
| `decisions.md` | 关键决策记录 | 用户提供 |
| `artifacts/` | 评估报告、编译日志、审查报告等 | 复制相关文件 |

### 目录结构

```
log/
├── log-index.json              # 全部会话索引（快速查找）
├── 2026-05-21/
│   ├── session-prd-enhancement-143022/   # 日期 + 功能 + 时间
│   │   ├── summary.md
│   │   ├── context.json
│   │   ├── changes.diff
│   │   ├── timeline.json
│   │   ├── decisions.md
│   │   └── artifacts/
│   └── session-bug-fix-151200/
│       └── ...
├── 2026-05-22/
│   └── ...
```

## 工作流程

### 第1步：获取用户输入

询问用户：
1. 这个会话的功能标签是什么？（如 "prd-enhancement"、"bug-fix" — 用于目录命名）
2. 有什么关键决策需要记录？如果用户不提供，留空即可

### 第2步：捕获当前状态

读取以下文件并记录内容：

- **prd.json**（如果存在）：当前用户故事列表和状态
- **progress.txt**（如果存在）：当前实现进度
- **AGENTS.md**：项目级上下文信息

执行以下命令捕获变更：
- `git branch`：当前分支
- `git log --oneline -10`：最近提交历史
- `git diff`：未暂存的变更
- `git diff --cached`：已暂存的变更
- `git status`：完整工作区状态

### 第3步：收集 Artifacts（可选）

查找以下目录中最近生成的文件并复制到 `artifacts/`（仅复制文本文件，不超过 200KB）：
- `reviewer-workspace/` 中最新的 eval 报告（report.md）
- `.opencode/skills/` 中被修改的技能文件（检查 git diff）
- `dev_v*/` 中有无新的/修改的文件
- 根目录下任何 `*.log` 文件

### 第4步：创建会话目录

目录路径格式：`log/<YYYY-MM-DD>/session-<feature-tag>-<HHMMSS>/`

```
示例:
log/2026-05-21/session-reviewer-enhancement-143022/
```

规则：
- 日期部分使用当前系统日期（`Get-Date -Format 'yyyy-MM-dd'`）
- feature-tag 来自用户输入，字母数字+连字符，最长 30 字符
- 时间戳使用当前系统时间（`Get-Date -Format 'HHmmss'`）
- 如果用户未提供 feature-tag，使用自动检测的上下文名称

### 第5步：保存文件

写入以下文件到会话目录：

#### summary.md
```
# 工作记录: <feature-tag>

**日期**: <YYYY-MM-DD HH:MM:SS>
**分支**: <branch-name>

## 做什么
<用户描述或自动从 git log 推断>

## 改了什么
<关键文件变更列表>

## 关键决策
<用户提供的决策记录，无则留空>
```

#### context.json
JSON 格式，包含 prd.json 和 progress.txt 的完整内容（如果存在）。

#### changes.diff
`git diff` 和 `git diff --cached` 的合并输出。

#### timeline.json
```json
{
  "branch": "test-ralph",
  "saved_at": "2026-05-21T14:30:22",
  "recent_commits": [
    "abc1234 commit message",
    ...
  ]
}
```

#### decisions.md
如果用户提供了关键决策，以 Markdown 列表保存。未提供则留空或写入 "*本次未记录关键决策*"。

#### artifacts/ 目录
复制相关报告文件到此目录。

### 第6步：更新日志索引

读取或创建 `log/log-index.json`，追加新会话条目：

```json
{
  "sessions": [
    {
      "date": "2026-05-21",
      "time": "14:30:22",
      "tag": "reviewer-enhancement",
      "path": "log/2026-05-21/session-reviewer-enhancement-143022",
      "summary": "增强 reviewer 技能",
      "branch": "test-ralph"
    }
  ]
}
```

### 第7步：汇报结果

```
✓ 工作记录已保存
  路径: log/<YYYY-MM-DD>/session-<tag>-<time>/
  内容: summary.md / context.json / changes.diff / decisions.md / artifacts/
```

## 集成到其他技能

### Ralph 循环集成

Ralph 循环每次迭代完成（包括检视通过或失败）后，提示用户：

```
迭代完成，是否需要保存工作记录快照到 log/ 目录？
```

### Reviewer 集成

Review 通过后，提示用户：

```
检视通过，是否需要保存当前状态到 log/ 目录？
```

### Git-push 集成

Git 推送成功后，提示用户：

```
代码已推送，是否需要保存工作记录到 log/ 目录？
```

## 边界情况处理

| 场景 | 处理方式 |
|------|---------|
| 用户拒绝提供 feature-tag | 使用自动检测：检查 git diff 涉及的文件路径推断功能名 |
| git diff 为空（无变更） | 只保存 context.json + timeline.json，changes.diff 标注 "无变更" |
| prd.json 不存在 | context.json 只包含 progress.txt 内容 |
| 日志目录已存在同名 session | 时间戳 HHMMSS 确保唯一性，不会冲突 |
| 文件读取失败 | 跳过该文件，在 summary.md 中注明 "读取失败" |
| log-index.json 损坏 | 重新创建索引，只包含当前会话 |

## 使用示例

```
用户: "保存一下当前进度"
→ 询问功能标签和决策
→ 捕获上下文
→ 保存到 log/2026-05-21/session-feature-143022/
→ 更新索引
→ 汇报结果
```
