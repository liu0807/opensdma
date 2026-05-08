# PRD: SDMA 混合场景测试用例（参数化）

## 简介

为 sdma tool 新增一个测试用例 `case9_mixed_scenario.c`，基于 `case_muti_direction.c` 框架，但调整为**只有 send 和 recv 两个进程**，每个进程各自创建线程来下发搬运任务。通过新增参数控制 stride 模式、通道类型和线程数量，默认普通模式（三个 stride 字段均为0）。

## 目标

- 在 `src/` 下新增 `case9_mixed_scenario.c`，不修改现有用例
- 修改 `struct sdma_test_input` 添加字段：src_stride_len、dst_stride_len、stride_num（范围 0~4M）、chn_type、thread_num
- 支持通过命令行参数控制：
  - `--src-stride (-S)`：源步长长度（0~4M）
  - `--dst-stride (-D)`：目标步长长度（0~4M）
  - `--stride-num (-N)`：步长数量（0~4M）
  - `--chn-type (-C)`：通道类型（控制 send/recv 进程的通道类型）
  - `--thread-num (-T)`：线程数量
  - `--send (-s)`：send 进程绑核号（[0, 607]）
  - `--recv (-r)`：recv 进程绑核号（[0, 607]）
- 测试混合通道类型（通过 --chn-type 参数控制）
- 复用 `case_muti_direction.c` 的多进程框架（调整为两个进程：send/recv）
- 每个进程创建线程下发搬运任务，线程一一对应，循环次数 1~2000

## 用户故事

### US-001: 创建新测试用例框架
**描述：** 作为开发者，我需要创建 `case9_mixed_scenario.c`，基于 `case_muti_direction.c` 但只有 send 和 recv 两个进程，每个进程创建线程下发任务，线程一一对应。

**验收标准：**
- [ ] 在 `src/` 下创建 `case9_mixed_scenario.c`
- [ ] 调整为两个进程（send 和 recv），复用 fork 框架
- [ ] 每个进程创建线程（数量由 --thread-num 参数控制）
- [ ] send 的 thread1 对应 recv 的 thread1，以此类推（一一对应）
- [ ] 每个线程只处理自己及对端内存的搬运任务，不使用其他内存
- [ ] 线程绑定核：send 进程绑核号由 --send 指定，其线程绑核号为 send+1, send+2, ...
- [ ] 线程绑定核：recv 进程绑核号由 --recv 指定，其线程绑核号为 recv+1, recv+2, ...
- [ ] 所有线程核号必须在 [0, 607] 范围内
- [ ] 复用 `case_muti_direction.c` 的共享内存同步、NUMA 绑定、内存固定流程
- [ ] 通过 `ut_sdma_main.c` 的参数解析框架注册新用例（case 13）
- [ ] Typecheck 通过

### US-002: 扩展测试输入结构体
**描述：** 作为开发者，我需要扩展 `struct sdma_test_input`，添加 stride 参数、通道类型和线程数量字段。

**验收标准：**
- [ ] 在 `src/ut_sdma.h` 的 `struct sdma_test_input` 中添加字段：
  - `src_stride_len`：源步长长度（有效值范围 0~4M）
  - `dst_stride_len`：目标步长长度（有效值范围 0~4M）
  - `stride_num`：步长数量（有效值范围 0~4M）
  - `chn_type`：通道类型（控制 send/recv 进程的通道类型）
  - `thread_num`：线程数量（控制 send/recv 进程创建的线程数）
- [ ] 在 `ut_sdma_main.c` 中添加参数解析：
  - `--src-stride (-S)`：设置 src_stride_len（范围 0~4M）
  - `--dst-stride (-D)`：设置 dst_stride_len（范围 0~4M）
  - `--stride-num (-N)`：设置 stride_num（范围 0~4M）
  - `--chn-type (-C)`：设置通道类型
  - `--thread-num (-T)`：设置线程数量
  - `--send (-s)`：send 进程绑核号（范围 [0, 607]）
  - `--recv (-r)`：recv 进程绑核号（范围 [0, 607]）
- [ ] 更新 `case9_mixed_scenario` 函数签名以使用新参数
- [ ] Typecheck 通过

### US-003: 实现 stride 参数控制
**描述：** 作为测试人员，我希望通过 `--src-stride`、`--dst-stride`、`--stride-num` 参数控制 stride 模式，默认普通模式（三个字段均为0），有效值范围 0~4M。

**验收标准：**
- [ ] 默认普通模式：src_stride_len=0, dst_stride_len=0, stride_num=0, opcode=OPCODE_COMMON_MODE
- [ ] 使用 `--src-stride (-S)` 参数设置 sqe_task 的 src_stride_len 字段（范围 0~4M）
- [ ] 使用 `--dst-stride (-D)` 参数设置 sqe_task 的 dst_stride_len 字段（范围 0~4M）
- [ ] 使用 `--stride-num (-N)` 参数设置 sqe_task 的 stride_num 字段（范围 0~4M）
- [ ] 当三个参数非0时，启用 stride 传输模式
- [ ] 不支持 scatter-gather 模式
- [ ] Typecheck 通过

### US-004: 实现通道类型和线程控制
**描述：** 作为测试人员，我希望通过 `--chn-type` 参数控制 send/recv 进程的通道类型，通过 `--thread-num` 控制线程数量，线程一一对应。

