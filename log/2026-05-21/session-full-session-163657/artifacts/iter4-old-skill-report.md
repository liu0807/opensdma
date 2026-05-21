## 检视结果

**状态：** FAIL

**被检视文件：**
- dev_v2/src/case9_mixed_scenario.c
- dev_v2/src/ut_sdma_main.c
- dev_v2/src/ut_sdma.h

**检视故事：** US-009 - 混合场景多线程跨进程数据拷贝测试

**验收标准检查：**
- [ ] SDMA API 使用正确性 - **未通过** - 发现多处 API 使用问题（见问题列表）
- [ ] 内存管理正确性 - **未通过** - 存在内存泄漏和未定义行为
- [ ] 并发安全性 - **未通过** - 存在数据竞争
- [ ] 输入验证充分性 - **部分通过** - 部分参数缺少范围检查
- [ ] 错误路径清理完整性 - **未通过** - 错误路径存在资源泄漏和 UB

**代码质量检查：**
- 代码模式：不符合 - 存在 CRITICAL 级 bug（static 变量共享、内存泄漏）
- 边界处理：不充分 - 缺少超时保护、CPU 范围校验、strtol 错误检测
- 一致性：有问题 - 命名风格基本一致，但内存释放逻辑有 break 滥用

**问题列表（带严重等级和修复建议）：**

1. [CRITICAL] 线程函数 `sdma_mixed_thread` 中 `static int ret` 被所有线程共享，造成数据竞争
   - 位置：case9_mixed_scenario.c:155
   - 问题：`static int ret = 0;` 是所有线程共享的全局变量。多个线程并发写入同一个地址，且每个线程都返回 `&ret` 指针给调用者。调用者通过 `*(pthread_ret[i])` 读取时可能读到被其他线程覆盖的值，导致误判线程执行结果。
   - 修复建议：将 `static` 移除，改为局部自动变量 `int ret = 0;`

2. [CRITICAL] `sdma_mem_alloc` 中 mmap 部分失败导致已分配内存泄漏
   - 位置：case9_mixed_scenario.c:202-212
   - 问题：`dst_addr` 先 mmap 成功后，如果 `src_addr` 的 mmap 失败，函数直接返回 `SDMA_TEST_FAILED`，但未对已成功的 `dst_addr` 执行 `munmap`。同样的问题存在于 `mbind` 失败路径（line 286-290）。
   - 修复建议：在 `src_addr mmap` 失败后先 `munmap(*dst_addr, mmap_size)`；在 `mbind src_addr` 失败后对 `*dst_addr` 和 `*src_addr` 都做 `munmap`。

3. [HIGH] `sdma_iquery_chn` 的 `SDMA_RNDCNT_ERR` 自旋缺少超时保护
   - 位置：case9_mixed_scenario.c:84-91
   - 问题：当 `ret == SDMA_RNDCNT_ERR` 时执行 `continue` 重试，但没有自旋次数限制。如果硬件状态异常，该循环会无限自旋。
   - 修复建议：增加自旋计数器，如 `for (int spin = 0; spin < MAX_SPIN && ret == SDMA_RNDCNT_ERR; spin++)`，超时后返回 `SDMA_TEST_FAILED`。

4. [HIGH] 共享内存进程间同步缺乏内存屏障，aarch64 弱序模型下有可见性问题
   - 位置：case9_mixed_scenario.c:667-673, 714-715, 469-477, 785-789, 804-808 等多处
   - 问题：send/recv 进程通过共享内存中的 `bool` 标志进行同步，但未使用任何原子操作或内存屏障。在 aarch64 弱内存序上，对 `submitter_process_id`/`dst_addr_list` 的写入可能在接收方看到 `pid_ready=true` 时尚未刷新到内存，导致读取脏数据。
   - 修复建议：使用 C11 的 `atomic_bool` 和 `atomic_store`/`atomic_load` 系列操作，或显式插入 `__sync_synchronize()` / `__atomic_thread_fence()`。

5. [HIGH] 错误路径中 `munmap(NULL)` 导致未定义行为
   - 位置：case9_mixed_scenario.c:295-336（`sdma_mem_release`）
   - 问题：当 `sdma_mem_alloc` 在中间索引失败时，`recv_src_addr[k]` 或 `recv_dst_addr[k]` 为 NULL。`sdma_mem_release` 在错误路径被调用时，会遍历数组并对 NULL 地址调用 `munmap`，行为未定义。
   - 修复建议：在 `munmap` 前检查指针是否为 NULL，如 `if (dst_addr[k]) { munmap(dst_addr[k], mmap_size); }`。

6. [HIGH] `case_get_input` 未对 `CPU0`/`CPU1` 输入做范围校验
   - 位置：case9_mixed_scenario.c:875-877
   - 问题：`CPU0`（send_cpu）和 `CPU1`（recv_cpu）被直接赋值并使用，仅在 thread cpu 赋值时检查 `> 607`，但缺少下界校验（< 0）。负数 `numa_id` 可导致数组越界。
   - 修复建议：在 `case_get_input` 中增加 `if (CPU0 > 607 || CPU1 > 607)` 校验。

7. [MEDIUM] 父进程未 `waitpid` 回收子进程，造成僵尸进程
   - 位置：case9_mixed_scenario.c:994-1064
   - 问题：子进程通过 `fork()` 创建后独立执行并 `_exit(0)`，但父进程从未调用 `waitpid()` 或设置 `SIGCHLD` 处理。子进程终止后会成为僵尸进程，直到父进程退出。
   - 修复建议：在父进程路径（else 分支末尾）增加 `waitpid(pid, NULL, 0)`。

8. [MEDIUM] `strtol` 缺少非数字输入检测
   - 位置：ut_sdma_main.c:250-330
   - 问题：所有 `strtol` 调用都未检查 `endptr == optarg` 的情况。当用户传入非数字字符串（如 `--case abc`）时，`strtol` 返回 0，可能被当作合法值处理，与 `case_num == 0` 的拒绝逻辑偶合但不通用。
   - 修复建议：在每个 `strtol` 调用后检查 `if (endptr == optarg) { ... error ... }`。

9. [MEDIUM] `sdma_mem_release` 中 `break` 替代 `continue` 导致提前截断释放
   - 位置：case9_mixed_scenario.c:322-330
   - 问题：循环中使用 `if (!dst_addr[k]) { break; }`，如果一个中间条目为 NULL 则停止后续所有条目的释放。当前逻辑下虽然不会出现"中间空"的情况，但这种模式易在后续代码演进中引入 bug。
   - 修复建议：将 `break` 改为 `continue`，使其能继续清理后续有效条目。
