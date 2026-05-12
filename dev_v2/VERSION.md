# 版本信息

## 版本号：v2

## 创建时间
2026-05-12

## 对应需求
- PRD: tasks/prd-mixed-scenario-test.md
- 新增：--direction 参数（控制数据搬运方向）

## 需求范围（v2 新增）
- US-005: 实现 --direction 参数控制数据搬运方向
  - 0=线程内：所有线程均下发任务，搬运地址为各自的 src → dst
  - 1=线程间单向：仅 send 创建的线程下发任务，搬运地址 send.src → recv.dst
  - 2=线程间双向：send/recv 线程均下发任务，搬运地址 send.src→recv.dst, recv.src→send.dst

## v1 已完成需求（继承到 v2）
- US-001: 创建新测试用例框架（case9_mixed_scenario.c）
- US-002: 扩展测试输入结构体（struct sdma_test_input）
- US-003: 实现 stride 参数控制（--src-stride, --dst-stride, --stride-num，范围0~4M）
- US-004: 实现通道类型和线程控制（--chn-type, --thread-num, --send, --recv）

## 源码基线
- 基于 dev_v1/ 修复后的代码（含 PROC_NUM 修复、next_sqe 越界修复、thread_num 上限检查、print_help 更新）

## 版本隔离说明
- 本版本所有修改在 dev_v2/ 目录下进行
- 与 dev_v1/ 完全隔离，v1 代码保持不变
