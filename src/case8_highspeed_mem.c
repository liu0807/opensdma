#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <numa.h>
#include <numaif.h>
#include <sys/mman.h>

#define __USE_GNU
#include <sched.h>

#include "mdk_sdma.h"
#include "ut_sdma.h"

#define CPU_MAX             608
#define DATA_SIZE           (2 * 1024)
#define DIRECTION           4
#define CHN_NUM             1
#define COOKIE_NUM          2
#define SQE_NUM             1
#define COOKIE_SRC          0
#define COOKIE_DST          1

#define D2D 0
#define D2H 1
#define H2D 2
#define H2H 3

#define CPU_NUM_PER_NODE_CASE1 38
#define CPU_NUM_PER_NODE_CASE2 144
#define CPU_NUM_PER_NODE_CASE3 152

static int NUMA_NODE_NUMS = 32;
static int CPU_NUM_PER_NODE = 38;

static int copy_wait_data(int fd, int direction, sdma_sqe_task_t *sqe, int count)
{
    int ret = SDMA_TEST_FAILED;
    void *sdma = NULL;

    sdma = sdma_alloc_chn(fd);
    if (sdma == NULL) {
        printf("create channel failed\n");
        return ret;
    }

    ret = sdma_copy_data(sdma, sqe, count);
    if (ret != 0) {
        printf("sdma copy direction %d failed\n", direction);
        goto release;
    }
    ret = sdma_wait_chn(sdma, count);
    if (ret != 0) {
        printf("sdma wait direction %d failed\n", direction);
        goto release;
    }
    printf("sdma copy-wait direction %d success!\n", direction);
    if (sdma_free_chn(sdma)) {
        printf("sdma_free_chn failed\n");
    }

    return ret;

release:
    if (sdma_free_chn(sdma)) {
        printf("sdma_free_chn failed\n");
    }
    return ret;
}

static void sdma_release_memory(sdma_sqe_task_t *sqe_task, char *src_addr, char *dst_addr,
                                unsigned long mmap_size)
{
    if (src_addr) {
        munmap(src_addr, mmap_size);
    }

    if (dst_addr) {
        munmap(dst_addr, mmap_size);
    }
}

static int sdma_mem_alloc(char **dst_addr, char **src_addr, long long unsigned int mem,
                          int direction, int numa_id)
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

    switch (direction) {
        case D2D:
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
        case H2D:
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
        case D2H:
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
        case H2H:
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
            printf("invalid direction type in allocing memory\n");
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

static int sdma_process(int direction, int fd, int cpu_id, int numa_id)
{
    sdma_sqe_task_t sqe_task[SQE_NUM + 1] = {0};
    unsigned long cookie[COOKIE_NUM] = {0};
    char *dst_addr = NULL;
    char *src_addr = NULL;
    uint32_t process_id;
    uint64_t mmap_size;
    int ret;

    mmap_size = (DATA_SIZE - 1 + HUGEPAGE_SIZE) / HUGEPAGE_SIZE * HUGEPAGE_SIZE;
    ret = sdma_mem_alloc(&dst_addr, &src_addr, mmap_size, direction, numa_id);
    if (ret != 0) {
        goto free_memory;
    }

    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        goto free_memory;
    }

    fill_sdma_task(sqe_task, SQE_NUM, CHN_NUM, DATA_SIZE, process_id, src_addr, dst_addr);
    ret = sdma_pin_umem(fd, src_addr, mmap_size, &cookie[COOKIE_SRC]);
    if (ret != 0) {
        printf("sdma_pin_umem src failed, ret = %d\n", ret);
        goto free_memory;
    }
    ret = sdma_pin_umem(fd, dst_addr, mmap_size, &cookie[COOKIE_DST]);
    if (ret != 0) {
        printf("sdma_pin_umem dst failed, ret = %d\n", ret);
        goto unpin_src;
    }

    ret = copy_wait_data(fd, direction, sqe_task, SQE_NUM);
    if (ret != 0) {
        goto unpin_dst;
    }

unpin_dst:
    if (sdma_unpin_umem(fd, cookie[COOKIE_DST])) {
        printf("sdma_unpin_umem dst_addr failed\n");
    }
unpin_src:
    if (sdma_unpin_umem(fd, cookie[COOKIE_SRC])) {
        printf("sdma_unpin_umem src_addr failed\n");
    }
free_memory:
    sdma_release_memory(sqe_task, src_addr, dst_addr, mmap_size);

    return ret;
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

    fprintf(stdout, "current cpu num = %d, numa_node = %d\n", cpu_id, numa_id);
    numa_bitmask_setbit(numa_mask, numa_id);
    numa_set_bind_policy(MPOL_BIND);
    numa_set_membind(numa_mask);
    numa_bitmask_free(numa_mask);
}

int case8_highspeed_mem(struct sdma_test_input *cmd)
{
    char sdma_dev[DEV_LEN] = {0};
    int direction;
    int ret, id;
    int numa_id;
    int cpu_id;
    int fd;

    CPU_NUM_PER_NODE = cmd->num_of_cpu;
    if (CPU_NUM_PER_NODE != CPU_NUM_PER_NODE_CASE1 && CPU_NUM_PER_NODE != CPU_NUM_PER_NODE_CASE2 &&
        CPU_NUM_PER_NODE != CPU_NUM_PER_NODE_CASE3) {
        printf("invalid cpu num per node, please input (38/144/152)\n");
        return SDMA_TEST_FAILED;
    }

    id = sdma_nearest_id();
    if (id < 0) {
        printf("sdma_nearest_id fail!, ret = %d\n", id);
        return id;
    }
    sprintf(sdma_dev, "/dev/sdma%d", id);

    fd = open(sdma_dev, O_RDWR);
    if (fd < 0) {
        printf("open sdma0 fail!\n");
        return SDMA_TEST_FAILED;
    }

    cpu_id = sched_getcpu();
    if (cpu_id < 0 || cpu_id >= CPU_MAX) {
        printf("invalid cpu number, now set to 0...\n");
        cpu_id = 0;
    }

    numa_id = cpu_id / CPU_NUM_PER_NODE;
    if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2)) {
        printf("invalid numa number, now set to 0...\n");
        numa_id = 0;
    }

    case_bind_cpu_node(cpu_id, numa_id);

    for (direction = 0; direction < DIRECTION; direction++) {
        ret = sdma_process(direction, fd, cpu_id, numa_id);
        if (ret != 0) {
            printf("task direction: %d fail\n", direction);
        }
    }

    close(fd);

    return ret;
}