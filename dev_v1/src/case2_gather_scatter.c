#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "mdk_sdma.h"
#include "ut_sdma.h"

#define SRC_STRIDE 10
#define DST_STRIDE 11
#define STRIDE_NUM 2
#define DATALEN 4096
#define DATA_SIZE 32
#define TASK_NUM 1
#define CHN_NUM 2
#define CNT 1

static int sdma_send_process(void *sdma, sdma_sqe_task_t *sqe_task, uint32_t count)
{
    int ret = 0;

    printf("sdma start copy data\n");
    ret = sdma_copy_data(sdma, sqe_task, count);
    if (ret != 0) {
        printf("sdma_copy_data failed, ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }

    printf("sdma start wait chn\n");
    ret = sdma_wait_chn(sdma, count);
    if (ret != 0) {
        printf("sdma_wait_chn failed, ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }
    printf("sdma_wait_chn success!\n");

    return 0;
}

static void sdma_stride_mode_task_fill(sdma_sqe_task_t *sqe_task, int task_num, int chn_num,
                                       int data_size, uint32_t process_id, char *src, char *dest)
{
    int i;

    for (i = 0; i < task_num * chn_num; i++) {
        sqe_task[i].src_addr = (uint64_t)(void *)(src + data_size * i);
        sqe_task[i].dst_addr = (uint64_t)(void *)(dest + data_size * i);
        sqe_task[i].src_process_id = process_id;
        sqe_task[i].dst_process_id = process_id;
        sqe_task[i].src_stride_len = SRC_STRIDE;
        sqe_task[i].dst_stride_len = DST_STRIDE;
        sqe_task[i].stride_num = STRIDE_NUM;
        sqe_task[i].length = data_size;
        sqe_task[i].opcode = OPCODE_COMMON_MODE;
        sqe_task[i].next_sqe = &sqe_task[i+1];
    }

    return;
}

int case2_gather_scatter(struct sdma_test_input *cmd)
{
    sdma_sqe_task_t sqe_task[TASK_NUM * CHN_NUM + 1] = {0};
    char sdma_dev[DEV_LEN] = {0};
    int data_size = DATA_SIZE;
    void *sdma[CHN_NUM] = {0};
    char dest[DATALEN] = {0};
    char src[DATALEN] = {0};
    uint32_t process_id;
    int fd, ret, i, id;

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
    ret = sdma_get_process_id(fd, &process_id);
    if (ret != 0) {
        printf("get process_id failed\n");
        close(fd);
        return SDMA_TEST_FAILED;
    }

    for (i = 0; i < DATALEN; i++) {
        src[i] = i;
    }

    sdma_stride_mode_task_fill(sqe_task, TASK_NUM, CHN_NUM, data_size, process_id, src, dest);
    for (int i = 0; i < CHN_NUM; i++) {
        sdma[i] = sdma_alloc_chn(fd);
        if (sdma[i] == NULL) {
            printf("creat channl failed\n");
            goto release;
        }
        ret = sdma_send_process(sdma[i], sqe_task, CNT);
        if (ret != 0) {
            printf("sdma_send_process failed\n");
            goto release;
        }

        for (int j = 0; j < data_size; j++) {
            if (dest[j] != j) {
                printf("data cmp wrong!, dest[%d] = %d\n", j, dest[j]);
                goto release;
            }
            dest[j] = 0;
        }
    }

    for (int i = 0; i < CHN_NUM; i++) {
        if (sdma_free_chn(sdma[i])) {
            printf("sdma_free_chn failed!\n");
        }
    }
    close(fd);

    return 0;

release:
    for (int i = 0; i < CHN_NUM; i++) {
        if (sdma[i]) {
            if (sdma_free_chn(sdma[i])) {
                printf("sdma_free_chn failed!\n");
            }
        }
    }
    close(fd);

    return SDMA_TEST_FAILED;
}