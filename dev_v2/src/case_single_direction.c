#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/time.h>
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

#define ALIGNMEMT 2097152
#define MAX_SIZE (1 * 1024 * 1024 * 1024)
#define PROC_NUM 1
#define MAX_LOOP_NUM 2000
#define CPU_NUM_PER_NODE_CASE1 38
#define CPU_NUM_PER_NODE_CASE2 144
#define CPU_NUM_PER_NODE_CASE3 152
#define MEMORY_TYPE_1 1
#define MEMORY_TYPE_2 2
#define MEMORY_TYPE_3 3
#define MEMORY_TYPE_4 4
#define NODE_PER_DIE_38CORESYS 4
#define US_PER_SEC 1000000L
#define MS_PER_SEC 1000
#define US_PER_MS 1000
#define BYTE_PER_KBYTE 1024
#define KBYTE_PER_MBYTE 1024
#define TIMEOUT 10

static long long unsigned int MAX_DATA_SIZE = 4194304;
static int CPU0 = 0;
static int CPU1 = 1;
static int MEMORY_TYPE = 1;
static int CPU_PER_NODE = 38;
static int LOOP = 100;

static int NUMA_NODE_NUMS = 8;

static int sdma_test(void *sdma, sdma_sqe_task_t *sqe_task, unsigned int data_size, int loop_times)
{
    sdma_request_t request;
    int ret;

    for (int i = 0; i < LOOP; i++) {
        ret = sdma_icopy_data(sdma, sqe_task, 1, &request);
        if (ret != 0) {
            printf("sdma_icopy_data failed, ret = %d\n", ret);
            return SDMA_TEST_FAILED;
        }

        do {
            ret = sdma_iquery_chn(sdma, &request);
            if (ret == SDMA_RNDCNT_ERR) {
                continue;
            } else if (ret != 0) {
                printf("sdma_iquery_chn failed!!, ret = %d\n", ret);
                return SDMA_TEST_FAILED;
            }
        } while (ret != 0);
    }

    return 0;
}

static void sdma_mem_release_recv(char *addr, unsigned long mmap_size)
{
    switch (MEMORY_TYPE) {
        case MEMORY_TYPE_1:
        case MEMORY_TYPE_2:
        case MEMORY_TYPE_3:
        case MEMORY_TYPE_4:
            munmap(addr, mmap_size);
            break;
        default:
            printf("invalid memory type of releasing recv process mem\n");
    }
}

static void sdma_mem_release_send(sdma_sqe_task_t *sqe_task, void *sdma, void *shm, char *addr,
                                  int mmap_size)
{
    free(sqe_task);

    if (shm) {
        if (shmdt(shm) == SHM_ERR) {
            fprintf(stderr, "shmdt failed\n");
        }
    }

    if (sdma) {
        if (sdma_deinit_chn(sdma) != 0) {
            fprintf(stderr, "sdma_deinit_chn failed\n");
        }
    }

    switch (MEMORY_TYPE) {
        case MEMORY_TYPE_1:
        case MEMORY_TYPE_2:
        case MEMORY_TYPE_3:
        case MEMORY_TYPE_4:
            munmap(addr, mmap_size);
            break;
        default:
            printf("invalid memory type of releasing send process mem\n");
    }
}

void release_share_mem(void *shm, int shmid)
{
    if (shm) {
        if (shmdt(shm) == SHM_ERR) {
            fprintf(stderr, "shmdt failed\n");
        }
        if (shmctl(shmid, IPC_RMID, NULL) == SHM_ERR) {
            fprintf(stderr, "shmctl(IPC_RMID) failed\n");
        }
    }
}

