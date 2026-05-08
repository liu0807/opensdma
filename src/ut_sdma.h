#ifndef __UT_SDMA_H
#define __UT_SDMA_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

#define OPCODE_COMMON_MODE  0x0
#define SDMA_TEST_FAILED    (-1)
#define SDMA_TEST_SUCCESS   0
#define NUMA_NODES_NUMA_ENV 32
#define NUMA_NODES_UMA_ENV  8

#define US_PER_SEC      1000000L
#define MS_PER_SEC      1000
#define US_PER_MS       1000
#define BYTE_PER_KBYTE  1024
#define KBYTE_PER_MBYTE 1024

#define SHM_ERR (-1)
#define DEV_LEN 20
#define RWCTL   0666

// hugepage size 2MB by default, please close THP
#define HUGEPAGE_SIZE   (2 * 1024 * 1024)
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#endif
#define SDMA_MMAP_FLAG  (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB)
#define SDMA_PROT_FLAG  (PROT_READ | PROT_WRITE)

struct sdma_test_input {
    uint32_t case_num;
    uint32_t print_help;
    uint32_t run_all_test;
    uint32_t send_cpu;
    uint32_t recv_cpu;
    uint32_t num_of_cpu;
    uint32_t data_size;
    uint32_t loop_times;
    uint32_t hpl_pattern;
    uint32_t memory_type;
    uint32_t die_id;
};

int case0_sdma_test(struct sdma_test_input *cmd);
int case1_isend_iwait(struct sdma_test_input *cmd);
int case2_gather_scatter(struct sdma_test_input *cmd);
int case3_opcode_excpt(struct sdma_test_input *cmd);
int case4_pin_umem(struct sdma_test_input *cmd);
int case5_not_add_authority(struct sdma_test_input *cmd);
int case6_add_authority(struct sdma_test_input *cmd);
int case7_sqe_overflow(struct sdma_test_input *cmd);
int case8_highspeed_mem(struct sdma_test_input *cmd);

int case_hpl_pattern_test(struct sdma_test_input *cmd);
int case_muti_direction(struct sdma_test_input *cmd);
int case_single_direction(struct sdma_test_input *cmd);

void fill_sdma_task(void *sqe_task, int task_num, int chn_num, int data_size, uint32_t pasid,
                    char *src, char *dest);
void sdma_count_bw_latency(struct timeval start, struct timeval end, uint64_t data_size,
                           int loop_times, int thread_num);
void print_help(void);

#endif
