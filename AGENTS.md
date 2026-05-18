# Agents

## 仓库概述
OpenCode 插件工作区，用于开发测试三个自定义技能：`prd`、`ralph`、`reviewer`。同时包含 sdma-dk（华为鲲鹏 SDMA 用户态驱动）源码，用于 Ralph 循环测试。

## 自定义技能
位于 `.opencode/skills/` 目录下，每个技能由特定短语触发（详见各目录下的 `skill.md`）：
- **prd** - 生成 PRD 并转换为 `prd.json`
- **ralph** - 每次迭代使用全新上下文的自主开发循环
- **reviewer** - Ralph 循环每轮迭代后的代码审查关卡
- **skill-creator** - 创建新技能、修改和改进现有技能。触发词：创建技能、写技能、生成技能

## 关键目录
- `.opencode/` - 插件配置（`package.json` 依赖 `@opencode-ai/plugin`）和技能文件
- `lib/` - sdma-dk 用户态接口源码：`mdk_sdma.c/h`（接口实现）、`hisi_sdma.h`（硬件接口定义）。开发过程中遇到 sdma 相关接口可在此目录查询接口定义，了解接口功能
- `src/` - sdma tool 工具源码，其作用是对 sdma 用户态接口进行测试：`case*.c`、`ut_sdma.*`

## sdma-dk 说明
- 目标平台：鲲鹏 aarch64（非该平台需交叉编译）
- 原始构建系统：CMake（参见源码包中的 `sdma-dk-1.0.0/CMakeLists.txt`）
- 补丁文件位于 `sdma-dk-openEuler-22.03-LTS-SP4/` 目录（若存在）

## Ralph 循环前置条件
- 项目根目录必须存在 `prd.json` 和 `progress.txt`
- 使用 `task` 工具为每轮迭代启动全新子代理
- Reviewer 技能会在进入下一轮前验证每轮迭代结果

## 重要约束
- **当用户提出新需求时，必须首先调用 prd 技能**生成 PRD，再进行其他操作
- 用户描述新功能需求时，禁止直接启动 Ralph 循环
- 用户说“帮我实现XXX”或“添加XXX功能”时，应先调用 prd 技能
- 当前项目仅做代码相关开发，无需进行任何构建（如 生成任何cmake、makefile文件或执行相关指令）或代码上传（如 git）操作
- **工作目录限制在 `D:\Opencode\test_ralph`，禁止访问任何其他文件夹**
- 当需求描述中没有详细说明的地方，必须按照 prd skill 的要求向用户追问澄清问题，禁止自行构造需求细节或假设参数
- 在用户没有明确指示“开始写代码”或“启动 Ralph 循环”之前，禁止执行 Ralph 循环或开始编写代码
- **定位问题时，定位清楚之后，列出修改方案并详细描述如何修改及为什么要这样修改，征询用户同意后才能修改代码**
- **不伪造任何内容**：未读过的文件不可断言其内容；未执行过的命令不可伪造其输出；未验证过的事实不可作为结论；所有引用必须可追溯

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.