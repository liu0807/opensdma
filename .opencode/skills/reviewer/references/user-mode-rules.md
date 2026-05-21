# 用户态检视规则

此文件包含详细的用户态检视维度。每个维度包含：
- **触发模式**: 哪些代码模式激活此维度
- **检查清单**: 具体的检查项
- **严重等级指南**: 什么情况下归什么等级
- **SDMA 特有模式**: SDMA 项目中特有的检查要点

---

## 目录

- [D1: 动态内存管理](#d1-动态内存管理)
- [D2: 字符串与缓冲区安全](#d2-字符串与缓冲区安全)
- [D3: 文件描述符与设备管理](#d3-文件描述符与设备管理)
- [D4: SDMA 通道生命周期](#d4-sdma-通道生命周期)
- [D5: SDMA 数据拷贝](#d5-sdma-数据拷贝)
- [D6: 内存注册与页锁定](#d6-内存注册与页锁定)
- [D7: 跨进程权限管理](#d7-跨进程权限管理)
- [D8: 并发与同步](#d8-并发与同步)
- [D9: 错误处理与资源清理](#d9-错误处理与资源清理)
- [D10: 命令行与输入处理](#d10-命令行与输入处理)
- [D11: 代码结构与命名规范](#d11-代码结构与命名规范)

---

## D1: 动态内存管理

**触发模式**: `malloc` `calloc` `realloc` `free` `mmap` `munmap`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 1.1 | malloc/calloc 返回值是否检查 NULL | CRITICAL | 分配失败后解引用=空指针崩溃 |
| 1.2 | 分配的内存是否在**所有**路径上释放 | CRITICAL | 包括错误路径、提前返回路径 |
| 1.3 | free 后的指针是否被再次使用 | CRITICAL | Use-After-Free，堆漏洞 |
| 1.4 | 是否 double-free | CRITICAL | 同一指针调用两次 free |
| 1.5 | calloc vs malloc 选择是否正确 | MEDIUM | calloc 已清零，malloc 需手动 memset |
| 1.6 | mmap(MAP_HUGETLB) 是否配对 munmap | HIGH | 巨页资源有限，泄漏影响系统 |
| 1.7 | mmap 返回值是否检查 MAP_FAILED | HIGH | mmap 失败返回 (void*)-1 |
| 1.8 | 结构体内部指针与结构体本身的释放顺序 | HIGH | 先释放内部指针再释放结构体 |
| 1.9 | 偏移/对齐是否在分配范围内 | HIGH | 指针运算后是否越界 |
| 1.10 | 分配大小是否涉及整数溢出 | HIGH | size 来自外部计算时可能溢出 |

### SDMA 特有模式

```c
// 正确: calloc + NULL 检查 + goto 清理
void *phandle = calloc(1, sizeof(sdma_handle_t));
if (!phandle) {
    SDMA_ERR("calloc handle fail");
    goto err_out;
}
// ... 使用 ...
err_out:
    free(phandle);
    return ret;
```

```c
// 错误: 无 NULL 检查，无清理 goto
void *phandle = calloc(1, sizeof(sdma_handle_t));
// 缺少 if (!phandle) 检查
```

---

## D2: 字符串与缓冲区安全

**触发模式**: `strcpy` `strncpy` `sprintf` `snprintf` `memcpy` `memmove` `gets` `scanf`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 2.1 | strcpy 目标缓冲区大小是否小于源字符串 | CRITICAL | 栈/堆缓冲区溢出 |
| 2.2 | sprintf 是否使用 %s 格式且无长度限制 | CRITICAL | vs snprintf(buf, sizeof(buf), ...) |
| 2.3 | memcpy 的 len 参数是否超过目标缓冲区 | CRITICAL | 目标缓冲区实际可用大小 |
| 2.4 | memcpy 的 src/dst 是否可能重叠 | HIGH | 重叠应使用 memmove |
| 2.5 | snprintf 返回值是否检查截断 | MEDIUM | 返回值 >= size 表示截断 |
| 2.6 | 字符串指针是否为 NULL 后才调用 strlen/strcpy | HIGH | NULL 指针传入字符串函数 |
| 2.7 | strncpy 是否缺少手动 NUL 终止 | MEDIUM | strncpy 不自动追加 NUL |
| 2.8 | sizeof vs strlen 混用 | HIGH | sizeof 是数组大小，strlen 是字符串长度 |

### SDMA 特有模式

日志格式化：
```c
// 正确: 用 snprintf 限制长度
snprintf(err_buf, sizeof(err_buf), "SDMA ERROR (%s|%d): [%s] %s",
         __func__, __LINE__, hostname, msg);
```

---

## D3: 文件描述符与设备管理

**触发模式**: `open` `close` `ioctl` `/dev/sdma`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 3.1 | open 返回值是否检查 < 0 | CRITICAL | FD 无效后继续使用 |
| 3.2 | 打开的 FD 是否在所有路径上关闭 | HIGH | FD 泄漏耗尽系统资源 |
| 3.3 | ioctl 返回值是否检查 | HIGH | 内核拒绝请求时静默失败 |
| 3.4 | ioctl 传入参数是否经过验证 | HIGH | 无效指针/大小传入 ioctl |
| 3.5 | 同一 FD 是否被重复关闭 | MEDIUM | close(-1) 安全，但 close(有效FD) 两次危险 |

### SDMA 特有模式

```c
// 正确: open + 错误处理 + close
fd = open("/dev/sdma0", O_RDWR);
if (fd < 0) {
    SDMA_ERR("open sdma0 fail");
    return ret;
}
// ... 使用 ...
close(fd);
```

---

## D4: SDMA 通道生命周期

**触发模式**: `sdma_alloc_chn` `sdma_init_chn` `sdma_free_chn` `sdma_deinit_chn`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 4.1 | sdma_alloc_chn 返回值是否检查 NULL | CRITICAL | 分配失败返回 NULL |
| 4.2 | sdma_init_chn 返回值是否检查 NULL | CRITICAL | 初始化失败返回 NULL |
| 4.3 | alloc_chn 是否配对 free_chn | HIGH | 独占通道泄漏 |
| 4.4 | init_chn 是否配对 deinit_chn | HIGH | 共享通道泄漏 |
| 4.5 | 独占通道是否正确使用 sync API | HIGH | alloc_chn → copy_data/wait_chn |
| 4.6 | 共享通道是否正确使用 async API | HIGH | init_chn → icopy_data/iwait_chn |
| 4.7 | 跨模式混用 API 是否被检测 | CRITICAL | 独占通道上调用 icopy_data 等 |

### API 配对规则

```
独占通道
  sdma_alloc_chn(fd) → void* phandle
  sdma_copy_data(phandle, sqe, count)   // 同步提交
  sdma_wait_chn(phandle, count)          // 同步等待
  sdma_free_chn(phandle)                 // 释放

共享通道
  sdma_init_chn(fd, chn_idx) → void* phandle
  sdma_icopy_data(phandle, sqe, count, req)  // 异步提交
  sdma_iwait_chn(phandle, req)               // 阻塞等待
  sdma_iquery_chn(phandle, req)              // 非阻塞查询
  sdma_progress(phandle)                     // 处理完成事件
  sdma_deinit_chn(phandle)                   // 去初始化
```

---

## D5: SDMA 数据拷贝

**触发模式**: `sdma_copy_data` `sdma_icopy_data` `sdma_wait_chn` `sdma_iwait_chn` `sdma_iquery_chn` `sdma_query_chn` `sdma_progress` `sdma_sqe_task_t`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 5.1 | sdma_sqe_task 的 src_addr/dst_addr 是否为有效地址 | HIGH | 无效地址导致 SMMU 错误 |
| 5.2 | length 字段是否超过目标缓冲区大小 | CRITICAL | 越界 DMA 写入 |
| 5.3 | src_stride_len/dst_stride_len + stride_num 组合是否有效 | HIGH | 总跨距计算: data_size * stride_num + stride_len * (stride_num - 1) |
| 5.4 | opcode 是否合法（0x0 normal/0x5 memset/0x6 preload） | MEDIUM | 非法 opcode 被硬件拒绝 |
| 5.5 | sdma_copy_data/icopy_data 返回值是否检查 | CRITICAL | 提交失败继续等待导致死等 |
| 5.6 | sdma_wait_chn/iwait_chn 返回值是否检查 | CRITICAL | 等待超时数据不完整 |
| 5.7 | count 参数与 sqe 数组长度是否一致 | HIGH | 提交 N 个 task 但 count 是 M |
| 5.8 | icopy_data 的 request 是否在 iwait 时正确传递 | HIGH | request 关联错误的等待 |
| 5.9 | sqe 填充是否在被提交后继续修改 | MEDIUM | 提交后 sqe 由硬件读取 |
| 5.10 | next_sqe 链表是否正确终止 (NULL) | HIGH | 链表未终止导致遍历越界 |
| 5.11 | sdma_request_t 是否显式初始化或确认零值合法 | MEDIUM | memset 零值的 request 可能被 API 视为无效（req_id/req_cnt/round_cnt 零值） |
| 5.12 | 重试循环（如 do-while ret==RNDCNT_ERR）是否有超时边界 | HIGH | 硬件异常时无超时重试导致无限自旋，需增加最大自旋次数 |

### SDMA 特有模式

```c
// 正确的 sqe_task 填充模式
sdma_sqe_task_t sqe = {0};
sqe.src_addr = (uint64_t)src_buf;
sqe.dst_addr = (uint64_t)dst_buf;
sqe.length = data_size;
sqe.opcode = 0x0;  // normal copy
sqe.src_stride_len = 0;  // non-stride
sqe.dst_stride_len = 0;
sqe.stride_num = 0;
sqe.next_sqe = NULL;

// stride 模式: 总跨距 = data_size * stride_num + stride_len * (stride_num - 1)
sqe.src_stride_len = stride_len;
sqe.dst_stride_len = stride_len;
sqe.stride_num = stride_count;
```

---

## D6: 内存注册与页锁定

**触发模式**: `sdma_pin_umem` `sdma_unpin_umem` `hisi_sdma_umem_info`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 6.1 | sdma_pin_umem 返回值是否检查 0 | CRITICAL | pin 失败后继续 DMA=数据损坏 |
| 6.2 | pin 的 cookie 是否保存供 unpin 使用 | HIGH | cookie 丢失导致无法 unpin |
| 6.3 | pin/unpin 是否成对出现 | HIGH | pin 而不 unpin 导致页永久锁定 |
| 6.4 | pin 的 size 与实际使用的 size 是否一致 | HIGH | pin 小了 DMA 越界 |
| 6.5 | unpin 时 cookie 是否与 pin 返回的一致 | MEDIUM | cookie 不匹配导致错误 |
| 6.6 | ib_umem 方式 vs sdma_pin_umem 方式的选择 | MEDIUM | 用途不同不可混用 |

### SDMA 特有模式

```c
// 正确: pin + 使用 + unpin 模式
uint64_t cookie = 0;
ret = sdma_pin_umem(fd, buf, size, &cookie);
if (ret != 0) {
    SDMA_ERR("pin umem fail, ret=%d", ret);
    goto err_out;
}
// ... DMA 操作 ...
ret = sdma_unpin_umem(fd, cookie);
if (ret != 0) {
    SDMA_ERR("unpin umem fail, ret=%d", ret);
}
```

---

## D7: 跨进程权限管理

**触发模式**: `sdma_get_process_id` `sdma_add_authority` `sdma_remove_authority` `fork` `shmget` `shmat`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 7.1 | 跨进程拷贝前是否先 add_authority | CRITICAL | 无权限的跨 PID 拷贝被硬件拒绝 |
| 7.2 | get_process_id 返回值是否检查 | HIGH | PASID 获取失败 |
| 7.3 | add_authority 的 id_list 是否包含正确的 PID | CRITICAL | 添加错误 PID=安全漏洞 |
| 7.4 | 跨进程共享内存 (shmget/shmat) 是否同步访问 | HIGH | 两个进程同时读写 |
| 7.5 | fork 后子进程中的 SDMA 句柄行为 | HIGH | fork 后的 handle 可能无效 |
| 7.6 | 权限是否在使用后清理(remove_authority) | MEDIUM | 残留权限=安全风险 |

### SDMA 特有模式

```c
// 跨进程拷贝: 权限设置流程
uint32_t pid = 0;
ret = sdma_get_process_id(fd, &pid);     // 获取本进程 PASID
if (ret != 0) { /* 错误处理 */ }

uint32_t auth_list[] = {target_pid};
ret = sdma_add_authority(fd, auth_list, 1);  // 授权目标 PID
if (ret != 0) { /* 错误处理 */ }

// sqe 中设置 PID 字段
sqe.src_process_id = my_pid;
sqe.dst_process_id = target_pid;
```

---

## D8: 并发与同步

**触发模式**: `pthread_create` `pthread_join` `pthread_mutex_*` `__sync_bool_compare_and_swap` `__sync_fetch_and_*` `fork` `shmget` `shmat` `shmdt` `shmctl`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 8.1 | 共享数据的无锁读取是否与其他线程的加锁写入重叠 | CRITICAL | 数据竞争（常见于 read-then-write 模式） |
| 8.2 | 互斥锁 init/lock/unlock/destroy 是否配对 | HIGH | 锁资源泄漏或死锁 |
| 8.3 | pthread_mutex_lock 返回值是否检查 | MEDIUM | 死锁检测或 EOWNERDEAD |
| 8.4 | 临界区是否过大（持锁调用阻塞操作） | MEDIUM | 降低并发性能 |
| 8.5 | 临界区是否过小（分步操作被拆成多次锁） | HIGH | TOCTOU 竞争条件 |
| 8.6 | 原子操作 (CAS) 是否用于正确的共享变量 | HIGH | CAS 模式错误导致状态不一致 |
| 8.7 | 共享内存 (shm) 多进程访问是否需要锁 | HIGH | 缺少进程间同步 |
| 8.8 | shmget key 是否可能与其他进程冲突 | MEDIUM | 使用 IPC_PRIVATE 或唯一 key |
| 8.9 | 信号处理函数中是否调用了非 async-signal-safe 函数 | HIGH | printf/malloc 在 signal handler 中 |
| 8.10 | 线程间数据传递是否存在生命周期问题 | HIGH | 栈变量被传给其他线程 |
| 8.11 | pthread_create 后错误路径是否 join 已创建的线程 | CRITICAL | 在 create→join 之间 goto/return，线程仍运行但栈即将回收，导致 use-after-free |

### SDMA 特有模式

```c
// 危险模式: pthread_create 后错误路径未 join
for (i = 0; i < THREAD_NUM; i++) {
    pthread_create(&tid[i], NULL, thread_func, &arg[i]);
}
// ... 某些操作失败 ...
goto err_out;  // ❌ tid[] 中的线程仍在运行
// ...
err_out:
    // 只清理资源，未处理已创建的线程
    free(mem);
    return FAIL;

// 正确模式: 错误路径先取消并 join 线程
err_out:
    g_barrier = true;  // 让线程正常退出
    for (i = 0; i < thread_created; i++) {
        pthread_join(tid[i], NULL);
    }
    free(mem);
    return FAIL;
```

```c
// sdma 中的自旋锁实现（使用 GCC 原子操作）
static inline void sdma_lock_chn(sdma_handle_t *phandle) {
    while (__sync_bool_compare_and_swap(
        &phandle->q_data.lock, 0, 1) == 0) {
        ; // spin wait
    }
}
// 检查: 锁变量必须 volatile，所有访问经过此锁保护
```

---

## D9: 错误处理与资源清理

**触发模式**: `goto` cleanup 标签、错误码、`err_out` `err_free` `err_unmap` `release` 等

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 9.1 | goto 标签间是否存在跨未初始化变量的跳转 | HIGH | 跳转到未初始化资源的清理代码 |
| 9.2 | 所有错误路径是否都释放了已分配的堆内存 | CRITICAL | error path leak |
| 9.3 | 所有错误路径是否关闭了已打开的 FD | HIGH | error path FD leak |
| 9.4 | 所有错误路径是否 unpin 了已 pin 的内存 | HIGH | error path 内存残留 |
| 9.5 | 函数是否有多条 return 语句导致清理遗漏 | HIGH | 存在 return 在 cleanup 代码之前 |
| 9.6 | 错误码语义是否正确 (0=成功, 负数=错误) | MEDIUM | SDMA 接口约定 |
| 9.7 | 错误消息中是否包含足够的上下文信息 | LOW | 至少函数名+行号+错误码 |
| 9.8 | SDMA_ERR vs SDMA_DBG 选择是否正确 | LOW | 运行时错误用 ERR，调试用 DBG |

### SDMA 特有模式

SDMA 库中的 goto 清理模式：
```c
// 标准模式: 按分配顺序反向释放
func() {
    // 分配资源1
    res1 = alloc1();
    if (!res1) goto err_out;
    // 分配资源2
    res2 = alloc2();
    if (!res2) goto err_free1;
    // 分配资源3
    res3 = alloc3();
    if (!res3) goto err_free2;
    // ... 使用 ...
    ret = SUCCESS;
err_free3:
    free_res3(res3);
err_free2:
    free_res2(res2);
err_free1:
    free_res1(res1);
err_out:
    return ret;
}
```

---

## D10: 命令行与输入处理

**触发模式**: `getopt_long` `strtol` `atoi` `sscanf` `optarg` `argc` `argv`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 10.1 | strtol/atoi 是否检查转换错误 (errno 和 endptr) | HIGH | 非数字输入导致未定义值。**必须**作为独立 issue 输出，不能仅标注维度激活。检查：errno=0 前置、errno==ERANGE 后置、endptr==optarg 无转换 |
| 10.2 | strtol 的范围是否在业务范围内验证 | HIGH | 负数索引、超大 size |
| 10.3 | getopt_long 的 optarg 是否检查 NULL | MEDIUM | 需要参数时未提供 |
| 10.4 | 命令行参数数量 argc 是否验证 | MEDIUM | 缺少必要参数 |
| 10.5 | 整数溢出: data_size * loop_times 是否检查 | HIGH | 用户输入的计算结果溢出 |
| 10.6 | 枚举类参数（如 memory_type）是否验证范围 | HIGH | 传入非法枚举值 |
| 10.7 | 类型转换: uint32_t = atoi() 负数截断 | MEDIUM | atoi 返回负数转 uint32 |

### SDMA 特有模式

```c
// 正确的参数解析模式
static int parse_args(int argc, char **argv, struct sdma_test_input *input) {
    int opt;
    while ((opt = getopt_long(argc, argv, "d:l:h", long_opt, NULL)) != -1) {
        switch (opt) {
        case 'd': {
            long val = strtol(optarg, NULL, 10);
            if (val <= 0 || val > MAX_DATA_SIZE) {
                fprintf(stderr, "invalid data size: %ld\n", val);
                return -1;
            }
            input->data_size = (uint32_t)val;
            break;
        }
        // ...
        }
    }
}
```

---

## D11: 代码结构与命名规范

**触发模式**: 新增函数/文件/结构体/宏定义

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| 11.1 | 函数命名是否使用 snake_case | MEDIUM | SDMA 项目约定 |
| 11.2 | 类型定义是否使用 `_t` 后缀 | MEDIUM | 如 sdma_handle_t |
| 11.3 | 宏定义是否使用全大写 | MEDIUM | 如 MAX_BUFFER_SIZE |
| 11.4 | 指针变量是否使用 `p` 前缀（Huawei 风格） | LOW | pchan, phandle, pbuffer |
| 11.5 | 头文件保护宏是否使用 `__NAME_H__` 格式 | LOW | #ifndef __MDK_SDMA_H__ |
| 11.6 | goto 标签是否使用 `err_` 前缀 | MEDIUM | err_out, err_free |
| 11.7 | 公开 API 是否有中文注释（功能描述/参数/返回值） | MEDIUM | SDK 接口文档约定 |
| 11.8 | 变量是否在函数顶部声明（C89 风格） | LOW | SDMA 库中 C89，测试代码 C99 均可 |
| 11.9 | 函数是否职责单一 | MEDIUM | 一个函数做一件事 |
| 11.10 | 是否包含死代码（注释掉的代码、未使用的函数、逻辑死条件） | LOW | 代码可维护性。逻辑死代码：条件永远成立/不成立（如 unsigned >= 0、主动赋值的变量后立即对同一值分支） |
| 11.11 | 返回值是否正确传递/使用 | MEDIUM | ret = func(); if (ret) 后续使用 ret |

### SDMA 特有模式

头文件注释风格：
```c
/**
 * sdma_alloc_chn - 申请独占通道
 * @fd: 设备文件描述符
 *
 * 功能描述: 从SDMA控制器申请一条独占通道，返回通道句柄
 * 输入参数: fd - 设备文件描述符
 * 输出参数: 无
 * 返回值: 成功返回通道句柄，失败返回NULL
 */
```

---

## 严重等级指南

| 等级 | 定义 | 例子 |
|------|------|------|
| **CRITICAL** | 必然导致崩溃/数据损坏/安全漏洞 | 空指针解引用、缓冲区溢出、数据竞争 |
| **HIGH** | 高风险，特定条件下导致严重问题 | 资源泄漏、缺乏边界检查、API 不配对 |
| **MEDIUM** | 可维护性问题，或仅有理论风险 | 死代码、返回值未检查（非安全相关）、命名不一致 |
| **LOW** | 风格/文档问题 | 注释不足、轻微风格不符 |

### 降级/升级规则

- **升级 MEDIUM→HIGH**: 问题在**错误路径**上出现（错误处理代码本身有 bug）
- **升级 HIGH→CRITICAL**: 问题发生在**外部可控输入**的路径上
- **降级 HIGH→MEDIUM**: 问题发生在非关键路径上（如仅用于调试的代码）
- **降级 CRITICAL→HIGH**: 有部分保护但不够充分（非完全没有保护）
