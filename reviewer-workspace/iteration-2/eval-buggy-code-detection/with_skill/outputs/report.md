# 代码检视报告

## 检视概述

| 项目 | 内容 |
|------|------|
| 检视文件 | `reviewer-workspace/fixtures/review-sample/buggy.c` |
| 文件行数 | 48 |
| 检视依据 | Reviewer Skill 4 阶段流程（语义分析驱动） |
| strictness | normal（不允许 CRITICAL/HIGH 等级问题） |

---

## Phase 1: 代码特征扫描

### 检测到的内存操作模式
- **动态内存分配**：`malloc` — 第 14 行（`sizeof(Record)` 分配）、第 17 行（256 字节分配）
- **动态内存释放**：未检测到任何 `free` 调用
- **不安全字符串函数**：`strcpy` — 第 16 行（`r->name`）、第 24 行（`buf`）；`sprintf` — 第 18 行（`r->data`）
- **指针解引用和数组访问**：第 15、16、18、31、37 行（直接解引用未 NULL 检查）；第 26 行（固定大小数组 `recs[10]`）

### 检测到的并发/同步模式
- 未检测到

### 检测到的 API/驱动模式
- 未检测到

### 检测到的输入处理模式
- **外部参数**：`argv[1]` — 第 46 行（命令行输入）
- **字符串解析**：`strtok` — 第 29、33 行（逗号分隔解析）
- **参数校验**：`argc < 2` 检查 — 第 42 行（仅检查参数存在性）

### 检测到的结构设计模式
- **结构体定义**：`Record` — 第 7~11 行（含 `id`、`name[32]`、`*data` 字段）
- **新增函数**：`create_record`（第 13 行）、`process_records`（第 22 行）、`main`（第 41 行）
- **函数间调用链**：`main` → `process_records` → `create_record`

---

## Phase 2: 激活的检视维度

| 检视维度 | 激活状态 | 激活理由 |
|---------|---------|---------|
| [内存安全] | **已激活** | 检测到 `malloc`×2（无配对 `free`），`strcpy`×2，`sprintf`×1，无 NULL 检查的指针解引用 |
| [并发安全] | 未激活 | 无并发/同步相关模式 |
| [API 使用正确性] | 未激活 | 无 SDMA/IOCTL/设备操作相关模式 |
| [输入与边界验证] | **已激活** | 检测到 `argv` 外部输入、`strtok` 字符串解析、无边界保护的数组访问 |
| [设计质量] | **已激活** | 结构体定义、新增函数、兜底维度（代码存在变更） |
| [验收标准检查] | **始终激活** | 对照需求逐条确认 |

---

## Phase 3 & 4: 问题详情（含证据链）

---

### 问题 1. 栈缓冲区溢出（Stack Buffer Overflow）@ buggy.c:24

**严重等级**: CRITICAL
**检视维度**: 内存安全 / 输入与边界验证

**证据链**:
1. **[存在性]** 第 23 行 `char buf[MAX_BUF]` 声明 64 字节（`#define MAX_BUF 64`）栈缓冲区；第 24 行 `strcpy(buf, input)` 将用户输入的 `input` 直接拷贝到该缓冲区，无任何长度限制。
2. **[影响性]** 用户输入超过 63 字符时发生栈缓冲区溢出，覆盖栈上局部变量（如 `recs[10]`、`count`）以及函数返回地址，可导致程序崩溃或被利用执行任意代码。
3. **[触发条件]** 运行 `buggy.exe` 并传入长度 ≥64 字符的命令行参数。由于 `argv[1]` 完全由用户控制，该触发条件是确定性可达的。
4. **[保护排除]** `MAX_BUF` 定义为 64，但 `strcpy` 本身不检查目标容量；第 22 行函数签名 `const char *input` 未携带长度信息，调用方（第 46 行 `main`）也未做任何长度校验。无任何保护措施。

**修复建议**:
```c
// 方案一（推荐）: 使用 strncpy_s 或 snprintf 限定拷贝长度
snprintf(buf, sizeof(buf), "%s", input);

// 方案二: 先校验长度再拷贝
if (strlen(input) >= MAX_BUF) {
    fprintf(stderr, "Error: input too long\n");
    return;
}
strcpy(buf, input);  // 此时安全
```

---

### 问题 2. 堆缓冲区溢出（Heap Buffer Overflow）@ buggy.c:16

**严重等级**: CRITICAL
**检视维度**: 内存安全 / 输入与边界验证

