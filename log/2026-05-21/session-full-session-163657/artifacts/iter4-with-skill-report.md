# 代码检视报告

## 代码特征扫描

- **内存操作模式**: `calloc` (case9:417,675), `mmap` (case9:202,208), `munmap` (case9:325,329), `free` (case9:300)
- **SDMA API 模式**: `sdma_alloc_chn`/`sdma_init_chn` (case9:482-484,744-746), `sdma_free_chn`/`sdma_deinit_chn` (case9:305-309), `sdma_copy_data`/`sdma_icopy_data` (case9:77,96), `sdma_wait_chn`/`sdma_iquery_chn` (case9:84,102), `sdma_pin_umem`/`sdma_unpin_umem` (case9:432-437,586-608,689-699,835-860), `sdma_get_process_id` (case9:444,702), `sdma_add_authority` (case9:463,728), `sdma_request_t` (case9:67), `sdma_sqe_task_t` (case9:64,374-509,754-771)
- **并发/同步模式**: `pthread_create` (case9:531,793), `pthread_join` (case9:551,815), `fork` (case9:995), `shmget`/`shmat`/`shmdt`/`shmctl` (case9:341-366,843-867)
- **文件/设备模式**: `open` `/dev/sdma*` (case9:1012-1051), `close` (case9:1030-1063)
- **输入处理模式**: `getopt_long` (main:241), `strtol` (main:250-325)
- **结构设计模式**: 新增 `struct sdma_mixed_th` (case9:59-68), 新增函数 `mixed_recv`/`mixed_send`/`sdma_mixed_thread`/`case9_mixed_scenario`/`sdma_mem_alloc`/`sdma_mem_release`
- **内核模式**: none

## 激活的检视维度

- [D1 动态内存管理] - 激活理由：检测到 calloc/mmap/munmap/free
- [D2 字符串与缓冲区安全] - 激活理由：检测到 sprintf
- [D3 文件描述符与设备管理] - 激活理由：检测到 open/close /dev/sdma*
- [D4 SDMA 通道生命周期] - 激活理由：检测到 sdma_alloc_chn/init_chn/free_chn/deinit_chn
- [D5 SDMA 数据拷贝] - 激活理由：检测到 sdma_copy_data/icopy_data/wait_chn/iquery_chn/sqe_task/request
- [D6 内存注册与页锁定] - 激活理由：检测到 sdma_pin_umem/unpin_umem
- [D7 跨进程权限管理] - 激活理由：检测到 sdma_get_process_id/add_authority/fork/shmget/shmat
- [D8 并发与同步] - 激活理由：检测到 pthread_create/join/fork/shmget/shmat
- [D9 错误处理与资源清理] - 激活理由：检测到 goto/unpin_mem_*/release_mem_*/unbind_share_mem/release_share_mem
- [D10 命令行与输入处理] - 激活理由：检测到 getopt_long/strtol
- [D11 代码结构与命名规范] - 激活理由：新增函数/结构体/文件

## 验收标准检查

当前为独立检视场景，无 prd.json 上下文，跳过验收标准检查。

## 问题详情

---

### [1]. pthread_create 后错误路径未 join 导致 use-after-free @ dev_v2/src/case9_mixed_scenario.c:530-539, 792-801

**严重等级**: CRITICAL
**检视维度**: D8-11

**证据链**:
1. **存在性**: `mixed_recv` (line 530-531) 和 `mixed_send` (line 792-793) 在 for 循环中创建线程，之后在 line 534-539 / 796-801 检查 `status[i]` 是否就绪；若超时则 `goto unpin_mem_recv` / `goto unpin_mem_send`。错误标签 `unpin_mem_recv` (line 597-608) 和 `unpin_mem_send` (line 849-860) 中无任何 `pthread_cancel` + `pthread_join` 操作，直接释放资源后返回。
2. **影响性**: 线程函数 `sdma_mixed_thread` (line 151-183) 持有指向 `mixed_recv`/`mixed_send` 栈帧的指针：`temp->sdma` 指向栈数组 `sdma[]`、`temp->g_barrier` 指向栈变量 `g_barrier`、`temp->status` 指向栈数组 `status[]`。当 `mixed_recv`/`mixed_send` 返回后栈帧回收：
   - `sdma_mem_release` (line 612/864) 调用 `sdma_deinit_chn(sdma[i])`/`sdma_free_chn(sdma[i])` 释放通道句柄，而线程仍持有指向该句柄的指针并可能继续调用 `sdma_icopy_data(sdma, ...)` → 通道 handle use-after-free。
   - 线程读取 `temp->g_barrier` 访问已回收栈 → 未定义行为。
