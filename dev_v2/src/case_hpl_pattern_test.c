#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <numaif.h>
#include <numa.h>

#define __USE_GNU
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "mdk_sdma.h"
#include "share_memory_test.h"
#include "ut_sdma.h"

#define CPU_NUM_PER_DIE_FOR_38CORESYS 152
#define CPU_NUM_PER_DIE_FOR_144CORESYS 144
#define PROC_NUM 3
#define PROC_1 0
#define PROC_2 1
#define PROC_3 2
#define PROC_4 3
#define ALIGNMEMT 2097152
#define MAX_LOOP_NUM 10000
#define CPU_NUM_PER_NODE_CASE1 38
#define CPU_NUM_PER_NODE_CASE2 144
#define CPU_NUM_PER_NODE_CASE3 152
#define DIE_ID_CASE1 0
#define DIE_ID_CASE2 1
#define DIE_ID_CASE3 2
#define DIE_ID_CASE4 3
#define PATTERN_CASE1 1
#define PATTERN_CASE2 2
#define PATTERN_CASE3 3
#define PATTERN_CASE4 4
#define COOKIE_NUM 2
#define COOKIE_SRC 0
#define COOKIE_DST 1

static int LOOP = 10000;
static int CPU_NUM_PER_NODE = 152;
static int DIE_ID = 0;
static int PATTERN = 1;

static int NUMA_NODE_NUMS = 8;
static long long unsigned int MAX_DATA_SIZE = 4194304;
static int SIZE = 1024;
static int NUM_STRIDE = 512;
static long long unsigned int STRIDE = 147456;
static int CPU1 = 0;
static int CPU2 = 152;
static int CPU3 = 304;
static int CPU4 = 456;

static void cpu_reset(int num)
{
    CPU1 += num;
    CPU2 += num;
    CPU3 += num;
    CPU4 += num;
}

static int change_cpu_sdmadev(int cpu_num_per_node, int die_id)
{
    switch (cpu_num_per_node) {
        case CPU_NUM_PER_NODE_CASE1:
            NUMA_NODE_NUMS = NUMA_NODES_NUMA_ENV;
            CPU1 = 0; /* node1 cpu id of 38-core-system: 0 */
            CPU2 = 38; /* node2 cpu id of 38-core-system: 38 */
            CPU3 = 76; /* node3 cpu id of 38-core-system: 76 */
            CPU4 = 114; /* node4 cpu id of 38-core-system: 114 */
            switch (die_id) {
                case DIE_ID_CASE1:
                    break;
                case DIE_ID_CASE2:
                    cpu_reset(CPU_NUM_PER_DIE_FOR_38CORESYS);
                    break;
                case DIE_ID_CASE3:
                    cpu_reset(CPU_NUM_PER_DIE_FOR_38CORESYS * DIE_ID_CASE3);
                    break;
                case DIE_ID_CASE4:
                    cpu_reset(CPU_NUM_PER_DIE_FOR_38CORESYS * DIE_ID_CASE4);
                    break;
                default:
                    return SDMA_TEST_FAILED;
            }
            break;
        case CPU_NUM_PER_NODE_CASE2:
            NUMA_NODE_NUMS = NUMA_NODES_NUMA_ENV;
            CPU1 = 0; /* node1 cpu id of 144-core-system: 0 */
            CPU2 = 36; /* node2 cpu id of 144-core-system: 36 */
            CPU3 = 72; /* node3 cpu id of 144-core-system: 72 */
            CPU4 = 108; /* node4 cpu id of 144-core-system: 108 */
            switch (die_id) {
                case DIE_ID_CASE1:
                    break;
                case DIE_ID_CASE2:
                    cpu_reset(CPU_NUM_PER_DIE_FOR_144CORESYS);
                    break;
                case DIE_ID_CASE3:
                    cpu_reset(CPU_NUM_PER_DIE_FOR_144CORESYS * DIE_ID_CASE3);
                    break;
                case DIE_ID_CASE4:
                    cpu_reset(CPU_NUM_PER_DIE_FOR_144CORESYS * DIE_ID_CASE4);
                    break;
                default:
                    return SDMA_TEST_FAILED;
            }
            break;
        case CPU_NUM_PER_NODE_CASE3:
            NUMA_NODE_NUMS = NUMA_NODES_UMA_ENV;
            CPU1 = 0; /* node1 cpu id of 152-core-system: 0 */
            CPU2 = 152; /* node2 cpu id of 152-core-system: 152 */
            CPU3 = 304; /* node3 cpu id of 152-core-system: 304 */
            CPU4 = 456; /* node4 cpu id of 152-core-system: 456 */
            break;
        default:
            return SDMA_TEST_FAILED;
    }
    return 0;
}

