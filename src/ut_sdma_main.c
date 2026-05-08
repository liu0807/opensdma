#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>

#include "ut_sdma.h"
#include "mdk_sdma.h"

#define DECIMAL 10
#define SDMA_PERFORMANCE_TEST_NUM 3
#define SDMA_HBM_TEST_NUM 1
#define VERSION "1.0.0"

typedef int (*EventFuncHandlePtr)(struct sdma_test_input *cmd);

typedef struct {
    EventFuncHandlePtr phandle;
} EventTableType;

static EventTableType eventMap[] = {
    {case0_sdma_test},
    {case1_isend_iwait},
    {case2_gather_scatter},
    {case3_opcode_excpt},
    {case4_pin_umem},
    {case5_not_add_authority},
    {case6_add_authority},
    {case7_sqe_overflow},
    {case8_highspeed_mem},
    {case_hpl_pattern_test},
    {case_single_direction},
    {case_muti_direction},
};

static char *case_names[] = {
    "case_sdma_test",
    "case_isend_iwait",
    "case_gather_scatter",
    "case_opcode_excpt",
    "case_pin_umem",
    "case_not_add_authority",
    "case_add_authority",
    "case_sqe_overflow",
    "case_highspeed_mem",
    "case_hpl_pattern_test",
    "case_single_direction",
    "case_muti_direction",
};

void print_help()
{
    printf(
        "\nNAME\n"
        "\tsdma_tool: test SDMA function and performance\n"
        "\tversion: %s\n"
        "USAGE\n"
        "\tsimple function case: sdma_tool {--case}\n"
        "\thelp: sdma_tool [--help]\n"
        "\trun all simple case: sdma_tool {--all}\n"
        "\tcase 9 hbm memory send test\n"
        "\t\tsdma_tool {--case, -c} {--num-of-cpu, -n}\n"
        "\thpl pattern test:\n"
        "\t\tsdma_tool {--case, -c} {--loop, -l} {--num-of-cpu, -n} {--id-of-die, -i} "
        "{--pattern, -p}\n"
        "\tsingle direction performance test:\n"
        "\t\tsdma_tool {--case, -c} {--data-size, -d} {--send, -s} {--recv, -r} {--num-of-cpu, -n} "
        "{--memory-type, -m} {--loop, -l}\n"
        "\tmulti direction performance test:\n"
        "\t\tsdma_tool {--case, -c} {--data-size, -d} {--send, -s} {--recv, -r} {--num-of-cpu, -n} "
        "{--memory-type, -m} {--loop, -l}\n"
        "DESCRIPTION\n"
        "\t[--case]:\n"
        "\t\tsupport 12 cases in total\n"
        "\t\tcase 1 to case 9 - simple function case, only need option case:\n"
        "\t\t\t--case 1: exclusiv_channel send test\n"
        "\t\t\t--case 2: share_channel send test\n"
        "\t\t\t--case 3: stride mode send test\n"
        "\t\t\t--case 4: exception test\n"
        "\t\t\t--case 5: pin mem test\n"
        "\t\t\t--case 6: not add authority test\n"
        "\t\t\t--case 7: add authority test\n"
        "\t\t\t--case 8: task overflow test\n"
        "\t\t\t--case 9: ddr to ddr, ddr to hbm, hbm to ddr, hbm to hbm memory send test\n"
        "\t\tcase 10 to case 12 - performance test case, need other parameters:\n"
        "\t\t\t--case 10: hpl pattern test\n"
        "\t\t\t--case 11: single direction performance test\n"
        "\t\t\t--case 12: multi direction performance test\n"
        "\t[--send]:\n"
        "\t\tcpu core id of send process\n"
        "\t[--recv]:\n"
        "\t\tcpu core id of recv process\n"
        "\t[--num-of-cpu]:\n"
        "\t\tcpu number of single numa node, only support 38/144/152\n"
        "\t[--data-size]:\n"
        "\t\tdata size of single sqe\n"
        "\t\t\tsmaller than 1GB for single direction performance test\n"
        "\t\t\tsmaller than 50MB for muti direction performance test\n"
        "\t[--loop]:\n"
        "\t\tloop times of send task\n"
        "\t\thpl pattern test loop times is limited to: (1-10000)\n"
        "\t\tsingle direction performance test loop times is limited to: (1-2000)\n"
        "\t\tmuti direction performance test loop times is limited to: (1-2000)\n"
        "\t[--pattern]:\n"
        "\t\thpl matrix pattern:\n"
        "\t\t\tpattern 1:ddr to ddr, move_length=1KB, num_stride=512, stride=147456B\n"
        "\t\t\tpattern 2:ddr to hbm, move_length=1KB, num_stride=4096, stride=147456B\n"
        "\t\t\tpattern 3:hbm to ddr, move_length=1KB, num_stride=4096, stride=147456B\n"
        "\t\t\tpattern 4:hbm to hbm, move_length=4KB, num_stride=4096, stride=393216B\n"
        "\t[--memory-type]:\n"
        "\t\tmemory transfer direction:\n"
        "\t\t\tmemory-type 1: ddr to ddr 2: ddr to hbm 3: hbm to ddr 4: hbm to hbm\n"
        "\t[--id-of-die]:\n"
        "\t\texcute hpl pattern test using a specific die id, support 0 to 3\n"
        "\t\twhen num-of-cpu is 152, the die id is invalid\n"
        "\t[--all]:\n"
        "\t\texcute all simple function case (1-8)\n"
        "TIPPS\n"
        "\t1. using 'echo 1024 >/sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages'\n"
        "\t   to set 2M hugetlb for highspeed related cases before running, such as:\n"
        "\t\t hpl pattern test/single direction/multi direction performance test/case 9\n"
        "\t\t all ddr and other nodes should set hugetlb greater than 1024\n"
        "\t2. if open sdma failed, please check if sdma_dae.ko already installed, if not, use\n"
        "\t   'modprobe sdma_dae' to install sdma_dae.ko\n"
        "\t3. if sdma_dae.ko already installed and open sdma still failed, please check if SMMU opened or not\n",
        VERSION
        );
}

