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

#define MAX_SIZE (50 * 1024 * 1024)
#define PROC_NUM 1
#define NUM 4
#define ALIGNMEMT 2097152
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
#define COOKIE_NUM (2 * NUM)
#define TIMEOUT 300000
#define FINISH_FLAG_ORIGIN 2
#define USLEEP_TIME 20
#define DIVIDEND 2

static long long unsigned int MAX_DATA_SIZE;
static int CPU0;
static int CPU1;
static int MEMORY_TYPE;
static int CPU_PER_NODE;
static int LOOP;

static int NUMA_NODE_NUMS = 8;

struct sdma_multi_th {
    int num;
    int cpu_num;
    bool *status;
    void *sdma;
    sdma_sqe_task_t *sqe_task;
    int loop_times;
    bool *g_barrier;
    sdma_request_t request;
};

static int sdma_test(void *sdma, sdma_sqe_task_t *sqe_task, unsigned int data_size, sdma_request_t *request)
{
    int ret;

    for (int i = 0; i < LOOP; i++) {
        ret = sdma_icopy_data(sdma, sqe_task, 1, request); /* only send 1 task every loop */
        if (ret != 0) {
            printf("sdma_icopy_data failed, ret = %d\n", ret);
            return SDMA_TEST_FAILED;
        }

        do {
            ret = sdma_iquery_chn(sdma, request);
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

static int ready_status_timeout_judgement(bool *ready)
{
    int cnt = 0;

    if (ready == NULL) {
        printf("detected NULL!\n");
    }

    while (!(*ready) && cnt < TIMEOUT) {
        usleep(USLEEP_TIME);
        ++cnt;
    }
    if (cnt == TIMEOUT) {
        return SDMA_TEST_FAILED;
    }

    return 0;
}

static int finish_status_judgement(int *finish_flag, int *process_ret)
{
    int cnt = 0;

    while ((*finish_flag) != 0 && cnt < TIMEOUT) {
        usleep(USLEEP_TIME);
        ++cnt;
    }
    if (cnt == TIMEOUT) {
        return SDMA_TEST_FAILED;
    }

    if ((*process_ret) != 0) {
        return SDMA_TEST_FAILED;
    }

    return 0;
}

static void *sdma_put_zcopy_thread(void *arg)
{
    struct sdma_multi_th *temp = (struct sdma_multi_th *)arg;
    cpu_set_t affinity;
    static int ret = 0;
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(temp->cpu_num, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) == SHM_ERR) {
        printf("warning: could not set CPU affinity, continuing...\n");
    }
    CPU_ZERO(&affinity);

    if (sched_getaffinity(0, sizeof(affinity), &affinity) == SHM_ERR) {
        printf("warning: cound not get Process affinity, continuing...\n");
    }

    temp->status[temp->num] = true;
    ret = ready_status_timeout_judgement(temp->g_barrier);
    if (ret != 0) {
        printf("wait temp->g_barrier timeout \n");
        return (void *)&ret;
    }

    ret = sdma_test(temp->sdma, temp->sqe_task, 0, &temp->request);
    if (ret != 0) {
        printf("sdma_test failed\n");
        return (void *)&ret;
    }
    return (void *)&ret;
}

static int sdma_mem_alloc(char **src_addr, char **dst_addr, int numa_id, int mmap_size)
{
    struct bitmask bitmask_src;
    struct bitmask bitmask_dst;
    nodemask_t nodemask_src;
    nodemask_t nodemask_dst;
     struct bitmask *bmp;
    int hbm_numa_id;
    int ret;

    bitmask_src.size = NUMA_NUM_NODES;
    bitmask_src.maskp = nodemask_src.n;
    bitmask_dst.size = NUMA_NUM_NODES;
    bitmask_dst.maskp = nodemask_dst.n;

    hbm_numa_id = numa_id + NUMA_NODE_NUMS / 2;

    *dst_addr = (char *)mmap(NULL, mmap_size, SDMA_PROT_FLAG, SDMA_MMAP_FLAG, -1, 0);
    if (*dst_addr == MAP_FAILED) {
        perror("mmap dst_addr");
        return SDMA_TEST_FAILED;
    }

    *src_addr = (char *)mmap(NULL, mmap_size, SDMA_PROT_FLAG, SDMA_MMAP_FLAG, -1, 0);
    if (*src_addr == MAP_FAILED) {
        perror("mmap src_addr failed");
        return SDMA_TEST_FAILED;
    }

    bmp = numa_bitmask_clearall(&bitmask_dst);
    if (bmp == NULL) {
        printf("numa_bitmask_clearall failed\n");
        return SDMA_TEST_FAILED;
    }

    bmp = numa_bitmask_clearall(&bitmask_src);
    if (bmp == NULL) {
        printf("numa_bitmask_clearall failed\n");
        return SDMA_TEST_FAILED;
    }

    switch (MEMORY_TYPE) {
        case MEMORY_TYPE_1:
            bmp = numa_bitmask_setbit(&bitmask_dst, numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            bmp = numa_bitmask_setbit(&bitmask_src, numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            break;
        case MEMORY_TYPE_2:
            bmp = numa_bitmask_setbit(&bitmask_dst, numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            bmp = numa_bitmask_setbit(&bitmask_src, hbm_numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            break;
        case MEMORY_TYPE_3:
            bmp = numa_bitmask_setbit(&bitmask_dst, hbm_numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            bmp = numa_bitmask_setbit(&bitmask_src, numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            break;
        case MEMORY_TYPE_4:
            bmp = numa_bitmask_setbit(&bitmask_dst, hbm_numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            bmp = numa_bitmask_setbit(&bitmask_src, hbm_numa_id);
            if (bmp == NULL) {
                printf("numa_bitmask_setbit failed\n");
                return SDMA_TEST_FAILED;
            }
            break;
        default:
            printf("unsupported memory type in binding memory!\n");
            return SDMA_TEST_FAILED;
    }

    ret = mbind(*dst_addr, mmap_size, MPOL_BIND, nodemask_dst.n, NUMA_NUM_NODES, 0);
    if (ret) {
        perror("mbind dst_addr failed");
        return SDMA_TEST_FAILED;
    }

    ret = mbind(*src_addr, mmap_size, MPOL_BIND, nodemask_src.n, NUMA_NUM_NODES, 0);
    if (ret) {
        perror("mbind src_addr failed");
        return SDMA_TEST_FAILED;
    }

    return 0;
}

static void sdma_mem_release(sdma_sqe_task_t *sqe_task, char *src_addr[], char *dst_addr[],
                             void *sdma[], int mmap_size)
{
    int k;

    free(sqe_task);

    for (k = 0; k < NUM; k++) {
        if (sdma[k]) {
            if (sdma_deinit_chn(sdma[k])) {
                printf("sdma_deinit_chn failed!\n");
            }
        }
    }

    switch (MEMORY_TYPE) {
        case MEMORY_TYPE_1:
        case MEMORY_TYPE_2:
        case MEMORY_TYPE_3:
        case MEMORY_TYPE_4:
            for (k = 0; k < NUM; k++) {
                if (!dst_addr[k]) {
                    break;
                }
                munmap(dst_addr[k], mmap_size);
                if (!src_addr[k]) {
                    break;
                }
                munmap(src_addr[k], mmap_size);
            }
            break;
        default:
            printf("unsupported memory type in unmapping memory!\n");
            return;
    }
}

static void release_share_mem(void *shm, int shmid)
{
    if (shm) {
        if (shmdt(shm) == SHM_ERR) {
            fprintf(stderr, "shmdt failed\n");
        }
        if (shmctl(shmid, IPC_RMID, 0) == SHM_ERR) {
            fprintf(stderr, "shmctl(IPC_RMID) failed\n");
        }
    }
}

static int open_share_mem(key_t key, void **share_mem)
{
    int shmid;

    shmid = shmget(key, sizeof(struct shared_use_st), RWCTL | IPC_CREAT);
    if (shmid < 0) {
        fprintf(stderr, "shmget failed\n");
        return SDMA_TEST_FAILED;
    }
    *share_mem = shmat(shmid, 0, 0);
    if (*share_mem == (void *)SHM_ERR) {
        fprintf(stderr, "shmat failed\n");
        return SDMA_TEST_FAILED;
    }

    return shmid;
}

static int put_recv(int fd, key_t key, int numa_id)
{
    int mmap_size = (MAX_DATA_SIZE - 1 + HUGEPAGE_SIZE) / HUGEPAGE_SIZE * HUGEPAGE_SIZE;
    struct sdma_multi_th pt_input[NUM] = {0};
    struct shared_use_st *shared = NULL;
    uint64_t cookie[COOKIE_NUM] = {0};
    sdma_sqe_task_t *sqe_task = NULL;
    char *recv_dst_addr[NUM] = {0};
    char *recv_src_addr[NUM] = {0};
    uint64_t send_dst_addr[NUM];
    int *pthread_ret[NUM] = {0};
    struct timeval start, end;
    uint32_t owner_process_id;
    pthread_t tid[NUM] = {0};
    bool g_barrier = false;
    bool status[NUM] = {0};
    void *sdma[NUM] = {0};
    int cookie_num = 0;
    void *shm = NULL;
    int shmid;
    int ret;
    int i;

    shmid = open_share_mem(key, &shm);
    if (shmid < 0 || shm == (void *)SHM_ERR) {
        printf("[recv] init share_mem failed\n");
        return SDMA_TEST_FAILED;
    }
    shared = (struct shared_use_st *)shm;
    sqe_task = calloc(NUM, sizeof(sdma_sqe_task_t));
    if (sqe_task == NULL) {
        printf("[recv] calloc sqe_task failed\n");
        goto release_share_mem;
    }

    for (i = 0; i < NUM; i++) {
        ret = sdma_mem_alloc(&recv_src_addr[i], &recv_dst_addr[i], numa_id, mmap_size);
        if (ret < 0) {
            printf("[recv] sdma_mem_alloc[%d] failed\n", i);
            goto release_mem_recv;
        }
    }

    for (i = 0; i < NUM; i++) {
        if (sdma_pin_umem(fd, recv_dst_addr[i], mmap_size, &cookie[i])) {
            printf("recv_dst_addr[%d] pin fail!\n ", i);
            goto unpin_mem_recv;
        }
        cookie_num++;
        if (sdma_pin_umem(fd, recv_src_addr[i], mmap_size, &cookie[i + NUM])) {
            printf("recv_src_addr[%d] pin fail!\n ", i);
            goto unpin_mem_recv;
        }
        cookie_num++;
    }

    ret = sdma_get_process_id(fd, &owner_process_id);
    if (ret != 0) {
        printf("[recv] get process_id failed\n");
        goto unpin_mem_recv;
    }

    ret = ready_status_timeout_judgement(&shared->submitter_pid_ready);
    if (ret != 0) {
        printf("[recv] wait submitter_pid_ready timeout \n");
        goto unpin_mem_recv;
    }

    for (i = 0; i < NUM; i++) {
        send_dst_addr[i] = shared->src_addr_list[i];
    }

    ret = sdma_add_authority(fd, &shared->submitter_process_id, 1);
    if (ret < 0) {
        printf("[recv] sdma_add_authority failed\n");
        goto unpin_mem_recv;
    }

    shared->owner_process_id = owner_process_id;
    for (i = 0; i < NUM; i++) {
        shared->dst_addr_list[i] = (uint64_t)(void *)recv_dst_addr[i];
    }
    shared->owner_pid_ready = true;

    for (i = 0; i < NUM; i++) {
        sdma[i] = sdma_init_chn(fd, i);
        if (sdma[i] == NULL) {
            printf("[recv] creat channel failed\n");
            goto unpin_mem_recv;
        }
    }

    for (i = 0; i < NUM; i++) {
        sqe_task[i].src_addr = send_dst_addr[i];
        sqe_task[i].dst_addr = (uint64_t)(void *)(recv_src_addr[i]);
        sqe_task[i].src_process_id = shared->submitter_process_id;
        sqe_task[i].dst_process_id = owner_process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = MAX_DATA_SIZE;
        sqe_task[i].opcode = OPCODE_COMMON_MODE;
        sqe_task[i].next_sqe = &sqe_task[i + 1];
        pt_input[i].num = i;
        pt_input[i].cpu_num = i + 1 + CPU1;
        pt_input[i].status = status;
        pt_input[i].sdma = sdma[i];
        pt_input[i].sqe_task = &sqe_task[i];
        pt_input[i].loop_times = LOOP;
        pt_input[i].g_barrier = &g_barrier;
    }

    shared->owner_task_ready = true;
    ret = ready_status_timeout_judgement(&shared->submitter_task_ready);
    if (ret != 0) {
        printf("[recv] wait submitter_task_ready timeout \n");
        goto unpin_mem_recv;
    }

    for (i = 0; i < NUM; i++) {
        pthread_create(&tid[i], NULL, sdma_put_zcopy_thread, &pt_input[i]);
    }

    for (i = 0; i < NUM; i++) {
        ret = ready_status_timeout_judgement(&status[i]);
        if (ret != 0) {
            printf("[recv] wait recv task ready timeout \n");
            goto unpin_mem_recv;
        }
    }

    shared->owner_grant_finish = true;
    ret = ready_status_timeout_judgement(&shared->submitter_grant_finish);
    if (ret != 0) {
        printf("[recv] wait submitter_grant_finish timeout \n");
        goto unpin_mem_recv;
    }
    gettimeofday(&start, NULL);
    g_barrier = true;
    for (i = 0; i < NUM; i++) {
        pthread_join(tid[i], (void**)&(pthread_ret[i]));
    }
    for (i = 0; i < NUM; i++) {
        if (pthread_ret[i]) {
            if (*(pthread_ret[i]) != 0) {
                printf("[recv] sdma_put_zcopy_thread execute failed!\n");
                goto unpin_mem_recv;
            }
        }
    }

    gettimeofday(&end, NULL);
    sdma_count_bw_latency(start, end, MAX_DATA_SIZE, LOOP, NUM);

    shared->recv_proc_ret = 0;
    shared->finish_flag--;
    ret = finish_status_judgement(&shared->finish_flag, &shared->send_proc_ret);
    if (ret != 0) {
        printf("[recv] proc failed due to send proc not finished or failed!\n");
    }

    for (i = 0; i < COOKIE_NUM; i++) {
        if (sdma_unpin_umem(fd, cookie[i])) {
            printf("[recv] unpin fail!\n");
            goto release_mem_recv;
        }
    }
    sdma_mem_release(sqe_task, recv_src_addr, recv_dst_addr, sdma, mmap_size);
    release_share_mem(shm, shmid);

    return ret;

unpin_mem_recv:
    for (i = 0; i < cookie_num; i++) {
        if (i % DIVIDEND == 0) {
            if (sdma_unpin_umem(fd, cookie[i / DIVIDEND])) {
                printf("[recv] unpin fail!\n");
            }
        } else {
            if (sdma_unpin_umem(fd, cookie[i / DIVIDEND + NUM])) {
                printf("[recv] unpin fail!\n");
            }
        }
    }
release_mem_recv:
    shared->recv_proc_ret = SDMA_TEST_FAILED;
    shared->finish_flag--;
    sdma_mem_release(sqe_task, recv_src_addr, recv_dst_addr, sdma, mmap_size);
release_share_mem:
    release_share_mem(shm, shmid);

    return SDMA_TEST_FAILED;
}

static int put_send(int fd, key_t key, int numa_id)
{
    int mmap_size = (MAX_DATA_SIZE - 1 + HUGEPAGE_SIZE) / HUGEPAGE_SIZE * HUGEPAGE_SIZE;
    struct sdma_multi_th pt_input[NUM] = {0};
    uint32_t dst_process_id, process_id;
    struct shared_use_st *shared = NULL;
    sdma_sqe_task_t *sqe_task = NULL;
    char *send_src_addr[NUM] = {0};
    char *send_dst_addr[NUM] = {0};
    uint64_t cookie[COOKIE_NUM];
    int *pthread_ret[NUM] = {0};
    struct timeval start, end;
    uint64_t dest[NUM] = {0};
    bool status[NUM] = {0};
    bool g_barrier = false;
    void *sdma[NUM] = {0};
    pthread_t tid[NUM];
    int cookie_num = 0;
    void *shm = NULL;
    int shmid;
    int ret;
    int i;

    shmid = open_share_mem(key, &shm);
    if (shmid < 0 || shm == (void *)SHM_ERR) {
        printf("[send] init share_mem failed\n");
        return SDMA_TEST_FAILED;
    }
    shared = (struct shared_use_st *)shm;
    shared->submitter_pid_ready = false;
    shared->owner_pid_ready = false;
    shared->owner_task_ready = false;
    shared->submitter_task_ready = false;
    shared->owner_grant_finish = false;
    shared->submitter_grant_finish = false;
    shared->finish_flag = FINISH_FLAG_ORIGIN;

    sqe_task = calloc(NUM, sizeof(sdma_sqe_task_t));
    if (sqe_task == NULL) {
        printf("[send] calloc sqe_task failed\n");
        goto unbind_share_mem;
    }

    for (i = 0; i < NUM; i++) {
        ret = sdma_mem_alloc(&send_src_addr[i], &send_dst_addr[i], numa_id, mmap_size);
        if (ret < 0) {
            printf("[send] sdma_mem_alloc[%d] failed\n", i);
            goto release_mem_send;
        }
    }

    for (i = 0; i < NUM; i++) {
        if (sdma_pin_umem(fd, send_src_addr[i], mmap_size, &cookie[i])) {
            printf("send_src_addr[%d] pin fail!\n ", i);
            goto unpin_mem_send;
        }
        cookie_num++;
        if (sdma_pin_umem(fd, send_dst_addr[i], mmap_size, &cookie[i + NUM])) {
            printf("send_dst_addr[%d] pin fail!\n ", i);
            goto unpin_mem_send;
        }
        cookie_num++;
    }

    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("[send] get process_id failed\n");
        goto unpin_mem_send;
    }

    for (i = 0; i < NUM; i++) {
        shared->src_addr_list[i] = (uint64_t)(void *)send_dst_addr[i];
    }
    shared->submitter_process_id = process_id;
    shared->submitter_pid_ready = true;

    ret = ready_status_timeout_judgement(&shared->owner_pid_ready);
    if (ret != 0) {
        printf("[send] wait owner_pid_ready timeout \n");
        goto unpin_mem_send;
    }

    for (i = 0; i < NUM; i++) {
        dest[i] = shared->dst_addr_list[i];
    }
    dst_process_id = shared->owner_process_id;

    ret = sdma_add_authority(fd, &dst_process_id, 1);
    if (ret < 0) {
        printf("[send] sdma_add_authority failed\n");
        goto unpin_mem_send;
    }

    for (i = 0; i < NUM; i++) {
        sdma[i] = sdma_init_chn(fd, NUM + i);
        if (sdma[i] == NULL) {
            printf("[send] creat channel failed\n");
            goto unpin_mem_send;
        }
    }

    for (i = 0; i < NUM; i++) {
        sqe_task[i].src_addr = dest[i];
        sqe_task[i].dst_addr = (uint64_t)(void *)(send_src_addr[i]);
        sqe_task[i].src_process_id = dst_process_id;
        sqe_task[i].dst_process_id = process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = MAX_DATA_SIZE;
        sqe_task[i].opcode = OPCODE_COMMON_MODE;
        sqe_task[i].next_sqe = &sqe_task[i + 1];
        pt_input[i].num = i;
        pt_input[i].cpu_num = i + 1 + CPU0;
        pt_input[i].status = status;
        pt_input[i].sdma = sdma[i];
        pt_input[i].sqe_task = &sqe_task[i];
        pt_input[i].loop_times = LOOP;
        pt_input[i].g_barrier = &g_barrier;
    }

    shared->submitter_task_ready = true;
    ret = ready_status_timeout_judgement(&shared->owner_task_ready);
    if (ret != 0) {
        printf("[send] wait owner_task_ready timeout \n");
        goto unpin_mem_send;
    }

    for (i = 0; i < NUM; i++) {
        pthread_create(&tid[i], NULL, sdma_put_zcopy_thread, &pt_input[i]);
    }

    for (i = 0; i < NUM; i++) {
        ret = ready_status_timeout_judgement(&status[i]);
        if (ret != 0) {
            printf("[send] wait send task ready timeout \n");
            goto unpin_mem_send;
        }
    }

    shared->submitter_grant_finish = true;
    ret = ready_status_timeout_judgement(&shared->owner_grant_finish);
    if (ret != 0) {
        printf("[send] wait owner_grant_finish timeout \n");
        goto unpin_mem_send;
    }

    gettimeofday(&start, NULL);
    g_barrier = true;

    for (i = 0; i < NUM; i++) {
        pthread_join(tid[i], (void**)&(pthread_ret[i]));
    }
    for (i = 0; i < NUM; i++) {
        if (pthread_ret[i]) {
            if (*(pthread_ret[i]) != 0) {
                printf("[send] sdma_put_zcopy_thread execute failed!\n");
                goto unpin_mem_send;
            }
        }
    }
    gettimeofday(&end, NULL);
    sdma_count_bw_latency(start, end, MAX_DATA_SIZE, LOOP, NUM);

    shared->send_proc_ret = 0;
    shared->finish_flag--;
    ret = finish_status_judgement(&shared->finish_flag, &shared->recv_proc_ret);
    if (ret != 0) {
        printf("[send] proc failed due to recv proc not finished or failed!\n");
    }

    for (i = 0; i < COOKIE_NUM; i++) {
        if (sdma_unpin_umem(fd, cookie[i])) {
            printf("[send] unpin fail!\n");
            goto release_mem_send;
        }
    }

    sdma_mem_release(sqe_task, send_src_addr, send_dst_addr, sdma, mmap_size);
    if (shmdt(shm) == SHM_ERR) {
        fprintf(stderr, "[send] shmdt failed\n");
    }

    return ret;

unpin_mem_send:
    for (i = 0; i < cookie_num; i++) {
        if (i % DIVIDEND == 0) {
            if (sdma_unpin_umem(fd, cookie[i / DIVIDEND])) {
                printf("[send] unpin fail!\n");
            }
        } else {
            if (sdma_unpin_umem(fd, cookie[i / DIVIDEND + NUM])) {
                printf("[send] unpin fail!\n");
            }
        }
    }
release_mem_send:
    shared->send_proc_ret = SDMA_TEST_FAILED;
    shared->finish_flag--;
    sdma_mem_release(sqe_task, send_src_addr, send_dst_addr, sdma, mmap_size);
unbind_share_mem:
    if (shmdt(shm) == SHM_ERR) {
        fprintf(stderr, "[send] shmdt failed\n");
    }

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
        printf("MAX_DATA_SIZE num wrong, please input num (1B-50MB)\n");
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
    }

    return 0;
}

static void case_bind_cpu_node(int cpu_id, int numa_id)
{
    struct bitmask *numa_mask = numa_allocate_cpumask();
    cpu_set_t affinity;
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) == SHM_ERR) {
        printf("warning: could not set CPU affinity, continuing...\n");
    }
    CPU_ZERO(&affinity);
    if (sched_getaffinity(0, sizeof(affinity), &affinity) == SHM_ERR) {
        printf("warning: cound not get Process affinity, continuing...\n");
    }

    fprintf(stdout, "child, current cpu num = %d, numa_node = %d\n", cpu_id, numa_id);
    numa_bitmask_setbit(numa_mask, numa_id);
    numa_set_bind_policy(MPOL_BIND);
    numa_set_membind(numa_mask);
    numa_bitmask_free(numa_mask);
}

int case_muti_direction(struct sdma_test_input *cmd)
{
    int device_num_1, device_num_2;
    char sdma_dev[DEV_LEN];
    int fd1, fd2, fd3, fd4;
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
        numa_id = CPU0 / CPU_PER_NODE;
        if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2)) {
            printf("numa id %d out of range, now set to 0...\n", numa_id);
            numa_id = 0;
        }

        case_bind_cpu_node(CPU0, numa_id);
        printf("I'm %d child, pid = %u, father pid is %u\n", i + 1, getpid(), getppid());
        printf("child, sdma device num = %d\n", device_num_1);
        sprintf(sdma_dev, "/dev/sdma%d", device_num_1);
        fd1 = open(sdma_dev, O_RDWR);
        if (fd1 < 0) {
            printf("open sdma%d failed!\n", device_num_1);
            return SDMA_TEST_FAILED;
        }

        sprintf(sdma_dev, "/dev/sdma%d", device_num_2);
        fd2 = open(sdma_dev, O_RDWR);
        if (fd2 < 0) {
            printf("open sdma%d failed!\n", device_num_2);
            return SDMA_TEST_FAILED;
        }

        ret = put_send(fd1, key, numa_id);
        if (ret < 0) {
            printf("sdma send proc failed\n");
        }

        close(fd1);
        close(fd2);
        _exit(0);
    } else {
        numa_id = CPU1 / CPU_PER_NODE;
        if (numa_id < 0 || numa_id >= (NUMA_NODE_NUMS / 2)) {
            printf("numa id %d out of range, now set to 0...\n", numa_id);
            numa_id = 0;
        }

        case_bind_cpu_node(CPU1, numa_id);
        printf("I'm parent, pid = %u\n", getpid());
        printf("parent, sdma device num = %d\n", device_num_2);
        sprintf(sdma_dev, "/dev/sdma%d", device_num_2);
        fd3 = open(sdma_dev, O_RDWR);
        if (fd3 < 0) {
            printf("open sdma%d failed!\n", device_num_2);
            return SDMA_TEST_FAILED;
        }

        sprintf(sdma_dev, "/dev/sdma%d", device_num_1);
        fd4 = open(sdma_dev, O_RDWR);
        if (fd4 < 0) {
            printf("open sdma%d failed!\n", device_num_1);
            return SDMA_TEST_FAILED;
        }

        ret = put_recv(fd3, key, numa_id);
        if (ret < 0) {
            printf("sdma recv proc failed\n");
        }

        close(fd3);
        close(fd4);
    }

    if (ret < 0) {
        return SDMA_TEST_FAILED;
    }

    return SDMA_TEST_SUCCESS;
}
