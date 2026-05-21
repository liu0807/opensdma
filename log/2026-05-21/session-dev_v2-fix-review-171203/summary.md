# 工作记录: dev_v2-fix-review

**日期**: 2026-05-21
**分支**: test-ralph

## 做什么

对 dev_v2/src/case9_mixed_scenario.c 进行代码修复和 reviewer 审查循环。

## 改了什么

- `dev_v2/src/case9_mixed_scenario.c` — 8 项修复:
  1. 线程函数 `static int ret` → `int ret`（消除线程间共享）
  2. 错误路径添加 `pthread_join`（防止线程泄漏）
  3. fd2/fd4 打开失败时关闭已打开的 fd1/fd3（FD 泄漏修复）
  4. munmap 前检查 MAP_FAILED（增强健壮性）
  5. `ready_status_timeout_judgement` NULL 检测后添加 return（缺陷修复）
  6. 移除 `numa_id < 0` 检查（unsigned 类型无效）
  7. `sprintf` → `snprintf`（缓冲区安全）
  8. finish_flag 二次递减修复（unpin 失败时移除 goto，避免重复递减）
- 修复后 reviewer 审查发现 2 个新问题（线程返回局部地址 + finish_flag 二次递减）并修复
- 重新审查 → PASS

## 关键决策

- 线程返回值改为值传递 `(void *)(intptr_t)ret` 而非地址 `(void *)&ret`，消除栈上变量地址返回的 UB
- unpin 失败时仅打印错误并继续清理剩余 cookie，而非 goto 跳过（避免重复递减和数据不一致）
- 以上修复经 reviewer 独立审查确认通过后推送
