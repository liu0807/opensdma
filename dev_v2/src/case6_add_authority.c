#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/shm.h>
#include "mdk_sdma.h"
#include "share_memory_test.h"
#include "ut_sdma.h"

#define DATA_SIZE 9000
#define REQUEST_NUM 10
#define PROC_NUM 1
#define CHN_IDX 0
#define CNT_MAX 10
#define CNT 1

static int fd0 = -1;
static int fd1 = -1;

static int sdma_isend_test(sdma_sqe_task_t *sqe_task, uint32_t process_id, int dst_process_id,
                           char *src, uint64_t dest)
{
    sdma_request_t request[REQUEST_NUM] = {0};
    void *sdma = NULL;
    int i, ret;

    for (i = 0; i < REQUEST_NUM; i++) {
        sqe_task[i].src_addr = (uint64_t)(void *)(src);
        sqe_task[i].dst_addr = (dest);
        sqe_task[i].src_process_id = process_id;
        sqe_task[i].dst_process_id = dst_process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = DATA_SIZE;
    }

    sdma = sdma_init_chn(fd0, CHN_IDX);
    if (sdma == NULL) {
        printf("creat share channel failed\n");
        return SDMA_TEST_FAILED;
    }

    printf("sdma start icopy data\n");
    ret = sdma_icopy_data(sdma, sqe_task, CNT, &request[0]);
    if (ret != 0) {
        printf("sdma_icopy_data failed\n");
        goto release;
    }

    ret = sdma_iwait_chn(sdma, &request[0]);
    if (ret != 0) {
        printf("sdma_iwait_chn failed\n");
        goto release;
    }
    printf("sdma iwait success!\n");

    if (sdma_deinit_chn(sdma)) {
        printf("sdma_deinit_chn failed\n");
    }

    return ret;

release:
    if (sdma_deinit_chn(sdma)) {
        printf("sdma_deinit_chn failed\n");
    }

    return SDMA_TEST_FAILED;
}

int sdma_recv_add_authority(int key, char *dst_addr)
{
    struct shared_use_st *shared = NULL;
    uint32_t owner_process_id;
    void *shm = NULL;
    int shmid, ret;
    int cnt = 0;

    ret = sdma_get_process_id(fd1, &owner_process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        return ret;
    }

    shmid = shmget((key_t)key, sizeof(struct shared_use_st), RWCTL | IPC_CREAT);
    if (shmid == -1) {
        fprintf(stderr, "shmget failed\n");
        return shmid;
    }

    shm = shmat(shmid, 0, 0);
    if (shm == (void*)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    shared = (struct shared_use_st*)shm;
    shared->readen = 0;
    shared->owner_process_id = owner_process_id;
    shared->dst_addr = (unsigned long)(void *)dst_addr;
    sleep(1);
    shared->owner_use = 0;

    ret = sdma_add_authority(fd1, &shared->submitter_process_id, 1);
    if (ret < 0) {
        printf("add_authority failed\n");
        return ret;
    }
    while (shared->readen == 0) {
        sleep(1);
        cnt++;
        if (cnt >= CNT_MAX) {
            exit(EXIT_FAILURE);
        }
    }

    if (shmdt(shm) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }

    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "shmctl(IPC_RMID) failed\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int sdma_send_add_authority(int key, char *src_addr)
{
    sdma_sqe_task_t sqe_task[REQUEST_NUM + 1] = {0};
    struct shared_use_st *shared = NULL;
    int shmid, ret, dst_process_id;
    uint32_t process_id;
    void *shm = NULL;
    int running = 1;
    uint64_t dest;
    int cnt = 0;

    ret = sdma_get_process_id(fd0, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        return ret;
    }

    shmid = shmget((key_t)key, sizeof(struct shared_use_st), RWCTL | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    shm = shmat(shmid, (void*)0, 0);
    if (shm == (void*)SHM_ERR) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    shared = (struct shared_use_st*)shm;
    shared->submitter_process_id = process_id;
    shared->owner_use = 1;

    while (running) {
        while (shared->owner_use == 1) {
            sleep(1);
            printf("sdma_send Waiting...\n");
            cnt++;
            if (cnt >= CNT_MAX) {
                exit(EXIT_FAILURE);
            }
        }
        dest = shared->dst_addr;
        dst_process_id = shared->owner_process_id;

        ret = sdma_isend_test(sqe_task, process_id, dst_process_id, src_addr, dest);
        shared->readen = 1;
        running = 0;
        if (ret < 0) {
            printf("sdma_isend_test failed!\n");
            if (shmdt(shm) == SHM_ERR) {
                fprintf(stderr, "shmdt failed\n");
                exit(EXIT_FAILURE);
            }
            return ret;
        }
    }
    if (shmdt(shm) == SHM_ERR) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

int case6_add_authority(struct sdma_test_input *cmd)
{
    char *src =NULL, *dst_addr = NULL;
    char sdma_dev[DEV_LEN] = {0};
    int ret, i, id, key;
    pid_t pid;

    srand(getpid());
    key = rand();

    for (i = 0; i < PROC_NUM; i++) {
        pid = fork();
        if (pid == 0) {
            break;
        }
    }

    if (i >= PROC_NUM) {
        printf("I'm parent, pid = %u\n", getpid());
        id = sdma_nearest_id();
        if (id < 0) {
            printf("sdma_nearest_id fail!, ret = %d\n", id);
            return id;
        }
        sprintf(sdma_dev, "/dev/sdma%d", id);

        src = (char *)malloc(DATA_SIZE);
        if (!src) {
            printf("src addr is NULL\n");
            return SDMA_TEST_FAILED;
        }

        for (i = 0; i < DATA_SIZE; i++) {
            src[i] = i;
        }

        fd0 = open(sdma_dev, O_RDWR);
        if (fd0 < 0) {
            printf("child open sdma%d failed!\n", id);
            free(src);
            return fd0;
        }

        ret = sdma_send_add_authority(key, src);
        if (ret != 0) {
            printf("sdma_send_add_authority failed!\n");
            free(src);
            close(fd0);
            return ret;
        }
        free(src);
        close(fd0);
    } else {
        printf("I'm %d child, pid = %u, father pid is %u\n", i+1, getpid(), getppid());
        id = sdma_nearest_id();
        if (id < 0) {
            printf("sdma_nearest_id fail!, ret = %d\n", id);
            exit(EXIT_FAILURE);
        }
        sprintf(sdma_dev, "/dev/sdma%d", id);

        dst_addr = (char *)calloc(DATA_SIZE, sizeof(char));
        if (dst_addr == NULL ) {
            printf("dest addr is NULL\n");
            exit(EXIT_FAILURE);
        }

        fd1 = open(sdma_dev, O_RDWR);
        if (fd1 < 0) {
            printf("parent open sdma%d failed!\n", id);
            free(dst_addr);
            exit(EXIT_FAILURE);
        }

        ret = sdma_recv_add_authority(key, dst_addr);
        if (ret != 0) {
            printf("sdma_recv_add_authority failed!\n");
        }
        free(dst_addr);
        close(fd1);
        exit(EXIT_SUCCESS);
    }

    return 0;
}