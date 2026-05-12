#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define GATHER_PROC_NUM 200

struct shared_use_st {
    uint64_t dst_addr_list[GATHER_PROC_NUM-1];
    uint64_t src_addr_list[GATHER_PROC_NUM-1];
	uint64_t dst_addr;
    uint64_t src_addr;
    int written;
	int readen;
    int owner_use;
    uint32_t owner_process_id;
    int finish_flag;
    uint32_t submitter_process_id;
    int send_proc_ret;
    int recv_proc_ret;

    bool owner_pid_ready;
    bool owner_task_ready;
    bool owner_grant_finish;
    bool submitter_pid_ready;
    bool submitter_task_ready;
    bool submitter_grant_finish;
};