3. **触发条件**: `ready_status_timeout_judgement(&status[i])` 在 `USLEEP_TIME * TIMEOUT = 20 * 300000 = 6s` 内未返回（线程启动延迟或系统繁忙），或线程启动后 `sdma_test()` 执行时 `sdma_iquery_chn` 超时。该分支在正常负载下概率较低，但在高竞争/低资源环境可达。
4. **保护排除**: 代码中无任何保护。`g_barrier = true` (line 549/812) 仅在成功路径 `pthread_join` (line 550-552/814-816) 之前设置，错误路径完全不涉及线程生命周期管理。已创建的线程数 `THREAD_NUM` 已知但未传递给清理代码。

**修复建议**:
在 `mixed_recv` 的 `unpin_mem_recv` 前增加线程取消+join 逻辑：
```c
// 在 goto unpin_mem_recv 之前（或标签内），对已创建的线程进行处理
err_after_thread_create:
    g_barrier = true;  // 让已创建的线程从等待中退出
    for (int t = 0; t < THREAD_NUM; t++) {
        pthread_join(tid[t], NULL);
    }
    // 然后继续 fall through 到 unpin_mem_recv
```
`mixed_send` 同理。需在 `pthread_create` 处记录实际已创建的线程数（防止在循环中途跳转）。

---

### [2]. sdma_mixed_thread 中 static ret 导致数据竞争 @ dev_v2/src/case9_mixed_scenario.c:155

**严重等级**: CRITICAL
**检视维度**: D8-1

**证据链**:
1. **存在性**: `sdma_mixed_thread` line 155 声明 `static int ret = 0;`，所有线程共享同一 `static` 变量。line 171-182 中该变量被赋值并取地址返回：`return (void *)&ret;`。调用方 `mixed_recv` (line 553-560) / `mixed_send` (line 817-824) 通过 `*(pthread_ret[i])` 读取线程返回值。
2. **影响性**: 所有线程共用同一 `static int ret`，产生无锁写竞争：
   - 线程 A 执行 sdma_test 返回失败，`ret = SDMA_TEST_FAILED`（-1）
   - 线程 B 同时通过 sdma_test 返回成功，`ret = 0`
   - 调用方读取 `*(pthread_ret[A])` 可能得到 0（线程 B 写入的值），误判线程 A 成功
   - 导致线程 A 的实际失败被静默忽略，调用方继续执行后续流程使用不完整数据
3. **触发条件**: 任何线程执行失败时，只要有其他线程在其后被调度并成功执行。多线程并发场景下几乎确定触发，只是依赖调度时序的竞态窗口大小问题。
4. **保护排除**: `pthread_join` 提供 happen-before 语义，但仅限于参与 join 的线程之间。`static ret` 是所有线程共享的，某线程的写入可被 join 后仍被另一线程覆盖。`pthread_ret[i]` 指针数组每个元素指向同一 `&ret`，无法区分各线程的返回值。

**修复建议**:
去掉 `static`，改为每个线程栈上局部变量：
```c
static void *sdma_mixed_thread(void *arg)
{
    ...
    int ret = 0;  // 每个线程独有的栈变量
    ...
    ret = sdma_test(...);
    ...
    return (void *)(intptr_t)ret;  // 直接返回值而非取地址
}
```
调用方改为直接取值：
```c
for (i = 0; i < THREAD_NUM; i++) {
    void *thread_ret;
    pthread_join(tid[i], &thread_ret);
    if ((int)(intptr_t)thread_ret != 0) {
        printf("...failed\n");
        goto unpin_mem_recv;
    }
}
```

---

### [3]. case9_mixed_scenario 中 open 第二个设备失败时未 close 第一个设备 @ dev_v2/src/case9_mixed_scenario.c:1022,1054

**严重等级**: HIGH
**检视维度**: D3-2

**证据链**:
1. **存在性**: 子进程 line 1012-1022 中先 `open("/dev/sdmaX", O_RDWR)` 获取 fd1，再 `open("/dev/sdmaY", O_RDWR)` 获取 fd2。若 fd2 open 失败（line 1020-1022），函数直接 `return SDMA_TEST_FAILED` 而未 `close(fd1)`。父进程 line 1044-1054 同理：fd4 open 失败时未 `close(fd3)`。
2. **影响性**: fd1 / fd3 文件描述符泄漏。在长时间运行的测试进程中，重复触发此失败路径将逐渐耗尽进程 FD 限制（ulimit -n），导致后续所有 open 失败，使测试功能全面瘫痪。
3. **触发条件**: `/dev/sdmaY` 设备不存在、驱动未加载、权限不足或并发打开数超限。此路径在测试环境配置错误或驱动异常时可达。
4. **保护排除**: 代码中无任何保护。子进程使用 `_exit(0)` 退出（line 1032），在进程退出时 FD 会被内核回收，不会对系统级资源造成永久泄漏。但父进程（line 1062-1063）继续运行，FD泄漏影响当前进程后续操作。此外，即使子进程退出清理，未释放的通道/内存等资源在 `return` 前也未清理（第二 open 失败时 sqe_task 未分配、通道未初始化）。

