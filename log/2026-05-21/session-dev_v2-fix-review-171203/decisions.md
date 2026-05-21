## 关键决策记录

### 1. 线程返回值机制变更

- **问题**: `sdma_mixed_thread` 返回 `(void *)&ret`，当 `static` 改为局部变量后成为栈地址 UB
- **决策**: 改为值传递 `(void *)(intptr_t)ret`，调用方用 `(int)(intptr_t)pthread_ret[i]` 解码
- **理由**: 消除线程栈内存引用 UB，同时保持返回值语义
- **影响**: 影响 sdma_mixed_thread + mixed_recv + mixed_send 三处代码

### 2. finish_flag 二次递减修复

- **问题**: 正常路径 unpin 失败时 goto release_mem_recv 导致 finish_flag 二次递减，对端进程误判本进程状态
- **决策**: unpin 失败时仅打印错误，继续清理剩余 cookie，不 goto
- **理由**: 数据已传输完成，unpin 失败是系统级清理问题，不应影响跨进程同步状态
- **影响**: 移除 mixed_recv 和 mixed_send 正常路径 unpin 循环中的 goto

### 3. 修复流程

- 遵循"定位问题 → 列出方案 → 征询同意 → 修改"四步流程
- reviewer 审查发现 7 个问题 → 用户确认修复 2 个关键问题 → 复查 PASS
- 预存问题（thread 核号下界检查、request 零值初始化、死代码等）留待后续处理
