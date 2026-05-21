## 检视结果

**状态：** FAIL

**被检视文件：**
- `reviewer-workspace/fixtures/review-sample/buggy.c`

**检视故事：** N/A（独立检视测试）

**验收标准检查：**
- N/A（无对应 PRD 故事）

**代码质量检查：**
- 代码模式：不符合 - 多处违反安全编码规范
- 边界处理：不充分 - 多处缺少长度和空指针检查
- 一致性：有问题 - 硬编码 Magic Number，缺少命名常量

**问题列表（带严重等级和修复建议）：**

1. [CRITICAL] **栈缓冲区溢出** - `process_records()` 第 24 行 `strcpy(buf, input)`：`buf` 固定 64 字节，`input` 由 `argv[1]` 传入无长度限制。输入超过 63 字符即溢出栈内存，可被利用执行任意代码。
   - 修复建议：使用 `strncpy(buf, input, MAX_BUF - 1); buf[MAX_BUF - 1] = '\0';` 或 `snprintf(buf, MAX_BUF, "%s", input);`

2. [CRITICAL] **栈缓冲区溢出** - `create_record()` 第 16 行 `strcpy(r->name, name)`：`name` 字段仅 32 字节，`name` 参数无长度限制。输入超过 31 字符即溢出。
   - 修复建议：使用 `strncpy(r->name, name, sizeof(r->name) - 1); r->name[sizeof(r->name) - 1] = '\0';`

3. [CRITICAL] **空指针解引用** - `create_record()` 第 15 行 `r->id = id;`：`malloc` 可能返回 NULL，未做检查直接解引用导致段错误。
   - 修复建议：`malloc` 后立即判断 `if (r == NULL) return NULL;`

4. [CRITICAL] **空指针解引用** - `create_record()` 第 18 行 `sprintf(r->data, ...)`：`malloc` 可能返回 NULL，未做检查。
   - 修复建议：`malloc` 后立即判断 `if (r->data == NULL) { free(r); return NULL; }`

5. [CRITICAL] **数组越界写入** - `process_records()` 第 31 行 `recs[count] = create_record(count, token);`：`recs` 固定 10 个元素，无越界检查。输入超过 10 个 token 时写入栈外内存。
   - 修复建议：循环中增加 `if (count >= 10) break;` 防止越界

6. [HIGH] **内存泄漏** - `create_record()` 分配的 `Record`（第 14 行）和 `r->data`（第 17 行）在 `process_records()` 中从未释放。每次调用泄漏约 280+ 字节。
   - 修复建议：`process_records()` 返回前添加循环 `for (int i = 0; i < count; i++) { free(recs[i]->data); free(recs[i]); }`

7. [HIGH] **内存泄漏（部分分配失败场景）** - 若第 17 行 `malloc` 失败，第 14 行的 `Record` 已分配但未释放，直接返回 NULL 导致内存泄漏。
   - 修复建议：第 17 行 `malloc` 失败时应先 `free(r)` 再返回 NULL

8. [MEDIUM] **使用不安全的 `sprintf`** - `create_record()` 第 18 行 `sprintf(r->data, "Record-%d-data", id)`：未指定最大长度，若 `id` 值很大可能溢出 256 字节缓冲区。
   - 修复建议：使用 `snprintf(r->data, 256, "Record-%d-data", id);`

9. [MEDIUM] **`strtok` 修改原始输入** - `process_records()` 第 29 行 `strtok(buf, ",")` 修改 `buf` 内容，实际修改了 `argv[1]` 指向的命令行参数字符串。
   - 修复建议：若需保留原始输入，先做 `strdup` 再 `strtok`

10. [LOW] **硬编码 Magic Number** - `create_record()` 第 17 行硬编码 `256`，无命名常量说明含义。
    - 修复建议：定义宏如 `#define DATA_BUF_SIZE 256` 并注释用途

11. [LOW] **未检查 `printf` 返回值** - `process_records()` 第 37 行 `printf` 返回值未被检查。
    - 修复建议：在安全敏感场景中可添加返回值检查并处理写入错误

**总计：** 5 个 CRITICAL、2 个 HIGH、2 个 MEDIUM、2 个 LOW — 共 11 个问题