static int put_recv(int fd, key_t key, int numa_id)
{
    struct shared_use_st *shared;
    int bind_numa_id = numa_id;
    uint32_t owner_process_id;
    unsigned long mmap_size;
    struct bitmask bitmask;
    struct bitmask *bmp;
    nodemask_t nodemask;
    void *shm = NULL;
    uint64_t cookie;
    char *dst_addr;
    int shmid;
    int ret;
    int cnt;

    shmid = shmget(key, sizeof(struct shared_use_st), RWCTL | IPC_CREAT);
    if (shmid < 0) {
        fprintf(stderr, "shmget failed\n");
        return SDMA_TEST_FAILED;
    }
    shm = shmat(shmid, 0, 0);
    if (shm == (void *)SHM_ERR) {
        fprintf(stderr, "shmat failed\n");
        return SDMA_TEST_FAILED;
    }

    shared = (struct shared_use_st *)shm;
    shared->owner_grant_finish = false;
    shared->submitter_pid_ready = false;

    mmap_size = (MAX_DATA_SIZE + HUGEPAGE_SIZE - 1) / HUGEPAGE_SIZE * HUGEPAGE_SIZE;
    bitmask.size = NUMA_NUM_NODES;
    bitmask.maskp = nodemask.n;

    dst_addr = mmap(NULL, mmap_size, SDMA_PROT_FLAG, SDMA_MMAP_FLAG, -1, 0);
    if (dst_addr == MAP_FAILED) {
        perror("mmap dst_addr");
        goto release_shm;
    }

    switch (MEMORY_TYPE) {
        case MEMORY_TYPE_1:
        case MEMORY_TYPE_2:
            break;
        case MEMORY_TYPE_3:
        case MEMORY_TYPE_4:
            bind_numa_id += NUMA_NODE_NUMS / 2;
            break;
        default:
            printf("invalid memory type of binding recv process mem\n");
            goto release_mem_recv;
    }

    bmp = numa_bitmask_clearall(&bitmask);
    if (bmp == NULL) {
        printf("numa_bitmask_clearall failed\n");
    }
    bmp = numa_bitmask_setbit(&bitmask, bind_numa_id);
    if (bmp == NULL) {
        printf("numa_bitmask_clearall failed\n");
    }

    ret = mbind(dst_addr, mmap_size, MPOL_BIND, nodemask.n, NUMA_NUM_NODES, 0);
    if (ret != 0) {
        perror("mbind dst_addr");
        goto release_mem_recv;
    }

    if (sdma_pin_umem(fd, dst_addr, MAX_DATA_SIZE, &cookie)) {
        printf("src_addr pin fail!\n ");
        goto release_mem_recv;
    }

    ret = sdma_get_process_id(fd, &owner_process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        goto unpin_mem_recv;
    }

    cnt = 0;
    while (!shared->submitter_pid_ready && cnt < TIMEOUT) {
        printf("[recv] wait submitter pid ready..\n");
        sleep(1);
        ++cnt;
    }

    if (cnt == TIMEOUT) {
        printf("[recv] wait timeout \n");
        goto unpin_mem_recv;
    }

    ret = sdma_add_authority(fd, &shared->submitter_process_id, 1);
    if (ret < 0) {
        printf("sdma_add_authority failed\n");
        goto unpin_mem_recv;
    }

    shared->readen = 0;
    shared->owner_process_id = owner_process_id;
    shared->dst_addr = (uint64_t)(void *)dst_addr;
    shared->owner_grant_finish = true;

    while (shared->readen == 0) {
        printf("[recv] shared->readen == 0,  sleep\n");
        sleep(2);
    }

    if (shared->send_proc_ret != 0) {
        printf("recv proc also failed due to send proc failed!\n");
        goto unpin_mem_recv;
    }

    if (sdma_unpin_umem(fd, cookie)) {
        printf("src_addr unpin fail!\n");
        goto release_mem_recv;
    }
    sdma_mem_release_recv(dst_addr, mmap_size);
    release_share_mem(shm, shmid);

    return 0;

unpin_mem_recv:
    if (sdma_unpin_umem(fd, cookie)) {
        printf("src_addr unpin fail!\n");
    }
release_mem_recv:
    sdma_mem_release_recv(dst_addr, mmap_size);
release_shm:
    release_share_mem(shm, shmid);

    return SDMA_TEST_FAILED;
}