**修复建议**:
```c
fd2 = open(sdma_dev, O_RDWR);
if (fd2 < 0) {
    printf("open sdma%d failed!\n", device_num_2);
    close(fd1);  // 添加
    return SDMA_TEST_FAILED;
}
```
父进程 fd4 同理：
```c
fd4 = open(sdma_dev, O_RDWR);
if (fd4 < 0) {
    printf("open sdma%d failed!\n", device_num_1);
    close(fd3);  // 添加
    return SDMA_TEST_FAILED;
}
```

---

### [4]. sdma_mem_alloc 失败路径 MAP_FAILED 未被 sdma_mem_release 正确处理 @ dev_v2/src/case9_mixed_scenario.c:202-212,295-336

**严重等级**: MEDIUM
**检视维度**: D1-8, D1-9

**证据链**:
1. **存在性**: `sdma_mem_alloc` 中 line 202 `*dst_addr = mmap(...)`，若成功则 line 208 `*src_addr = mmap(...)`。若 line 208 失败，`*src_addr` 被设为 `MAP_FAILED`（即 `(void*)-1`），line 209-212 直接 `return SDMA_TEST_FAILED`。之后调用 `sdma_mem_release` 中 line 326 `if (!src_addr[k])` 检查：`MAP_FAILED` 不为 NULL，检查通过，line 329 执行 `munmap(MAP_FAILED, mmap_size)`。
2. **影响性**: `munmap(MAP_FAILED, ...)` 在 Linux 上返回 -1/EINVAL（不崩溃），但属于 API 误用；若 `*dst_addr` mmap 也失败（line 202 返回 MAP_FAILED），同理 `dst_addr[k]` == MAP_FAILED 也会触发 `munmap(MAP_FAILED, ...)`。
3. **触发条件**: `mmap` 在分配内存时失败（巨页不足或系统内存压力）。
4. **保护排除**: `sdma_mem_release` 的 NULL 检查 `if (!dst_addr[k])` 和 `if (!src_addr[k])` 对 `MAP_FAILED` 无效，因为 `(void*)-1 != NULL`。

**修复建议**:
将 `sdma_mem_release` 中的 NULL 检查改为 `MAP_FAILED` 检查：
```c
if (dst_addr[k] == NULL || dst_addr[k] == MAP_FAILED) {
    break;
}
munmap(dst_addr[k], mmap_size);
if (src_addr[k] == NULL || src_addr[k] == MAP_FAILED) {
    break;
}
munmap(src_addr[k], mmap_size);
```
或在进入 `sdma_mem_release` 前确保 MAP_FAILED 值被置为 NULL。

---

### [5]. ready_status_timeout_judgement NULL 指针检查不完整 @ dev_v2/src/case9_mixed_scenario.c:113-130

**严重等级**: MEDIUM
**检视维度**: D9-1

**证据链**:
1. **存在性**: line 117-119 检查 `ready == NULL` 时仅打印 `"detected NULL!"`，但不 return。line 121 执行 `while (!(*ready) && ...)` 解引用空指针。
2. **影响性**: 若 `ready` 为 NULL，line 121 对空指针解引用导致段错误，进程崩溃。
3. **触发条件**: 在调用 `ready_status_timeout_judgement` 的路径中，如果传入的布尔指针本身为 NULL。当前代码中调用点传入的都是有效栈/共享内存地址（`&g_barrier`, `&status[i]`, `&shared->*`），此路径当前不可达。但如果未来重构引入新调用，或传递未初始化指针，则可能触发。
4. **保护排除**: 当前调用点均传入有效地址，所以 NULL 不会发生。但检查代码本身存在语义缺陷——检查到问题后不退出，属于防御性编程"检查但不处理"的反模式。

**修复建议**:
```c
if (ready == NULL) {
    printf("detected NULL!\n");
    return SDMA_TEST_FAILED;  // 添加
}
```

---

### [6]. sdma_request_t 零值初始化未经验证 @ dev_v2/src/case9_mixed_scenario.c:67,402

**严重等级**: MEDIUM
**检视维度**: D5-11