static int set_pattern(int PATTERN)
{
    switch (PATTERN) {
        case PATTERN_CASE1:
            SIZE = 1024; /* HPL MATRIX PATTERN 1 DATASIZE: 1024Byte */
            NUM_STRIDE = 512; /* HPL MATRIX PATTERN 1 STRIDE NUM: 512 */
            STRIDE = 147456; /* HPL MATRIX PATTERN 1 STRIDE BYTES: 147456Byte */
            break;
        case PATTERN_CASE2:
            SIZE = 1024; /* HPL MATRIX PATTERN 2 DATASIZE: 1024Byte */
            NUM_STRIDE = 4096; /* HPL MATRIX PATTERN 2 STRIDE NUM: 4096 */
            STRIDE = 147456; /* HPL MATRIX PATTERN 2 STRIDE BYTES: 147456Byte */
            break;
        case PATTERN_CASE3:
            SIZE = 1024; /* HPL MATRIX PATTERN 3 DATASIZE: 1024Byte */
            NUM_STRIDE = 4096; /* HPL MATRIX PATTERN 3 STRIDE NUM: 4096 */
            STRIDE = 147456; /* HPL MATRIX PATTERN 3 STRIDE BYTES: 147456Byte */
            break;
        case PATTERN_CASE4:
            SIZE = 4096; /* HPL MATRIX PATTERN 4 DATASIZE: 4096Byte */
            NUM_STRIDE = 4096; /* HPL MATRIX PATTERN 4 STRIDE NUM: 4096 */
            STRIDE = 393216; /* HPL MATRIX PATTERN 4 STRIDE BYTES: 393216Byte */
            break;
        default:
            return SDMA_TEST_FAILED;
    }
    return 0;
}