static int put_send(int fd, key_t key, int numa_id)
{
    uint32_t dst_process_id, process_id;
    int bind_numa_id = numa_id;
    struct timeval start, end;
    unsigned long mmap_size;
    struct bitmask bitmask;
    struct bitmask *bmp;
    nodemask_t nodemask;
    void *sdma = NULL;
    uint64_t cookie;
    uint64_t dest;
    char *src;
    int cnt;
    int ret;

    sdma_sqe_task_t *sqe_task = calloc(1, sizeof(sdma_sqe_task_t));
    if (sqe_task == NULL) {
        printf("calloc sqe_task failed\n");
        return SDMA_TEST_FAILED;
    }

    void *shm = NULL;
    struct shared_use_st *shared = NULL;
    int shmid;

    shmid = shmget(key, sizeof(struct shared_use_st), RWCTL | IPC_CREAT);
    if (shmid < 0) {
        free(sqe_task);
        perror("shmget");
        return SDMA_TEST_FAILED;
    }

    shm = shmat(shmid, (void *)0, 0);
    if (shm == (void *)SHM_ERR) {
        free(sqe_task);
        fprintf(stderr, "shmat failed\n");
        return SDMA_TEST_FAILED;
    }

    shared = (struct shared_use_st *)shm;
    shared->submitter_pid_ready = false;
    shared->owner_grant_finish = false;

    mmap_size = (MAX_DATA_SIZE + HUGEPAGE_SIZE - 1) / HUGEPAGE_SIZE * HUGEPAGE_SIZE;
    bitmask.size = NUMA_NUM_NODES;
    bitmask.maskp = nodemask.n;

    src = mmap(NULL, mmap_size, SDMA_PROT_FLAG, SDMA_MMAP_FLAG, -1, 0);
    if (src == MAP_FAILED) {
        perror("mmap src_addr");
        goto release_mem_send;
    }

    switch (MEMORY_TYPE) {
        case MEMORY_TYPE_1:
        case MEMORY_TYPE_3:
            break;
        case MEMORY_TYPE_2:
        case MEMORY_TYPE_4:
            bind_numa_id += NUMA_NODE_NUMS / 2;
            break;
        default:
            printf("invalid memory type of binding send process mem\n");
            goto release_mem_send;
    }

    bmp = numa_bitmask_clearall(&bitmask);
    if (bmp == NULL) {
        printf("numa_bitmask_clearall failed\n");
    }

    bmp = numa_bitmask_setbit(&bitmask, bind_numa_id);
    if (bmp == NULL) {
        printf("numa_bitmask_setbit failed\n");
    }

    ret = mbind(src, mmap_size, MPOL_BIND, nodemask.n, NUMA_NUM_NODES, 0);
    if (ret != 0) {
        perror("mbind src_addr");
        goto release_mem_send;
    }

    if (sdma_pin_umem(fd, src, MAX_DATA_SIZE, &cookie)) {
        printf("src_addr pin fail!\n ");
        goto release_mem_send;
    }

    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        goto unpin_mem_send;
    }

    shared->submitter_process_id = process_id;
    shared->submitter_pid_ready = true;

    cnt = 0;
    while (!shared->owner_grant_finish && cnt < TIMEOUT) {
        sleep(1);
        ++cnt;
        printf("[send] wait owner grant aothurity..\n");
        shared->submitter_pid_ready = true;
    }

    if (cnt == TIMEOUT) {
        printf("[send] wait timeout \n");
        goto unpin_mem_send;
    }

    dest = shared->dst_addr;
    dst_process_id = shared->owner_process_id;

    sdma = sdma_init_chn(fd, 0);
    if (sdma == NULL) {
        printf("creat channl failed\n");
        goto unpin_mem_send;
    }

    sqe_task->src_addr = dest;
    sqe_task->dst_addr = (unsigned long)(void *)(src);
    sqe_task->src_process_id = dst_process_id;
    sqe_task->dst_process_id = process_id;
    sqe_task->src_stride_len = 0;
    sqe_task->dst_stride_len = 0;
    sqe_task->stride_num = 0;
    sqe_task->length = MAX_DATA_SIZE;
    sqe_task->opcode = OPCODE_COMMON_MODE;

    gettimeofday(&start, NULL);

    ret = sdma_test(sdma, sqe_task, MAX_DATA_SIZE, LOOP);
    if (ret != 0) {
        printf("sdma_test failed!\n");
        shared->send_proc_ret = ret;
        shared->readen = 1;
        goto unpin_mem_send;
    }

    gettimeofday(&end, NULL);

    sdma_count_bw_latency(start, end, MAX_DATA_SIZE, LOOP, 1);

    shared->send_proc_ret = ret;
    shared->readen = 1;

    if (sdma_unpin_umem(fd, cookie)) {
        printf("src_addr unpin fail!\n");

        goto release_mem_send;
    }
    sdma_mem_release_send(sqe_task, sdma, shm, src, mmap_size);

    return 0;