**证据链**:
1. **存在性**: `struct sdma_mixed_th` 包含嵌入式 `sdma_request_t request` 成员（line 67），`pt_input` 数组通过 `memset(pt_input, 0, ...)` 零初始化（line 402/652）。因此 `request` 的 `req_id=0`, `req_cnt=0`, `round_cnt=0`。该 request 被传递给 `sdma_icopy_data` 和 `sdma_iquery_chn`（line 77,84）。
2. **影响性**: `sdma_request_t` 定义（mdk_sdma.h:45-49）包含 `req_id`, `req_cnt`, `round_cnt`。SDMA API 可能将全零字段视为无效请求，导致 `sdma_icopy_data` 拒绝提交或 `sdma_iquery_chn` 行为异常。参考 D5-11 规则描述："memset 零值的 request 可能被 API 视为无效（req_id/req_cnt/round_cnt 零值）"。
3. **触发条件**: SDMA 下层实现中对 request 字段有非零校验时立即触发。具体是否触发取决于驱动实现细节。
4. **保护排除**: 无。代码直接使用零值未进行任何初始化/设置。

**修复建议**:
在创建线程后、提交任务前，对 request 的字段进行显式初始化：
```c
pt_input[i].request.req_id = 1;
pt_input[i].request.req_cnt = 1;
pt_input[i].request.round_cnt = LOOP;
```
或根据 API 文档补充正确的初始值。

---

### [7]. numa_id < 0 逻辑死代码 @ dev_v2/src/case9_mixed_scenario.c:1003,1035

**严重等级**: LOW
**检视维度**: D11-10

**证据链**:
1. **存在性**: line 1002-1003（子进程）和 line 1034-1035（父进程）对 `numa_id` 做边界检查：`if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2))`。其中 `numa_id = CPU0 / CPU_PER_NODE` 或 `CPU1 / CPU_PER_NODE`。
2. **影响性**: `CPU0` 和 `CPU1` 来自 `cmd->send_cpu` / `cmd->recv_cpu`（uint32_t），上限验证为 607。`CPU_PER_NODE` 为 38/144/152（正值）。两个正整数的整除结果 `numa_id >= 0` 永远成立，因此 `numa_id < 0` 分支永远不会执行，属于逻辑死代码。
3. **触发条件**: 永不触发。
4. **保护排除**: 该检查位于业务代码中，不影响运行正确性；但死代码降低可维护性，给后续维护者造成困惑。

**修复建议**:
移除 `numa_id < 0` 检查，仅保留 `numa_id >= (NUMA_NODE_NUMS / 2)` 检查：
```c
if (numa_id >= (NUMA_NODE_NUMS / 2)) {
    printf("numa id %d out of range, now set to 0...\n", numa_id);
    numa_id = 0;
}
```

---

### [8]. sprintf 缺少长度限制 @ dev_v2/src/case9_mixed_scenario.c:1011,1018,1043,1050

**严重等级**: LOW
**检视维度**: D2-2

**证据链**:
1. **存在性**: line 1011,1018,1043,1050 使用 `sprintf(sdma_dev, "/dev/sdma%d", device_num_x)` 构建设备路径。`sdma_dev` 大小为 `DEV_LEN = 20`。
2. **影响性**: 格式串 `/dev/sdma` 9 字符 + device_num 理论上最多约 3 位十进制 = 12 字符 + NUL = 13，小于 20。当前是安全的，但使用 `sprintf` 而非 `snprintf` 是脆弱的编码风格，后续若修改 `DEV_LEN` 或格式串，容易引入缓冲区溢出。
3. **触发条件**: 仅在 DEV_LEN 被改小或 device_num 格式变更时触发。
4. **保护排除**: 当前值安全，但无编译期或运行期保护。

**修复建议**:
```c
snprintf(sdma_dev, sizeof(sdma_dev), "/dev/sdma%d", device_num_1);
```

---

## 检视结论

**状态**: FAIL

**已确认问题数**: 8（CRITICAL: 2, HIGH: 1, MEDIUM: 3, LOW: 2）

| 序号 | 维度 | 严重等级 | 简要描述 |
|------|------|---------|---------|
| #1 | D8-11 | CRITICAL | pthread_create 错误路径未 join → 线程 use-after-free |
| #2 | D8-1 | CRITICAL | static ret 数据竞争 → 线程返回值被覆盖 |
| #3 | D3-2 | HIGH | 第二设备 open 失败未 close 第一设备 |
| #4 | D1-8/D1-3 | MEDIUM | sdma_mem_release 对 MAP_FAILED 误处理 |
| #5 | D9-1 | MEDIUM | ready_status_timeout_judgement NULL 检查不完整 |
| #6 | D5-11 | MEDIUM | sdma_request_t 零值初始化未验证 |
| #7 | D11-10 | LOW | numa_id < 0 逻辑死代码 |
| #8 | D2-2 | LOW | sprintf 无长度限制（当前安全） |

PASS 条件：无 CRITICAL/HIGH 问题。
结果：FAIL（2 CRITICAL + 1 HIGH，不满足通过条件）。
