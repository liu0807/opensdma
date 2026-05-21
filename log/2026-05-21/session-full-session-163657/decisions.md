## 关键决策记录

### 1. reviewer 规则架构设计
- **决策**: 不照搬 review_rules 全部 12 个 skill + MCP graph
- **理由**: Ralph 循环是轻量关卡场景，一次只改几个文件，不需要文件级循环和外部脚本
- **提取的核心模式**: 语义激活（Phase 1→2）、4 点证据链（Phase 3）、独立挑战验证（agent）
- **不适用的**: 内核态规则（K1-K8）留作条件激活，当前项目为用户态驱动

### 2. 用户态 / 内核态分离
- **结构**: `references/user-mode-rules.md` + `references/kernel-mode-rules.md`
- **内核态激活策略**: 仅在代码变更中检测到 copy_from_user/kmalloc/spin_lock 等模式时激活
- **粒度**: 用户态 11 维度（D1-D11），内核态 8 维度（K1-K8）
- **SDMA 特有**: 每个用户态维度包含 SDMA API 代码示例和配对规则

### 3. worklog 目录结构
- **顶层**: `log/YYYY-MM-DD/`（按日期分）
- **内层**: `session-<feature-tag>-<HHMMSS>/`（按功能标签 + 时间戳确保唯一）
- **索引**: `log/log-index.json` 集中管理所有会话
- **内容**: summary.md / context.json / changes.diff / timeline.json / decisions.md / artifacts/

### 4. 数据完整性原则
- **不伪造**: 所有检查项必须有可追溯的代码模式匹配
- **证据链**: 每个发现必须包含存在性/影响性/触发条件/保护排除 4 点
- **挑战验证**: 独立 agent 逐条验证每条发现的真实性，减少误报
