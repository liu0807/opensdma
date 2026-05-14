#!/bin/bash
# v2 功能验证脚本 - case9 (case 13) 混合场景测试
# 覆盖所有参数组合：direction、thread-num、chn-type、stride 参数
# 在鲲鹏 aarch64 目标机上执行

SDMA_TOOL="./sdma_tool"
CASE="-c 13"
SEND="-s 0"
RECV="-r 4"
CPU_NUM="-n 144"
DATA_SIZE="-d 4096"
LOOP="-l 10"
MEM="-m 1"

PASS=0
FAIL=0

run_test() {
    local desc="$1"
    shift
    echo "----------------------------------------"
    echo "TEST: $desc"
    echo "CMD: $SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM $@"
    $SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM "$@"
    if [ $? -eq 0 ]; then
        echo "RESULT: PASS"
        PASS=$((PASS + 1))
    else
        echo "RESULT: FAIL"
        FAIL=$((FAIL + 1))
    fi
    echo "----------------------------------------"
    echo ""
}

# =============================================
# Group 1: direction 基本功能验证
# =============================================
echo "========================================"
echo " Group 1: direction 基本功能"
echo "========================================"

run_test "direction=0 (线程内，默认，1 thread)" -T 1
run_test "direction=0 (线程内，4 threads)" -T 4
run_test "direction=1 (线程间单向，1 thread)" -T 1 -R 1
run_test "direction=1 (线程间单向，4 threads)" -T 4 -R 1
run_test "direction=2 (线程间双向，1 thread)" -T 1 -R 2
run_test "direction=2 (线程间双向，4 threads)" -T 4 -R 2

# =============================================
# Group 2: direction + thread-num 组合
# =============================================
echo "========================================"
echo " Group 2: direction + thread-num 组合"
echo "========================================"

for dir in 0 1 2; do
    for thr in 2 8 16 32; do
        run_test "direction=$dir thread-num=$thr" -T "$thr" -R "$dir"
    done
done

# =============================================
# Group 3: direction + chn-type 组合
# =============================================
echo "========================================"
echo " Group 3: direction + chn-type 组合"
echo "========================================"

for dir in 0 1 2; do
    run_test "direction=$dir chn-type=0 (共享通道)" -T 4 -R "$dir" -C 0
    run_test "direction=$dir chn-type=1 (独占通道)" -T 4 -R "$dir" -C 1
done

# =============================================
# Group 4: direction + stride 组合
# =============================================
echo "========================================"
echo " Group 4: direction + stride 参数组合"
echo "========================================"

for dir in 0 1 2; do
    run_test "direction=$dir stride(1K,1K,4)" -T 2 -R "$dir" -S 1024 -D 1024 -N 4
    run_test "direction=$dir stride(4K,4K,8)" -T 2 -R "$dir" -S 4096 -D 4096 -N 8
    run_test "direction=$dir stride(1M,2M,2)" -T 2 -R "$dir" -S 1048576 -D 2097152 -N 2
done

# =============================================
# Group 6: 多方向 + 不同数据大小
# =============================================
echo "========================================"
echo " Group 6: direction + 不同数据大小"
echo "========================================"

for dir in 0 1 2; do
    run_test "direction=$dir data_size=1K" -T 2 -R "$dir" -d 1024
    run_test "direction=$dir data_size=1M" -T 2 -R "$dir" -d 1048576
    run_test "direction=$dir data_size=4M" -T 2 -R "$dir" -d 4194304
done

# =============================================
# Group 7: 异常输入验证（预期失败）
# =============================================
echo "========================================"
echo " Group 7: 异常输入（预期失败）"
echo "========================================"

echo "----------------------------------------"
echo "TEST: direction=3 (非法值，预期失败)"
echo "CMD: $SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM -T 2 -R 3"
$SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM -T 2 -R 3
if [ $? -ne 0 ]; then
    echo "RESULT: PASS (正确拒绝)"
    PASS=$((PASS + 1))
else
    echo "RESULT: FAIL (应当拒绝但未拒绝)"
    FAIL=$((FAIL + 1))
fi
echo "----------------------------------------"
echo ""

echo "----------------------------------------"
echo "TEST: thread-num=151 (超最大值，预期失败)"
echo "CMD: $SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM -T 151"
$SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM -T 151
if [ $? -ne 0 ]; then
    echo "RESULT: PASS (正确拒绝)"
    PASS=$((PASS + 1))
else
    echo "RESULT: FAIL (应当拒绝但未拒绝)"
    FAIL=$((FAIL + 1))
fi
echo "----------------------------------------"
echo ""

echo "----------------------------------------"
echo "TEST: src_stride=5M (超范围，预期失败)"
echo "CMD: $SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM -T 2 -S 5242880"
$SDMA_TOOL $CASE $SEND $RECV $CPU_NUM $DATA_SIZE $LOOP $MEM -T 2 -S 5242880
if [ $? -ne 0 ]; then
    echo "RESULT: PASS (正确拒绝)"
    PASS=$((PASS + 1))
else
    echo "RESULT: FAIL (应当拒绝但未拒绝)"
    FAIL=$((FAIL + 1))
fi
echo "----------------------------------------"
echo ""

# =============================================
# 汇总
# =============================================
echo "========================================"
echo " 验证完成"
echo " PASS: $PASS"
echo " FAIL: $FAIL"
echo "========================================"