static int sdma_mem_alloc(char **dst_addr, char **src_addr, long long unsigned int mem,
                          int pattern, int numa_id)
{
    struct bitmask bitmask_src;
    struct bitmask bitmask_dst;
    nodemask_t nodemask_src;
    nodemask_t nodemask_dst;
    struct bitmask *bm_src;
    struct bitmask *bm_dst;
    int hbm_numa_id;
    int ret;

    bitmask_src.size = NUMA_NUM_NODES;
    bitmask_src.maskp = nodemask_src.n;
    bitmask_dst.size = NUMA_NUM_NODES;
    bitmask_dst.maskp = nodemask_dst.n;

    hbm_numa_id = numa_id + NUMA_NODE_NUMS / 2;

    *dst_addr = (char *)mmap(NULL, mem, SDMA_PROT_FLAG, SDMA_MMAP_FLAG, -1, 0);
    if (*dst_addr == MAP_FAILED) {
        perror("mmap dst_addr failed");
        return SDMA_TEST_FAILED;
    }

    *src_addr = (char *)mmap(NULL, mem, SDMA_PROT_FLAG, SDMA_MMAP_FLAG, -1, 0);
    if (*src_addr == MAP_FAILED) {
        perror("mmap src_addr failed");
        return SDMA_TEST_FAILED;
    }

    bm_dst = numa_bitmask_clearall(&bitmask_dst);
    if (bm_dst == NULL) {
        perror("numa_bitmask_clearall dst_addr failed");
        return SDMA_TEST_FAILED;
    }
    bm_src = numa_bitmask_clearall(&bitmask_src);
    if (bm_src == NULL) {
        perror("numa_bitmask_clearall src_addr failed");
        return SDMA_TEST_FAILED;
    }

    switch (pattern) {
        case PATTERN_CASE1:
            bm_dst = numa_bitmask_setbit(&bitmask_dst, numa_id);
            if (bm_dst == NULL) {
                perror("numa_bitmask_setbit dst_addr failed");
                return SDMA_TEST_FAILED;
            }
            bm_src = numa_bitmask_setbit(&bitmask_src, numa_id);
            if (bm_src == NULL) {
                perror("numa_bitmask_setbit src_addr failed");
                return SDMA_TEST_FAILED;
            }
            break;
        case PATTERN_CASE2:
            bm_dst = numa_bitmask_setbit(&bitmask_dst, numa_id);
            if (bm_dst == NULL) {
                perror("numa_bitmask_setbit dst_addr failed");
                return SDMA_TEST_FAILED;
            }
            bm_src = numa_bitmask_setbit(&bitmask_src, hbm_numa_id);
            if (bm_src == NULL) {
                perror("numa_bitmask_setbit src_addr failed");
                return SDMA_TEST_FAILED;
            }
            break;
        case PATTERN_CASE3:
            bm_dst = numa_bitmask_setbit(&bitmask_dst, hbm_numa_id);
            if (bm_dst == NULL) {
                perror("numa_bitmask_setbit dst_addr failed");
                return SDMA_TEST_FAILED;
            }
            bm_src = numa_bitmask_setbit(&bitmask_src, numa_id);
            if (bm_src == NULL) {
                perror("numa_bitmask_setbit src_addr failed");
                return SDMA_TEST_FAILED;
            }
            break;
        case PATTERN_CASE4:
            bm_dst = numa_bitmask_setbit(&bitmask_dst, hbm_numa_id);
            if (bm_dst == NULL) {
                perror("numa_bitmask_setbit dst_addr failed");
                return SDMA_TEST_FAILED;
            }
            bm_src = numa_bitmask_setbit(&bitmask_src, hbm_numa_id);
            if (bm_src == NULL) {
                perror("numa_bitmask_setbit src_addr failed");
                return SDMA_TEST_FAILED;
            }
            break;
        default:
            printf("invalid pattern type in allocing memory\n");
            return SDMA_TEST_FAILED;
    }

    ret = mbind(*dst_addr, mem, MPOL_BIND, nodemask_dst.n, NUMA_NUM_NODES, 0);
    if (ret) {
        perror("mbind dst_addr failed");
        return SDMA_TEST_FAILED;
    }

    ret = mbind(*src_addr, mem, MPOL_BIND, nodemask_src.n, NUMA_NUM_NODES, 0);
    if (ret) {
        perror("mbind src_addr failed");
        return SDMA_TEST_FAILED;
    }

    return 0;
}

static void sdma_mem_release(sdma_sqe_task_t *sqe_task, char *src_addr, char *dst_addr,
                             int pattern, void *sdma, long long unsigned int mem)
{
    if (sdma) {
        if (sdma_deinit_chn(sdma)) {
            printf("sdma_deinit_chn failed!\n");
        }
    }

    free(sqe_task);

    switch (pattern) {
        case PATTERN_CASE1:
        case PATTERN_CASE2:
        case PATTERN_CASE3:
        case PATTERN_CASE4:
            if (dst_addr) {
                munmap(dst_addr, mem);
            }
            if (src_addr) {
                munmap(src_addr, mem);
            }
            break;
        default:
            printf("invalid pattern type in releasing memory\n");
    }
}