static int initialize_test_environment(void)
{
    int fd;
    fd = open("/dev/sdma0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open sdma fail!\n");
        return SDMA_TEST_FAILED;
    }
    close(fd);

    return SDMA_TEST_SUCCESS;
}

static int execute_ut_sdma_single(struct sdma_test_input *test_cmd)
{
    int ret;
    int idx;

    idx = test_cmd->case_num;
    if (idx > sizeof(eventMap) / sizeof(eventMap[0])) {
        fprintf(stderr, "Unsupported sdma case %d\n", idx);
        return SDMA_TEST_FAILED;
    }

    printf("\n\t\t*** Starting SDMA test %d %s***\n", idx, case_names[idx - 1]);
    ret = eventMap[idx - 1].phandle(test_cmd);
    if (ret != 0) {
        fprintf(stderr, "\nExcuting SDMA test %d %s FAILED!\n", idx, case_names[idx - 1]);
        return SDMA_TEST_FAILED;
    }
    printf("\n\t\t*** Single SDMA test %d %s SUCCESS!***\n", idx, case_names[idx - 1]);

    return SDMA_TEST_SUCCESS;
}

static int execute_ut_sdma(struct sdma_test_input *test_cmd)
{
    int num = 0;
    int ret;
    int i;

    printf("\n\t\t*** Starting All SDMA tests ***\n");
    for (i = 0; i < sizeof(eventMap) / sizeof(eventMap[0]) - SDMA_PERFORMANCE_TEST_NUM -
         SDMA_HBM_TEST_NUM; i++) {
        test_cmd->case_num = i + 1;
        ret = execute_ut_sdma_single(test_cmd);
        if (ret != 0) {
            num++;
        }
        sleep(1);
    }

    if (num != 0) {
        fprintf(stderr, "\n Excuting SDMA All tests FAILED!\n");
        return SDMA_TEST_FAILED;
    }

    printf("\n Excuting SDMA All tests SUCCESS!\n");
    return SDMA_TEST_SUCCESS;
}

