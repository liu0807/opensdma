/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "mdk_sdma.h"
#include "ut_sdma.h"

#define DATA_SIZE (DATA_LEN * TASK_NUM * CHN_NUM)
#define TASK_TOTAL_NUM  (TASK_NUM * CHN_NUM)
#define OPCODE_ERR_TEST 0xf
#define START_BADADDR 3
#define START_OPCODE 1
#define END_BADADDR 5
#define END_OPCODE 3
#define TASK_NUM 10
#define DATA_LEN 32
#define CHN_NUM 1

static void fill_sdma_task_opcode_error(void *sqe, int task_num, int chn_num, int data_size,
                                        uint32_t process_id, char *src, char *dest)
{
    int i;
    sdma_sqe_task_t *sqe_task = (sdma_sqe_task_t*)sqe;
    for (i = 0; i < task_num * chn_num; i++) {
        sqe_task[i].src_addr = (uint64_t)(void *)(src + data_size * i);
        sqe_task[i].dst_addr = (uint64_t)(void *)(dest + data_size * i);
        sqe_task[i].src_process_id = process_id;
        sqe_task[i].dst_process_id = process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = data_size;
        sqe_task[i].opcode = OPCODE_ERR_TEST;
    }
}

static void fill_sdma_task_src_addr_error(void *sqe, int task_num, int chn_num, int data_size,
                                          uint32_t process_id, char *src, char *dest)
{
    int i;
    sdma_sqe_task_t *sqe_task = (sdma_sqe_task_t*)sqe;
    for (i = 0; i < task_num * chn_num; i++) {
        sqe_task[i].src_addr = (uint64_t)(void *)(0);
        sqe_task[i].dst_addr = (uint64_t)(void *)(dest + data_size * i);
        sqe_task[i].src_process_id = process_id;
        sqe_task[i].dst_process_id = process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = data_size;
        sqe_task[i].opcode = OPCODE_COMMON_MODE;
    }
}

static int sdma_isend_error_progress(int fd, sdma_sqe_task_t *sqe, int first_index, int last_index,
                                     int expect_ret)
{
    sdma_request_t request[TASK_TOTAL_NUM] = {0};
    void *sdma[TASK_TOTAL_NUM] = {0};
    int used_chn_num = last_index;
    int ret, i, flag = 0;

    for (i = first_index; i < used_chn_num; i++) {
        sdma[i] = sdma_init_chn(fd, i);
        if (sdma[i] == NULL) {
            printf("creat channl%d failed\n", i);
            goto release;
        }
        ret = sdma_icopy_data(sdma[i], sqe, 1, &request[i]);
        if (ret != 0) {
            printf("sdma_icopy%d_data failed ret = %d\n", i, ret);
            goto release;
        }
        sleep(1);

        ret = sdma_iwait_chn(sdma[i], &request[i]);
        if (ret != 0) {
            printf("sdma_iwait%d_chn failed, ret = %d\n", i, ret);
            if (ret == expect_ret) {
                printf("expected error %d occurred!\n", ret);
                flag = 1;
            }
        }
    }

    for (int i = first_index; i < used_chn_num; i++) {
        if (sdma_deinit_chn(sdma[i])) {
            printf("sdma_deinit_chn%d failed\n", i);
        }
    }

    if (flag != 1) {
        return SDMA_TEST_FAILED;
    }

    return 0;

release:
    for (int i = first_index; i < used_chn_num; i++) {
        if (sdma[i] != NULL) {
            if (sdma_deinit_chn(sdma[i])) {
                printf("sdma_deinit_chn%d failed\n", i);
            }
        }
    }

    return SDMA_TEST_FAILED;
}

int case3_opcode_excpt(struct sdma_test_input *cmd)
{
    sdma_sqe_task_t sqe_task[TASK_TOTAL_NUM] = {0};
    char sdma_dev[DEV_LEN] = {0};
    char dest[DATA_SIZE] = {0};
    char src[DATA_SIZE] = {0};
    uint32_t process_id;
    int fd, ret, id;

    id = sdma_nearest_id();
    if (id < 0) {
        printf("sdma_nearest_id fail!, ret = %d\n", id);
        return id;
    }
    sprintf(sdma_dev, "/dev/sdma%d", id);
    fd = open(sdma_dev, O_RDWR);
    if (fd < 0) {
        printf("open sdma fail!\n");
        return fd;
    }

    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        close(fd);
        return ret;
    }

    fill_sdma_task_opcode_error(sqe_task, TASK_NUM, CHN_NUM, DATA_LEN, process_id, src, dest);
    printf("ready to opcode error icopy\n");
    ret = sdma_isend_error_progress(fd, sqe_task, START_OPCODE, END_OPCODE, SDMA_INVALID_OPCODE);
    if (ret != 0) {
        printf("sdma_isend_error_progress failed\n");
        close(fd);
        return ret;
    }

    fill_sdma_task_src_addr_error(sqe_task, TASK_NUM, CHN_NUM, DATA_LEN, process_id, src, dest);
    printf("ready to smmu error icopy\n");
    ret = sdma_isend_error_progress(fd, sqe_task, START_BADADDR, END_BADADDR, SDMA_SMMU_TERMINATE);
    if (ret != 0) {
        printf("sdma_isend_error_progress failed\n");
        close(fd);
        return ret;
    }

    close(fd);
    return 0;
}