static int sdma_process(int fd, long long unsigned int max_size, int loop_times, int num,
                        int pattern, int cpu_num, int numa_id)
{
    long long unsigned int mem = max_size, mmap_size;
    uint64_t cookie[COOKIE_NUM];
    struct timeval start, end;
    uint32_t owner_process_id;
    sdma_request_t request;
    char *dst_addr = NULL;
    char *src_addr = NULL;
    void *sdma = NULL;
    int ret;
    int i;

    sdma_sqe_task_t *sqe_task = calloc(1, sizeof(sdma_sqe_task_t));
    if (sqe_task == NULL) {
        printf("calloc sqe_task failed\n");
        return SDMA_TEST_FAILED;
    }

    mmap_size = (mem + HUGEPAGE_SIZE - 1) / HUGEPAGE_SIZE * HUGEPAGE_SIZE;

    ret = sdma_mem_alloc(&dst_addr, &src_addr, mmap_size, pattern, numa_id);
    if (ret < 0) {
        printf("sdma_mem_alloc failed\n");
        goto release_mem;
    }

    ret = sdma_pin_umem(fd, dst_addr, mmap_size, &cookie[COOKIE_DST]);
    if (ret < 0) {
        printf("dst_addr pin fail!\n ");
        goto release_mem;
    }

    ret = sdma_pin_umem(fd, src_addr, mmap_size, &cookie[COOKIE_SRC]);
    if (ret < 0) {
        printf("src_addr pin fail!\n ");
        goto unpin_dst_mem;
    }

    ret = sdma_get_process_id(fd, &owner_process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        goto unpin_mem;
    }

    sdma = sdma_init_chn(fd, num + 1);
    if (sdma == NULL) {
        printf("creat channel failed\n");
        goto unpin_mem;
    }

    sqe_task->src_addr = (uint64_t)(void *)(dst_addr);
    sqe_task->dst_addr = (uint64_t)(void *)(src_addr);
    sqe_task->src_process_id = owner_process_id;
    sqe_task->dst_process_id = owner_process_id;
    sqe_task->src_stride_len = STRIDE;
    sqe_task->dst_stride_len = STRIDE;
    sqe_task->stride_num = NUM_STRIDE;
    sqe_task->length = SIZE;
    sqe_task->opcode = OPCODE_COMMON_MODE;

    gettimeofday(&start, NULL);

    for (i = 0; i < loop_times; i++)
    {
        ret = sdma_icopy_data(sdma, sqe_task, 1, &request);
        if (ret != 0) {
            printf("sdma_icopy_data failed, ret = %d\n", ret);
            goto unpin_mem;
        }

        do{
            ret = sdma_iquery_chn(sdma, &request);
            if (ret == SDMA_RNDCNT_ERR) {
                continue;
            }
            else if (ret != 0) {
                printf("sdma_iquery_chn failed, ret = %d\n", ret);
                goto unpin_mem;
            }
        } while (ret != 0);
        
    }

    gettimeofday(&end, NULL);

    sdma_count_bw_latency(start, end, (uint64_t)(SIZE * NUM_STRIDE), loop_times, 1);

    if (sdma_unpin_umem(fd, cookie[COOKIE_SRC])) {
        printf("unpin src_addr fail!\n");
    }

    if (sdma_unpin_umem(fd, cookie[COOKIE_DST])) {
        printf("unpin dst_addr fail!\n");
    }

    sdma_mem_release(sqe_task, src_addr, dst_addr, pattern, sdma, mmap_size);

    return 0;

unpin_mem:
    if (sdma_unpin_umem(fd, cookie[COOKIE_SRC])) {
        printf("unpin src_addr fail!\n");
    }
unpin_dst_mem:
    if (sdma_unpin_umem(fd, cookie[COOKIE_DST])) {
        printf("unpin dst_addr fail!\n");
    }
release_mem:
    sdma_mem_release(sqe_task, src_addr, dst_addr, pattern, sdma, mmap_size);

    return SDMA_TEST_FAILED;
}

static void case_bind_cpu_node(int cpu_id, int numa_id)
{
    struct bitmask *numa_mask = numa_allocate_cpumask();
    cpu_set_t affinity;
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        printf("warning: could not set CPU affinity, continuing...\n");
    }
    CPU_ZERO(&affinity);
    if (sched_getaffinity(0, sizeof(affinity), &affinity) == -1) {
        printf("warning: cound not get Process affinity, continuing...\n");
    }

    fprintf(stdout, "child, current cpu num = %d, numa_node = %d\n", cpu_id, numa_id);
    numa_bitmask_setbit(numa_mask, numa_id);
    numa_set_bind_policy(MPOL_BIND);
    numa_set_membind(numa_mask);
    numa_bitmask_free(numa_mask);
}

