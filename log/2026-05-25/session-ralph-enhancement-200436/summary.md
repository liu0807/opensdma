# 工作记录: ralph-enhancement

**日期**: 2026-05-25 20:04:36
**分支**: test-ralph

## 做什么

改进 ralph 技能，要求每一步成功后进行 git 提交。核心主旨是：为项目提供版本回滚能力，为下一轮迭代提供明确的变更差分（Diff），让智能体能够客观地评估现状。

## 改了什么

- `.opencode/skills/ralph/skill.md`:
  - 新增核心原则 #5：每步成功即提交
  - 主循环流程：子 agent 完成后验证提交 + 获取 diff；修复 agent 修复后也提交
  - Ralph Agent 任务指令：新增步骤 4（提交代码变更）、新增 git diff 上下文获取方法
  - 新增"Git 差分驱动上下文"小节：说明为什么用 diff 而非重新读文件
  - 更新流程图：加入 git commit 节点
  - 更新差异对照表：代码提交列改为"每步成功即提交"

## 关键决策

- Ralph 子 agent 实现故事后立即 `git commit`，格式 `US-XXX: <标题> (迭代 N)`
- 修复 agent 修复审查问题后也立即 `git commit`，格式 `US-XXX: fix review issues (迭代 X)`
- 下一轮迭代通过 `git diff HEAD~1..HEAD` 获取精确变更差分，而非重新读取文件
