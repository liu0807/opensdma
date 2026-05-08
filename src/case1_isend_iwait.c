/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include "mdk_sdma.h"
#include "ut_sdma.h"

#define DATA_SIZE (DATA_LENGTH * TASK_NUM * CHN_NUM)
#define TASK_TOTAL_NUM  (TASK_NUM * CHN_NUM)
#define DATA_LENGTH 32
#define TEST_DATA 0xc
#define TASK_NUM 10
#define CHN_IDX 2
#define CHN_NUM 1
#define CNT 1

static int sdma_isend_process(void *sdma, sdma_sqe_task_t *sqe_task, uint32_t count, sdma_request_t *request)
{
    int ret;

    printf("ready to icopy\n");
    ret = sdma_icopy_data(sdma, sqe_task, count, request);
    if (ret != 0) {
        printf("sdma_icopy_data failed ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }

    ret = sdma_iwait_chn(sdma, request);
    if (ret != 0) {
        printf("sdma_iwait_chn failed, ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }

    return 0;
}

int case1_isend_iwait(struct sdma_test_input *cmd)
{
    sdma_sqe_task_t sqe_task[TASK_TOTAL_NUM] = {0};
    sdma_request_t request[TASK_TOTAL_NUM] = {0};
    char sdma_dev[DEV_LEN] = {0};
    char dest[DATA_SIZE] = {0};
    char src[DATA_SIZE] = {0};
    uint32_t process_id;
    int fd, ret, i, id;
    void *sdma = NULL;

    id = sdma_nearest_id();
    if (id < 0) {
        printf("sdma_nearest_id fail!, ret = %d\n", id);
        return SDMA_TEST_FAILED;
    }
    sprintf(sdma_dev, "/dev/sdma%d", id);
    fd = open(sdma_dev, O_RDWR);
    if (fd < 0) {
        printf("open sdma fail!\n");
        return SDMA_TEST_FAILED;
    }

    for (i = 0; i < DATA_SIZE; i++) {
        src[i] = TEST_DATA;
    }

    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        close(fd);
        return SDMA_TEST_FAILED;
    }

    sdma = sdma_init_chn(fd, CHN_IDX);
    if (sdma == NULL) {
        printf("creat channl failed\n");
        close(fd);
        return SDMA_TEST_FAILED;
    }

    fill_sdma_task(sqe_task, TASK_NUM, CHN_NUM, DATA_LENGTH, process_id, src, dest);
    ret = sdma_isend_process(sdma, sqe_task, CNT, &request[0]);
    if (ret != 0) {
        printf("sdma_isend_process failed\n");
        goto release;
    }

    for (i = 0; i < DATA_LENGTH; i++) {
        if (dest[i] != TEST_DATA) {
            printf("data cmp wrong!, data=%d\n", dest[i]);
            goto release;
        }
    }

    if (sdma_deinit_chn(sdma)) {
        printf("sdma_deinit_chn failed!\n");
    }
    close(fd);
    return 0;

release:
    if (sdma_deinit_chn(sdma)) {
        printf("sdma_deinit_chn failed!\n");
    }
    close(fd);
    return SDMA_TEST_FAILED;
}