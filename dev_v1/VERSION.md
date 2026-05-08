# 版本信息

## 版本号：v1

## 创建时间
2026-05-08

## 对应需求
- PRD: tasks/prd-mixed-scenario-test.md
- prd.json: prd.json (copy to dev_v1/prd.json)

## 需求范围
- US-001: 创建新测试用例框架（case9_mixed_scenario.c）
- US-002: 扩展测试输入结构体（struct sdma_test_input）
- US-003: 实现 stride 参数控制（--src-stride, --dst-stride, --stride-num，范围0~4M）
- US-004: 实现通道类型和线程控制（--chn-type, --thread-num, --send, --recv）

## 源码基线
- 基于 src/ 目录：case0~case8, ut_sdma.h, ut_sdma_main.c
- 基于 lib/ 目录：mdk_sdma.c/h, hisi_sdma.h

## 版本隔离说明
- 本版本所有修改在 dev_v1/ 目录下进行
- 后续新增需求将创建 dev_v2/, dev_v3/ 等，与本次代码隔离
- 每个版本独立维护自己的源码和版本信息

## 工作目录
- 根目录：D:\Opencode\test_ralph\dev_v1
- 源码目录：D:\Opencode\test_ralph\dev_v1\src
- 库目录：D:\Opencode\test_ralph\dev_v1\lib
