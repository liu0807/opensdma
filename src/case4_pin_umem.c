/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "mdk_sdma.h"
#include "ut_sdma.h"

#define DATA_SIZE (DATA_LENGTH * TASK_NUM * CHN_NUM)
#define TASK_TOTAL_NUM  (TASK_NUM * CHN_NUM)
#define DATA_LENGTH 4096 * 2
#define TASK_NUM 10
#define CHN_IDX 0
#define CHN_NUM 1
#define CNT 1

static int sdma_isend_process(void *sdma, sdma_sqe_task_t *sqe_task, uint32_t count, sdma_request_t *request)
{
    int ret;

    ret = sdma_icopy_data(sdma, sqe_task, count, request);
    if (ret != 0) {
        printf("sdma_icopy_data failed ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }
    printf("sdma_icopy_data success!\n");

    ret = sdma_iwait_chn(sdma, request);
    if (ret != 0) {
        printf("sdma_iwait_chn failed, ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }
    printf("sdma_iwait_chn success!\n");

    return 0;
}

static int sdma_test(char *dest, char *src)
{
    sdma_sqe_task_t sqe_task[TASK_TOTAL_NUM] = {0};
    sdma_request_t request[TASK_TOTAL_NUM] = {0};
    char sdma_dev[DEV_LEN] = {0};
    uint32_t process_id;
    void *sdma = NULL;
    int fd, ret, id;
    uint64_t cookie;

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

    if (sdma_get_process_id(fd, &process_id) != 0) {
        printf("get process_id failed\n");
        close(fd);
        return SDMA_TEST_FAILED;
    }

    ret = sdma_pin_umem(fd, (void *)dest, (uint32_t)DATA_SIZE, &cookie);
    if (ret != 0) {
        printf("sdma_pin_umem failed,ret = %d\n", ret);
        goto err_close;
    }
    printf("sdma_pin_umem success!\n");

    sdma = sdma_init_chn(fd, CHN_IDX);
    if (sdma == NULL) {
        printf("creat channel failed!\n");
        goto err_unpin;
    }

    fill_sdma_task(sqe_task, TASK_NUM, CHN_NUM, DATA_LENGTH, process_id, src, dest);
    ret = sdma_isend_process(sdma, sqe_task, CNT, &request[0]);
    if (ret != 0) {
        printf("sdma_isend_process failed!\n");
        goto err_deinit;
    }

    if (sdma_deinit_chn(sdma)) {
        printf("sdma_deinit_chn failed!\n");
    }

    if (sdma_unpin_umem(fd, cookie)) {
        printf("sdma unpin umem fail!!!\n");
        ret = SDMA_TEST_FAILED;
    } else {
        printf("sdma unpin umem success!\n");
        ret = 0;
    }
    close(fd);

    return ret;

err_deinit:
    if (sdma_deinit_chn(sdma)) {
        printf("sdma_deinit_chn failed!\n");
    }
err_unpin:
    if (sdma_unpin_umem(fd, cookie)) {
        printf("sdma_unpin_umem failed!\n");
    }
err_close:
    close(fd);
    return SDMA_TEST_FAILED;
}

int case4_pin_umem(struct sdma_test_input *cmd)
{
    char *dest, *src;
    int ret;

    src = (char *)calloc(DATA_SIZE, sizeof(char));
    if (!src) {
        printf("Failed to alloc src addr\n");
        return SDMA_TEST_FAILED;
    }
    dest = (char *)calloc(DATA_SIZE, sizeof(char));
    if (!dest) {
        printf("Failed to alloc dest addr\n");
        free(src);
        return SDMA_TEST_FAILED;
    }

    printf("sdma test start!\n");
    ret = sdma_test(dest, src);
    if (ret != 0) {
        printf("sdma  test fail!\n");
    } else {
        printf("sdma test success!\n");
    }

    free(src);
    free(dest);
    return ret;
}