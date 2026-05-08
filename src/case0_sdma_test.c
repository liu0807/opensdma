#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include "mdk_sdma.h"
#include "ut_sdma.h"

#define SQE_NUM     2
#define TEST_DATA   0xc
#define DATA_SIZE   (DATA_LEN * SQE_NUM)
#define CHN_NUM     8
#define DATA_LEN    32
#define COUNT       1

static int sdma_test_process(void *sdma, sdma_sqe_task_t *sqe_task, uint32_t count)
{
    int ret;

    ret = sdma_copy_data(sdma, sqe_task, count);
    if (ret != 0) {
        printf("sdma_copy_data failed, ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }

    ret = sdma_wait_chn(sdma, count);
    if (ret != 0) {
        printf("sdma_wait_chn failed, ret = %d\n", ret);
        return SDMA_TEST_FAILED;
    }
    printf("sdma wait success!\n");

    return 0;
}

int case0_sdma_test(struct sdma_test_input *cmd)
{
    sdma_sqe_task_t sqe_task[SQE_NUM * CHN_NUM + 1] = {0};
    char sdma_dev[DEV_LEN] = {0};
    char dest[DATA_SIZE] = {0};
    char src[DATA_SIZE] = {0};
    void *sdma[CHN_NUM] = {0};
    uint32_t process_id;
    int fd, ret, id;
    int i, j;

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

    for (i = 0; i < DATA_SIZE ; i++) {
        src[i] = TEST_DATA;
    }

    fill_sdma_task(sqe_task, SQE_NUM, COUNT, DATA_LEN, process_id, src, dest);
    for (i = 0; i < CHN_NUM; i++) {
        sdma[i] = sdma_alloc_chn(fd);
        if (sdma[i] == NULL) {
            printf("creat channl failed\n");
            goto release;
        }
        ret = sdma_test_process(sdma[i], sqe_task, SQE_NUM);
        if (ret != 0) {
            printf("sdma_test_process failed\n");
            goto release;
        }
        for (j = 0; j < DATA_SIZE; j++) {
            if (dest[j] != TEST_DATA) {
                printf("data cmp wrong!, dest[%d] = %d\n", j, dest[j]);
                goto release;
            }
        }
    }

    for (i = 0; i < CHN_NUM; i++) {
        if (sdma_free_chn(sdma[i])) {
            printf("sdma_free_chn failed!\n");
        }
    }
    close(fd);
    return SDMA_TEST_SUCCESS;

release:
    for (i = 0; i < CHN_NUM; i++) {
        if (sdma[i] != NULL) {
            if (sdma_free_chn(sdma[i])) {
                printf("sdma_free_chn failed!\n");
            }
        }
    }
    close(fd);
    return SDMA_TEST_FAILED;
}
