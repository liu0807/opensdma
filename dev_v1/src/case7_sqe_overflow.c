#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "mdk_sdma.h"
#include "ut_sdma.h"

#define TASK_NUM (CHANNEL_DEPTH + OVERFLOW_NUM)
#define CHANNEL_DEPTH (1 << 10)
#define OVERFLOW_NUM 20
#define DATA_SIZE 2048

int case7_sqe_overflow(struct sdma_test_input *cmd)
{
    sdma_sqe_task_t sqe_task[TASK_NUM + 1] = {0};
    char sdma_dev[DEV_LEN] = {0};
    char *dest[TASK_NUM] = {0};
    char *src[TASK_NUM] = {0};
    uint32_t process_id;
    void *sdma = NULL;
    int ret;
    int id;
    int fd;
    int i;

    id = sdma_nearest_id();
    if (id < 0) {
        printf("sdma_nearest_id fail!, ret = %d\n", id);
        return SDMA_TEST_FAILED;
    }

    sprintf(sdma_dev, "/dev/sdma%d", id);

    fd = open(sdma_dev, O_RDWR);
    if (fd < 0) {
        printf("open sdma failed\n");
        return SDMA_TEST_FAILED;
    }

    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        goto close;
    }

    for (i = 0; i < TASK_NUM; i++) {
        src[i] = (char *)malloc(DATA_SIZE);
        if (src[i] == NULL) {
            printf("malloc SRC failed\n");
            ret = SDMA_TEST_FAILED;
            goto free;
        }
        dest[i] = (char *)malloc(DATA_SIZE);
        if (dest[i] == NULL) {
            printf("malloc DEST failed\n");
            ret = SDMA_TEST_FAILED;
            goto free;
        }
    }

    for (i = 0; i < TASK_NUM; i++) {
        sqe_task[i].src_addr = (uint64_t)(void *)(src[i]);
        sqe_task[i].dst_addr = (uint64_t)(void *)(dest[i]);
        sqe_task[i].src_process_id = process_id;
        sqe_task[i].dst_process_id = process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = DATA_SIZE;
        sqe_task[i].opcode = OPCODE_COMMON_MODE;
        sqe_task[i].next_sqe = &sqe_task[i+1];
    }

    sdma = sdma_alloc_chn(fd);
    if (sdma == NULL) {
        printf("alloc chn failed\n");
        ret = SDMA_TEST_FAILED;
        goto free;
    }
    printf("task %d send data\n", i);
    ret = sdma_copy_data(sdma, sqe_task, TASK_NUM);
    if (ret == SDMA_TEST_FAILED) {
        ret = 0;
        printf("sqe overflow! testcase success!\n");
        goto exit;
    }
    printf("task %d wait channel\n", i);
    ret = sdma_wait_chn(sdma, TASK_NUM);
    if (ret != 0) {
        goto exit;
    }
    printf("sdma wait channel done\n");

exit:
    if (sdma_free_chn(sdma)) {
        printf("sdma_free_chn failed!\n");
    }
free:
    for (i = 0; i < TASK_NUM; i++) {
        if (dest[i] != NULL) {
            free(dest[i]);
        }
        if (src[i] != NULL) {
            free(src[i]);
        }
    }
close:
    close(fd);
    return ret;
}