static int sdma_test_getopt(int argc, char **argv, struct sdma_test_input *test_cmd)
{
    char *endptr;
    int c = 0;

    struct option long_opt[] = {
        {"case", required_argument, NULL, 'c'},
        {"send", required_argument, NULL, 's'},
        {"recv", required_argument, NULL, 'r'},
        {"num-of-cpu", required_argument, NULL, 'n'},
        {"data-size", required_argument, NULL, 'd'},
        {"loop", required_argument, NULL, 'l'},
        {"pattern", required_argument, NULL, 'p'},
        {"memory-type", required_argument, NULL, 'm'},
        {"id-of-die", required_argument, NULL, 'i'},
        {"all", no_argument, NULL, 'a'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    /* Process Command Line arguments */
    while ((c = getopt_long(argc, argv, "hac:s:r:n:d:l:p:m:i:", long_opt, NULL)) != -1) {
        switch (c) {
            case 'a':
                test_cmd->run_all_test = 1;
                return SDMA_TEST_SUCCESS;
            case 'h':
                test_cmd->print_help = 1;
                return SDMA_TEST_SUCCESS;
            case 'c':
                test_cmd->case_num = strtol(optarg, &endptr, DECIMAL);
                if (test_cmd->case_num == 0 ||
                    test_cmd->case_num > sizeof(eventMap) / sizeof(eventMap[0])) {
                    fprintf(stderr, "Unsupported sdma case %d\n", test_cmd->case_num);
                    return SDMA_TEST_FAILED;
                }
                break;
            case 's':
                test_cmd->send_cpu = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'r':
                test_cmd->recv_cpu = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'n':
                test_cmd->num_of_cpu = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'd':
                test_cmd->data_size = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'l':
                test_cmd->loop_times = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'p':
                test_cmd->hpl_pattern = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'm':
                test_cmd->memory_type = strtol(optarg, &endptr, DECIMAL);
                break;
            case 'i':
                test_cmd->die_id = strtol(optarg, &endptr, DECIMAL);
                break;
            default:
                if (isprint (optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);

                return SDMA_TEST_FAILED;
        }
    }

    return SDMA_TEST_SUCCESS;
}

int main(int argc, char **argv)
{
    struct sdma_test_input test_cmd = {0};
    int status;
    int ret;

    if (argc <= 1) {
        fprintf(stderr, "\nPlease add input parameters, detail see help infomations\n");
        print_help();
        return SDMA_TEST_FAILED;
    }

    /* get user parameters */
    ret = sdma_test_getopt(argc, argv, &test_cmd);
    if (ret != 0) {
        fprintf(stderr, "\nGet input cmd FAILED!\n");
        print_help();
        return SDMA_TEST_FAILED;
    }

    if (test_cmd.print_help == 1) {
        print_help();
        return SDMA_TEST_SUCCESS;
    }

    /* SDMA usablility check */
    status = initialize_test_environment();
    if (status) {
        fprintf(stderr, "\nInitialize test environment FAILED!\n");
        return SDMA_TEST_FAILED;
    }

    /* excute testcase */
    printf("\n\t\t*** Starting SDMA test ***\n");
    if (test_cmd.run_all_test == 1) {
        ret = execute_ut_sdma(&test_cmd);
        if (ret != 0) {
            fprintf(stderr, "\n\t\t*** SDMA test FAILED! ***\n");
            return SDMA_TEST_FAILED;
        }
        printf("\n\t\t*** SDMA test SUCCESS! ***\n");
        return SDMA_TEST_SUCCESS;
    }
    if (test_cmd.case_num <= 0 || test_cmd.case_num > sizeof(eventMap) / sizeof(eventMap[0])) {
        fprintf(stderr, "\nInput illeague!FAILED\n");
        print_help();
        return SDMA_TEST_FAILED;
    }

    ret = execute_ut_sdma_single(&test_cmd);
    if (ret != 0) {
        fprintf(stderr, "\n\t\t*** SDMA test FAILED! ***\n");
        return SDMA_TEST_FAILED;
    }
    printf("\n\t\t*** SDMA test SUCCESS! ***\n");

    return SDMA_TEST_SUCCESS;
}

void fill_sdma_task(void *sqe, int task_num, int chn_num, int data_size, uint32_t process_id, char *src, char *dest)
{
    sdma_sqe_task_t *sqe_task = (sdma_sqe_task_t*)sqe;
    int i;

    for (i = 0; i < task_num * chn_num; i++) {
        sqe_task[i].src_addr = (uint64_t)(void *)(src + data_size * i);
        sqe_task[i].dst_addr = (uint64_t)(void *)(dest + data_size * i);
        sqe_task[i].src_process_id = process_id;
        sqe_task[i].dst_process_id = process_id;
        sqe_task[i].src_stride_len = 0;
        sqe_task[i].dst_stride_len = 0;
        sqe_task[i].stride_num = 0;
        sqe_task[i].length = data_size;
        sqe_task[i].opcode = OPCODE_COMMON_MODE;
        sqe_task[i].next_sqe = &sqe_task[i + 1];
    }
}

void sdma_count_bw_latency(struct timeval start, struct timeval end, uint64_t data_size,
                           int loop_times, int thread_num)
{
    uint64_t runtime_us, bandwidth;
    struct timeval diff;
    int pid = getpid();

    timersub(&end, &start, &diff);
    runtime_us = diff.tv_sec * US_PER_SEC + diff.tv_usec;
    bandwidth = ((uint64_t)data_size * loop_times * thread_num) / runtime_us;
    bandwidth = bandwidth * US_PER_MS / BYTE_PER_KBYTE;
    bandwidth = bandwidth * MS_PER_SEC / KBYTE_PER_MBYTE;
    printf("proc %d, runtime_us = %ldus, [BANDWIDTH] = %ldMB/s\n", pid, runtime_us, bandwidth);
}