unpin_mem_send:
    if (sdma_unpin_umem(fd, cookie)) {
        printf("src_addr unpin fail!\n");
    }
release_mem_send:
    sdma_mem_release_send(sqe_task, sdma, shm, src, mmap_size);

    return SDMA_TEST_FAILED;
}


static int case_get_input(struct sdma_test_input *cmd, int *dev1, int *dev2)
{
    MAX_DATA_SIZE = cmd->data_size;
    CPU0 = cmd->send_cpu;
    CPU1 = cmd->recv_cpu;
    CPU_PER_NODE = cmd->num_of_cpu;
    MEMORY_TYPE = cmd->memory_type;
    LOOP = cmd->loop_times;
    printf("MAX data length = %llu\n", MAX_DATA_SIZE);
    printf("CUP CORE OF SEND PROC [%d]\n", CPU0);
    printf("CUP CORE OF RECV PROC [%d]\n", CPU1);
    printf("CPU_PER_NODE [%d]\n", CPU_PER_NODE);
    printf("MEMORY_TYPE [%d]\n", MEMORY_TYPE);
    printf("LOOP [%d]\n", LOOP);
    if (MAX_DATA_SIZE < 1 || MAX_DATA_SIZE > MAX_SIZE) {
        printf("MAX_DATA_SIZE num wrong, please input num (1B-1GB)\n");
        return SDMA_TEST_FAILED;
    }

    if (CPU_PER_NODE != CPU_NUM_PER_NODE_CASE1 && 
        CPU_PER_NODE != CPU_NUM_PER_NODE_CASE2 && 
        CPU_PER_NODE != CPU_NUM_PER_NODE_CASE3) {
        printf("wrong CPU_PER_NODE, please input 38/144/152\n");
        return SDMA_TEST_FAILED;
    }

    if (MEMORY_TYPE < MEMORY_TYPE_1 || MEMORY_TYPE > MEMORY_TYPE_4) {
        printf("wrong MEMORY_TYPE num, please input num (1-4)\n");
        return SDMA_TEST_FAILED;
    }

    if (LOOP < 1 || LOOP > MAX_LOOP_NUM) {
        printf("LOOP num wrong, please input num (1-2000)\n");
        return SDMA_TEST_FAILED;
    }

    *dev1 = CPU0 / CPU_PER_NODE;
    *dev2 = CPU1 / CPU_PER_NODE;
    switch (CPU_PER_NODE) {
        case CPU_NUM_PER_NODE_CASE1:
            NUMA_NODE_NUMS = NUMA_NODES_NUMA_ENV;
            (*dev1) /= NODE_PER_DIE_38CORESYS;
            (*dev2) /= NODE_PER_DIE_38CORESYS;
            break;
        case CPU_NUM_PER_NODE_CASE2:
            NUMA_NODE_NUMS = NUMA_NODES_NUMA_ENV;
            break;
        case CPU_NUM_PER_NODE_CASE3:
            NUMA_NODE_NUMS = NUMA_NODES_UMA_ENV;
            break;
        default:
            printf("wrong CPU_PER_NODE, please input 38/144/152\n");
            return SDMA_TEST_FAILED;
    }

    return 0;
}