static int check_input_params(struct sdma_test_input *cmd)
{
    LOOP = cmd->loop_times;
    printf("LOOP [%d]\n", LOOP);

    if (LOOP < 1 || LOOP > MAX_LOOP_NUM) {
        printf("LOOP param wrong, please input num (1-10000)\n");
        return SDMA_TEST_FAILED;
    }

    CPU_NUM_PER_NODE = cmd->num_of_cpu;
    printf("CPU NUM OF ONE NODE [%d]\n", CPU_NUM_PER_NODE);

    DIE_ID = cmd->die_id;
    printf("DIE ID [%d]\n", DIE_ID);

    PATTERN = cmd->hpl_pattern;
    printf("PATTERN [%d]\n", PATTERN);

    return 0;
}

int case_hpl_pattern_test(struct sdma_test_input *cmd)
{
    int device[PROC_NUM + 1] = {0};
    int cpu[PROC_NUM + 1] = {0};
    char sdma_dev[DEV_LEN];
    int numa_id;
    pid_t pid;
    int ret;
    int fd;
    int i;

    ret = check_input_params(cmd);
    if (ret < 0) {
        return SDMA_TEST_FAILED;
    }

    ret = change_cpu_sdmadev(CPU_NUM_PER_NODE, DIE_ID);
    if (ret < 0) {
        printf("wrong input num for CPU_NUM_PER_NODE(38/144/152) or DIE_ID(0-3)\n");
        return ret;
    }
    printf("change cpu and sdma_device sucess!\n");
    printf("cpu1[%d], cpu2[%d], cpu3[%d], cpu4[%d]\n", CPU1, CPU2, CPU3, CPU4);

    ret = set_pattern(PATTERN);
    if (ret < 0) {
        printf("wrong input num for PATTERN(1-4)\n");
        return ret;
    }
    printf("SIZE[%d], NUM_STRIDE[%d], STRIDE[%lld]\n", SIZE, NUM_STRIDE, STRIDE);

    MAX_DATA_SIZE = (SIZE + STRIDE) * NUM_STRIDE;

    for (i = 0; i < PROC_NUM; i++) {
        pid = fork(); 
        if (pid == 0) {
            break;
        }
    }

    switch (i) {
        case PROC_1:
            cpu[i] = CPU1;
            break;
        case PROC_2:
            cpu[i] = CPU2;
            break;
        case PROC_3:
            cpu[i] = CPU3;
            break;
        case PROC_4:
            cpu[i] = CPU4;
            break;
    }

    numa_id = cpu[i] / CPU_NUM_PER_NODE;
    if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2)) {
        printf("numa id %d out of range, now set to 0...\n", numa_id);
        numa_id = 0;
    }

    case_bind_cpu_node(cpu[i], numa_id);
    device[i] = sdma_nearest_id();
    if (device[i] < 0) {
        printf("sdma_nearest_id failed!\n");
        return SDMA_TEST_FAILED;
    }

    printf("I'm %d process, pid = %u, father pid is %u\n", i + 1, getpid(), getppid());
    printf("sdma get nearest id = %d\n", device[i]);

    sprintf(sdma_dev, "/dev/sdma%d", device[i]);
    fd = open(sdma_dev, O_RDWR);
    if (fd < 0) {
        printf("open sdma%d failed!\n", device[i]);
        return SDMA_TEST_FAILED;
    }

    ret = sdma_process(fd, MAX_DATA_SIZE, LOOP, i, PATTERN, cpu[i], numa_id);
    if (ret < 0) {
        close(fd);
        return SDMA_TEST_FAILED;
    }

    close(fd);

    return SDMA_TEST_SUCCESS;
}