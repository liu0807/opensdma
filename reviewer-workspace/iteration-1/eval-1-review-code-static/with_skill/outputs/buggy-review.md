## 检视结果

**状态：** FAIL

**检视文件：** buggy.c（48 行 C 源码）

---

### 代码质量检查

- **代码模式：** 不符合 — 多处存在 C 语言经典安全陷阱（strcpy/sprintf 无边界检查）
- **边界处理：** 不充分 — 几乎完全缺乏输入验证和边界保护
- **一致性：** 良好 — 命名风格和缩进一致，但整体安全编码规范缺失

---

### 问题列表

#### 严重（CRITICAL）

1. **`strcpy` 缓冲区溢出 — `process_records()` L24**
   - `buf[MAX_BUF]` = 64 字节，`input` 来自 `argv[1]`，长度无任何限制
   - `strcpy(buf, input)` 可导致栈缓冲区溢出，引发程序崩溃或被利用
   - **修复：** 改用 `strncpy(buf, input, sizeof(buf) - 1)` 或 `snprintf(buf, sizeof(buf), "%s", input)`

2. **`strcpy` 缓冲区溢出 — `create_record()` L16**
   - `r->name[32]` 固定 32 字节，`name` 参数无长度约束
   - 超长 name 直接覆盖栈/堆后续内存
   - **修复：** `strncpy(r->name, name, sizeof(r->name) - 1)`，并在 `r->name[sizeof(r->name) - 1] = '\0'` 确保终止

3. **`sprintf` 缓冲区溢出 — `create_record()` L18**
   - `r->data` 仅分配 256 字节，`sprintf` 不检查目标大小
   - 虽然当前格式串较安全，但后续维护容易引入溢出；应改为防御性写法
   - **修复：** `snprintf(r->data, 256, "Record-%d-data", id)`

4. **malloc 返回值未检查 — `create_record()` L14, L17**
   - 两处 `malloc` 均未检查是否为 NULL
   - 内存耗尽时直接解引用 NULL 指针导致段错误（DoS）
   - **修复：** 每次 malloc 后 `if (!r) return NULL;`，调用者也需处理 NULL 返回

5. **内存泄漏 — `process_records()`**
   - `create_record()` 每次调用分配 `Record` 和 `r->data`，但 `process_records()` 返回前从未释放
   - 每次调用泄漏两段堆内存
   - **修复：** 在函数返回前遍历 `recs[0..count-1]`，逐一 `free(recs[i]->data); free(recs[i]);`

#### 中危（MODERATE）

6. **数组越界写入 — `process_records()` L31**
   - `Record *recs[10]` 固定 10 个元素，但循环中 `count++` 无上限检查
   - 输入超过 10 个 token（逗号分隔）时写入 `recs[10+]` 越界
   - **修复：** 在 `while` 循环内 `if (count >= 10) break;`

7. **strtok 可重入性问题**
   - `strtok()` 不是线程安全的，且内部状态在库函数间共享
   - 若此代码后被多线程上下文调用，会产生难以排查的竞态问题
   - **修复：** 改用 `strtok_s()`（C11 Annex K）或 `strtok_r()`（POSIX）

#### 低危（LOW）

8. **`main()` 无返回值检查**
   - `process_records(argv[1])` 未检查参数是否为 NULL（`argc < 2` 已处理，但无额外防御）
   - 当前无实质风险，但有总比没有好

---

### 修复建议（汇总）

按优先级排列：

| 优先级 | 问题 | 建议修复 |
|--------|------|----------|
| P0 | L24 strcpy 栈溢出 | `strncpy(buf, input, sizeof(buf)-1)` |
| P0 | L16 strcpy 堆溢出 | `strncpy(r->name, sizeof(r->name)-1, name)` |
| P0 | L14/L17 malloc 无 NULL 检查 | 添加 `if (!r)` 处理 |
| P0 | 内存泄漏 | `process_records()` 返回前释放所有 Record |
| P1 | L31 数组越界 | 加 `if (count >= 10) break` |
| P1 | L18 sprintf 溢出 | `snprintf(r->data, 256, ...)` |
| P2 | strtok 线程安全 | 改用 `strtok_s` / 注明非线程安全 |