**证据链**:
1. **[存在性]** 第 9 行 `char name[32]` 声明 32 字节固定数组；第 16 行 `strcpy(r->name, name)` 将 `name` 参数拷贝到该数组，无长度限制。
2. **[影响性]** 当 `name` 长度 ≥32 字节时，写入超出 `name[32]` 边界，破坏堆上相邻内存（包括 `r->id` 或其他堆块元数据），可导致堆损坏、程序崩溃或安全漏洞。
3. **[触发条件]** `name` 参数来源于第 31 行 `strtok(buf, ",")` 返回的 token。如果输入字符串中包含长度 ≥32 的逗号分隔字段，`create_record(count, token)` 调用将触发此溢出。
4. **[保护排除]** 第 22 行 `process_records` 函数及第 46 行 `main` 均未对 token 长度做任何限制；`strtok` 返回原始字符串片段，无截断机制。

**修复建议**:
```c
// 使用 strncpy 并确保 null-terminate
strncpy(r->name, name, sizeof(r->name) - 1);
r->name[sizeof(r->name) - 1] = '\0';
```

---

### 问题 3. 数组越界写入（Array Bounds Overflow）@ buggy.c:31

**严重等级**: CRITICAL
**检视维度**: 内存安全 / 输入与边界验证

**证据链**:
1. **[存在性]** 第 26 行 `Record *recs[10]` 声明固定 10 个元素的指针数组；第 30~34 行 `while` 循环中每轮执行 `recs[count] = create_record(count, token)` 后再 `count++`，`count` 无上限检查。
2. **[影响性]** 当输入逗号分隔字段数超过 10 个时，写入 `recs[10]`、`recs[11]`... 超出数组边界，破坏栈上相邻变量（如 `count`、`buf`、函数返回地址），导致崩溃或代码执行。
3. **[触发条件]** 输入参数包含 11 个以上逗号分隔字段。例如 `buggy.exe "a,b,c,d,e,f,g,h,i,j,k"`。
4. **[保护排除]** 无任何边界检查。`count` 在 while 循环中无条件递增；第 36 行 `for` 循环使用 `count` 遍历，但 `count` 可能已经超过 10。

**修复建议**:
```c
while (token != NULL && count < 10) {  // 添加边界检查
    recs[count] = create_record(count, token);
    count++;
    token = strtok(NULL, ",");
}
```

---

### 问题 4. 内存泄漏（Memory Leak）@ buggy.c:14,17

**严重等级**: HIGH
**检视维度**: 内存安全

**证据链**:
1. **[存在性]** `create_record` 函数第 14 行 `malloc(sizeof(Record))` 和第 17 行 `malloc(256)` 分配了两块堆内存；整个文件中没有任何 `free` 调用与之配对。
2. **[影响性]** 每次调用 `create_record` 泄漏 `sizeof(Record) + 256` 字节堆内存。当输入包含 10 个字段时，一次 `process_records` 调用泄漏约 10×(40+256) ≈ 2960 字节。长期运行的程序（或循环调用时）会耗尽内存。
3. **[触发条件]** 任何对 `create_record` 的调用（通过 `process_records` 的 while 循环，第 31 行）都会导致泄漏。无需特殊输入。
4. **[保护排除]** `process_records` 函数（第 36~38 行）的 `for` 循环仅打印数据后直接结束，未对 `recs` 数组中的指针做任何释放操作；`create_record` 函数也未提供配套的销毁函数。

**修复建议**:
```c
// 添加销毁函数
void destroy_record(Record *r) {
    if (r) {
        free(r->data);
        free(r);
    }
}

// 在 process_records 结束时调用
for (int i = 0; i < count; i++) {
    destroy_record(recs[i]);
}
```

---

### 问题 5. NULL 指针解引用 @ buggy.c:15,16,18

**严重等级**: HIGH
**检视维度**: 内存安全

**证据链**:
1. **[存在性]** 第 14 行 `malloc(sizeof(Record))` 后，第 15~18 行直接解引用 `r->id`、`r->name`、`r->data` 未检查 `r` 是否为 NULL；第 17 行 `malloc(256)` 后第 18 行直接 `sprintf(r->data, ...)` 未检查 `r->data` 是否为 NULL。
2. **[影响性]** 如果 `malloc` 失败（内存耗尽），`r` 或 `r->data` 为 NULL，则 `strcpy` / `sprintf` 通过 NULL 指针写入将导致段错误（SIGSEGV），程序崩溃。
3. **[触发条件]** 系统内存不足时，`malloc` 返回 NULL。（嵌入式/资源受限环境中可能性较高，通用桌面环境可能性较低但仍然是未定义行为。）
4. **[保护排除]** 无任何 NULL 检查。

**修复建议**:
```c
Record* create_record(int id, const char *name) {
    Record *r = (Record*)malloc(sizeof(Record));
    if (!r) return NULL;
    r->id = id;
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->name[sizeof(r->name) - 1] = '\0';
    r->data = (char*)malloc(256);
    if (!r->data) {
        free(r);
        return NULL;
    }
    snprintf(r->data, 256, "Record-%d-data", id);
    return r;
}
```