static void case_bind_cpu_node(int cpu_id, int numa_node)
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

    numa_bitmask_setbit(numa_mask, numa_node);
    numa_set_bind_policy(MPOL_BIND);
    numa_set_membind(numa_mask);
    numa_bitmask_free(numa_mask);
}

int case_single_direction(struct sdma_test_input *cmd)
{
    int device_num_1, device_num_2;
    int fd1, fd2, fd3, fd4;
    uint32_t cpu0, cpu1;
    char sdma_dev[20];
    int numa_id;
    pid_t pid;
    key_t key;
    int ret;
    int i;

    ret = case_get_input(cmd, &device_num_1, &device_num_2);
    if (ret != 0) {
        return SDMA_TEST_FAILED;
    }
    srandom(getpid());
    key = random();

    for (i = 0; i < PROC_NUM; i++) {
        pid = fork();
        if (pid == 0) {
            break;
        }
    }

    if (i < PROC_NUM) {
        cpu0 = CPU0;
        numa_id = cpu0 / CPU_PER_NODE;
        if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2)) {
            printf("numa id %d out of range, now set to 0...\n", numa_id);
            numa_id = 0;
        }

        case_bind_cpu_node(cpu0, numa_id);
        printf("I'm %d child, pid = %u, father pid is %u\n", i + 1, getpid(), getppid());
        printf("child, sdma device num = %d\n", device_num_1);
        printf("current cpu num = %d, numa_node = %d\n", cpu0, numa_id);

        sprintf(sdma_dev, "/dev/sdma%d", device_num_1);
        fd1 = open(sdma_dev, O_RDWR);
        if (fd1 < 0) {
            printf("open sdma%d failed!\n", device_num_1);
            return SDMA_TEST_FAILED;
        }

        sprintf(sdma_dev, "/dev/sdma%d", device_num_2);
        fd2 = open(sdma_dev, O_RDWR);
        if (fd2 < 0) {
            close(fd1);
            printf("open sdma%d failed!\n", device_num_2);
            return SDMA_TEST_FAILED;
        }

        ret = put_send(fd1, key, numa_id);

        close(fd1);
        close(fd2);

        printf("child process exit...\n");
        _exit(0);
    } else {
        cpu1 = CPU1;
        numa_id = cpu1 / CPU_PER_NODE;
        if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2)) {
            printf("numa id %d out of range, now set to 0...\n", numa_id);
            numa_id = 0;
        }
        case_bind_cpu_node(cpu1, numa_id);
        printf("I'm parent, pid = %u\n", getpid());
        printf("parent, sdma device num = %d\n", device_num_2);
        printf("current cpu num = %d, numa_node = %d\n", cpu1, numa_id);

        sprintf(sdma_dev, "/dev/sdma%d", device_num_2);
        fd3 = open(sdma_dev, O_RDWR);
        if (fd3 < 0) {
            printf("open sdma%d failed!\n", device_num_2);
            return SDMA_TEST_FAILED;
        }

        sprintf(sdma_dev, "/dev/sdma%d", device_num_1);
        fd4 = open(sdma_dev, O_RDWR);
        if (fd4 < 0) {
            close(fd3);
            printf("open sdma%d failed!\n", device_num_1);
            return SDMA_TEST_FAILED;
        }

        ret = put_recv(fd3, key, numa_id);

        close(fd3);
        close(fd4);

        printf("father process exit..\n");
    }

    if (ret < 0) {
        return SDMA_TEST_FAILED;
    } else {
        return SDMA_TEST_SUCCESS;
    }
}
