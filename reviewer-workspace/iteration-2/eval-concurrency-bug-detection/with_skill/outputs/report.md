# 代码检视报告

**文件：** `reviewer-workspace/fixtures/review-sample/review-me.c`

---

## Phase 1: 特征扫描

| 维度 | 发现 |
|------|------|
| 语言 | C11 |
| 并发模型 | pthread (pthread_mutex_t) |
| 数据结构 | SharedBuffer（int* buffer + size + mutex） |
| 函数数 | 4（create_buffer, write_value, process_all, main） |
| 风险特征 | malloc 堆操作、pthread 锁、指针解引用、循环边界访问 |

---

## Phase 2: 风险激活

检测到以下特征，激活对应检查清单：

- ✅ 多线程共享状态 + 互斥锁 → **并发/数据竞争检查** 激活
- ✅ malloc 堆分配 + 指针操作 → **内存安全/空指针检查** 激活
- ✅ 循环数组访问 → **缓冲区溢出/越界检查** 激活
- ✅ 无对应 free/destroy → **资源泄漏检查** 激活

---

## Phase 3: 证据链

### [CRITICAL] 问题 1 — 缓冲区溢出（Off-by-One）

**位置：** `review-me.c:27`

```
for (int i = 0; i <= sb->size; i++) {  // ← 应使用 < 而非 <=
    tmp[i] = sb->buffer[i];
}
```

**证据：** 当 `i == sb->size` 时，访问 `tmp[sb->size]` 和 `sb->buffer[sb->size]`，均超出分配范围（分配了 `size` 个元素，有效索引 0 ~ size-1）。写入 `tmp[sb->size]` 为堆内存越界写，读取 `sb->buffer[sb->size]` 为堆内存越界读。

**影响：** 堆内存破坏，可能导致程序崩溃或被利用执行任意代码。

---

### [CRITICAL] 问题 2 — 数据竞争（Data Race）

**位置：** `review-me.c:27-34`

```
// 读取阶段 — 未加锁 (line 27-29)
for (int i = 0; i <= sb->size; i++) {
    tmp[i] = sb->buffer[i];            // ❌ 读取时未持有锁
}
// 加锁 (line 30)
pthread_mutex_lock(&sb->lock);
// 写入阶段 (line 31-33)
for (int i = 0; i < sb->size; i++) {
    sb->buffer[i] = tmp[i] * 2;
}
pthread_mutex_unlock(&sb->lock);       // (line 34)
```

**证据：** `write_value` 在写入时持有锁（lines 20-22），但 `process_all` 的读取复制阶段（lines 27-29）完全没有加锁。并发线程可以在复制阶段通过 `write_value` 修改 `sb->buffer`，导致：
1. `tmp` 副本中的数据不一致（部分旧值、部分新值）
2. 读取的值与后续加锁后写入的值不同（TOCTOU 竞争条件）

**影响：** 数据竞争（C11 6.5.16 未定义行为），输出结果不可预测。

---

### [HIGH] 问题 3 — 数组越界（无边界检查）

**位置：** `review-me.c:20-21`

```
void write_value(SharedBuffer *sb, int index, int value) {
    pthread_mutex_lock(&sb->lock);
    sb->buffer[index] = value;          // ❌ 无边界检查
    pthread_mutex_unlock(&sb->lock);
}
```

**证据：** `index` 参数无范围检查（`0 <= index < sb->size`）。调用者传递任意 `index` 将导致越界写内存。

**影响：** 任意地址堆内存写入，安全漏洞。

---

### [HIGH] 问题 4 — 空指针解引用风险

**位置：** `review-me.c:12-14`, `review-me.c:26`

```
SharedBuffer *sb = (SharedBuffer*)malloc(sizeof(SharedBuffer));
sb->size = size;                       // ❌ malloc 失败则 sb == NULL，空指针解引用
sb->buffer = (int*)malloc(size * sizeof(int));
                                       // ❌ 两次 malloc 均未检查 NULL
```

**证据：** `create_buffer` 中两次 `malloc` 均未检查返回值是否为 NULL。若任意一次失败，后续代码解引用空指针导致程序崩溃。

`process_all` 中 `tmp` 的 `malloc`（line 26）同样未检查 NULL。

---

### [HIGH] 问题 5 — 内存泄漏（tmp）

**位置：** `review-me.c:26-35`

```
int *tmp = (int*)malloc(sb->size * sizeof(int));
// ... 使用 tmp ...
// ❌ 函数结束前无 free(tmp)
```

**证据：** `tmp` 在 `process_all` 中分配，但函数返回前从未调用 `free(tmp)`。每次调用 `process_all` 泄漏 `size * sizeof(int)` 字节。

---

### [MEDIUM] 问题 6 — 内存泄漏（sb）+ 未销毁互斥锁

**位置：** `review-me.c:37-43`

```
int main() {
    SharedBuffer *sb = create_buffer(100);
    // ... 使用 sb ...
    return 0;                          // ❌ sb 未释放，锁未销毁
}
```

**证据：** `main` 中 `sb`（包含 `sb->buffer` 和互斥锁）从未释放。`pthread_mutex_destroy` 也未调用。严格按 POSIX 要求，未销毁的互斥锁释放后行为未定义。

---

## Phase 4: 检视报告

| 项目 | 结果 |
|------|------|
| **状态** | **FAIL** |
| **被检视文件** | review-me.c |
| **严重问题数** | 2 CRITICAL + 2 HIGH + 2 HIGH(泄漏) + 1 MEDIUM |

### 问题汇总

| ID | 严重等级 | 类型 | 位置 | 描述 |
|----|---------|------|------|------|
| #1 | **CRITICAL** | 缓冲区溢出 | line 27 | Off-by-one：`i <= sb->size` 应为 `i < sb->size`，导致越界读写 |
| #2 | **CRITICAL** | 数据竞争 | lines 27-34 | 复制阶段未加锁，与 `write_value` 存在 TOCTOU 竞争 |
| #3 | **HIGH** | 数组越界 | line 21 | `write_value` 无 index 边界检查，可越界写 |
| #4 | **HIGH** | 空指针 | lines 12,14,26 | 三次 malloc 均未检查 NULL 返回值 |
| #5 | **HIGH** | 内存泄漏 | line 26 | `tmp` 在 `process_all` 中从未释放 |
| #6 | **MEDIUM** | 内存泄漏 | lines 38,15 | `sb` 未释放；`pthread_mutex_destroy` 未调用 |

### 验收标准检查

（无 prd.json — 独立检视任务。以上问题基于安全编码规范和并发编程最佳实践判定。）

### 修复建议

1. **#1 CRITICAL** — line 27：将 `<=` 改为 `<`
2. **#2 CRITICAL** — lines 27-34：将读取复制阶段移入锁保护范围内，或使用读写锁（pthread_rwlock_t）
3. **#3 HIGH** — line 21：在写入前添加 `if (index < 0 || index >= sb->size) return;` 或返回错误码
4. **#4 HIGH** — lines 12,14,26：每次 malloc 后检查是否为 NULL，为 NULL 时返回错误或清理已分配资源
5. **#5 HIGH** — line 35（函数结束前）：添加 `free(tmp);`
6. **#6 MEDIUM** — main 返回前：添加 `pthread_mutex_destroy(&sb->lock);`、`free(sb->buffer);`、`free(sb);`

---

*检视时间：2026-05-21 | 检视引擎：reviewer skill (Phase 1→2→3→4 流程)*