**验收标准：**
- [ ] 使用 `--chn-type (-C)` 参数控制通道类型：
  - send 进程根据 chn_type 选择通道：独占（sdma_alloc_chn + sdma_copy_data/sdma_wait_chn）或共享（sdma_init_chn + sdma_icopy_data/sdma_iwait_chn）
  - recv 进程同样根据 chn_type 选择通道类型
- [ ] 使用 `--thread-num (-T)` 参数控制每个进程创建的线程数量
- [ ] send 进程创建 thread_num 个线程，绑核号为 send+1, send+2, ..., send+thread_num
- [ ] recv 进程创建 thread_num 个线程，绑核号为 recv+1, recv+2, ..., recv+thread_num
- [ ] 所有线程核号必须在 [0, 607] 范围内
- [ ] 线程一一对应：send 的 thread-i 只与 recv 的 thread-i 交互（共享内存同步）
- [ ] 每个线程只处理自己及对端内存的搬运任务，不使用其他内存
- [ ] Typecheck 通过

## 功能需求

- FR-1: 新用例文件 `src/case9_mixed_scenario.c`，不修改现有 case0~8
- FR-2: 修改 `src/ut_sdma.h` 的 `struct sdma_test_input`（添加 src_stride_len、dst_stride_len、stride_num（范围 0~4M）、chn_type、thread_num 字段）
- FR-3: 扩展 `src/ut_sdma_main.c` 的参数解析（添加 --src-stride/-S、--dst-stride/-D、--stride-num/-N、--chn-type/-C、--thread-num/-T、--send/-s、--recv/-r 参数）
- FR-4: 在 `src/ut_sdma.h` 中添加 `case9_mixed_scenario` 函数原型
- FR-5: 在 `src/ut_sdma_main.c` 的 `eventMap` 和 `case_names[]` 中注册新用例（case 13）
- FR-6: 修改 `src/ut_sdma_main.c` 的参数范围检查：
  - --send、--recv 核号必须在 [0, 607] 范围内
  - --src-stride、--dst-stride、--stride-num 必须在 [0, 4M] 范围内
  - 所有线程核号（send+1 ~ send+thread_num, recv+1 ~ recv+thread_num）必须在 [0, 607] 范围内
- FR-7: 复用 `case_muti_direction.c` 的组件：
  - 两个进程框架（fork：send/recv 角色）
  - 每个进程创建 thread_num 个线程（`pthread_create`）
  - 线程核绑定：send 进程线程绑核号为 send+1, send+2, ...；recv 进程线程绑核号为 recv+1, recv+2, ...
  - 线程一一对应：send 的 thread-i 只与 recv 的 thread-i 交互
  - 每个线程只处理自己及对端内存的搬运任务
  - 共享内存同步（`shared_use_st`、`shmget`/`shmat`）
  - NUMA 绑定（`sdma_mem_alloc`、`mbind`）
  - 内存固定（`sdma_pin_umem`）
  - 通道初始化和函数选择（根据 `chn_type` 选择 sdma_alloc_chn/sdma_init_chn 及对应下发/回收函数）

## 非目标

- 不修改现有 case0~8 测试用例
- 不修改 `lib/mdk_sdma.c/h` 的接口实现
- 不添加新的 SDMA 接口
- 不添加传输模式参数（如 `--transfer-mode`）
- 不添加 opcode 类型参数（如 `--opcode-type`）
- 不添加数据长度模式参数（如 `--data-mode`）
- 不进行 scatter-gather 模式测试（用户未要求）
- 不进行性能统计和日志（用户未要求）

## 技术考虑

- 参考 `src/ut_sdma.h:38-50` 了解 `struct sdma_test_input` 现有字段
- 参考 `src/ut_sdma_main.c:196-268` 了解参数解析实现
- 参考 `src/case_muti_direction.c` 的多进程、多线程实现
- 使用 `src/ut_sdma.h:14` 定义的 `OPCODE_COMMON_MODE` 等常量
- 内存分配复用 `case_muti_direction.c:162-270` 的 `sdma_mem_alloc` 函数
- 参考 `case_muti_direction.c:128-160` 的线程创建和核绑定逻辑
- 错误处理：检查每个 SDMA 接口调用的返回值，检查核号范围 [0, 607]

## 成功指标

- 新用例能成功编译：`gcc -o sdma_tool *.c -lnuma -lsdma_dk`
- 通过 stride 参数能控制传输模式（范围 0~4M）：
  - 默认：普通模式（src_stride_len=0, dst_stride_len=0, stride_num=0）
  - 使用参数：`--src-stride 10 --dst-stride 11 --stride-num 2` 启用 stride 模式
- 通过 --chn-type 参数控制通道类型
- 通过 --thread-num 参数控制线程数量，线程一一对应
- send 绑核 --send（范围 [0, 607]），recv 绑核 --recv（范围 [0, 607]）
- 线程绑核号顺延（send+1, send+2, ...），所有核号在 [0, 607] 内
- 所有参数组合下测试通过（返回 `SDMA_TEST_SUCCESS`）
- 测试日志清晰，能区分不同任务的配置和结果

## 待解决问题

- stride 模式下的 stride_num 最大值是多少？（用户指定 4M = 4 * 1024 * 1024）
- 混合通道模式下，send 和 recv 进程的数据如何同步？
- 循环次数范围 1~2000，默认值和用户指定方式？
- 线程一一对应的共享内存结构如何设计？
- 每个线程只处理自己及对端内存，如何保证不越界？