---

### 问题 6. 设计缺陷——缺少资源清理路径 @ buggy.c:31,36-38

**严重等级**: MEDIUM
**检视维度**: 设计质量

**证据链**:
1. **[存在性]** 第 31 行分配资源后，如果中间某次 `create_record` 失败返回 NULL，第 37 行 `recs[i]->name` 将解引用 NULL 指针；且已创建的前 i 个 Record 无法被清理（无释放机制）。
2. **[影响性]** 部分失败场景下，既不能安全中止，也不能清理已有资源，导致内存泄漏 + 可能的崩溃组合。
3. **[触发条件]** 中间某次 `malloc` 失败（参考问题 5）；或者未来 `create_record` 增加了其他可能失败的逻辑。
4. **[保护排除]** 当前代码既无 NULL 检查（调用方），也无清理循环；即使 `create_record` 返回 NULL，调用方仍直接赋值并继续。

**修复建议**:
```c
void process_records(const char *input) {
    char buf[MAX_BUF];
    if (strlen(input) >= MAX_BUF) return;
    strcpy(buf, input);

    Record *recs[10] = {NULL};  // 初始化为 NULL
    int count = 0;

    char *token = strtok(buf, ",");
    while (token != NULL && count < 10) {
        recs[count] = create_record(count, token);
        if (!recs[count]) {   // 创建失败时清理已分配的资源
            for (int j = 0; j < count; j++) destroy_record(recs[j]);
            return;
        }
        count++;
        token = strtok(NULL, ",");
    }

    for (int i = 0; i < count; i++) {
        printf("Record %d: %s\n", recs[i]->id, recs[i]->name);
    }

    for (int i = 0; i < count; i++) destroy_record(recs[i]);
}
```

---

### 问题 7. 潜在整数溢出（Integer Overflow）@ buggy.c:32

**严重等级**: LOW
**检视维度**: 输入与边界验证

**证据链**:
1. **[存在性]** 第 27 行 `int count = 0`；第 32 行 `count++` 为有符号整数递增。
2. **[影响性]** 如果输入字段数超过 `INT_MAX`（2,147,483,647），`count` 回绕为负值，但受限于 `recs[10]` 数组大小，此问题实际被问题 3（数组越界）先触发，因此单独风险较低。
3. **[触发条件]** 理论上需要超过 21 亿个逗号分隔字段，实际不可达。
4. **[保护排除]** 问题 3 的边界检查（若修复）会限制 count 最大为 10，间接防止此溢出。

**修复建议**:
若修复了问题 3 的数组边界检查（`count < 10`），此问题自动消除。单独修复可改为 `size_t count`。

---

## 检视结论

| 指标 | 数值 |
|------|------|
| **状态** | **FAIL** |
| 已确认问题总数 | 7 |
| CRITICAL | 3（问题 1、2、3） |
| HIGH | 2（问题 4、5） |
| MEDIUM | 1（问题 6） |
| LOW | 1（问题 7） |

**PASS 条件判断**（strictness: normal）：
- ❌ 不允许 CRITICAL 或 HIGH 等级的问题 → 存在 3 CRITICAL + 2 HIGH
- ❌ 未通过

**汇总说明**：
`buggy.c` 存在 3 个 CRITICAL 级别的安全漏洞（栈/堆缓冲区溢出、数组越界写入），均可以通过用户恶意构造的命令行输入直接触发。此外还有 2 个 HIGH 级别问题（内存泄漏、NULL 指针解引用）和 1 个 MEDIUM 级别设计缺陷。代码整体上未遵循安全编码规范，对用户输入缺乏基本校验，资源管理不完整。必须修复所有 CRITICAL 和 HIGH 问题后才能通过检视。

---

## 修复优先级建议

1. **[CRITICAL] 问题 1 — 栈溢出** → 使用 `snprintf` 替代 `strcpy`，或先校验长度
2. **[CRITICAL] 问题 2 — 堆溢出** → 使用 `strncpy` 限制拷贝长度
3. **[CRITICAL] 问题 3 — 数组越界** → `while` 循环增加 `count < 10` 边界检查
4. **[HIGH] 问题 4 — 内存泄漏** → 添加 `destroy_record` 并在 `process_records` 末尾调用
5. **[HIGH] 问题 5 — NULL 指针** → `malloc` 后检查返回值
6. **[MEDIUM] 问题 6 — 清理路径** → `create_record` 失败时释放已分配资源
7. **[LOW] 问题 7 — 整数溢出** → 随问题 3 修复后自动缓解
