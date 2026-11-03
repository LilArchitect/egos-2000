/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"

#define MLFQ_NLEVELS          5
#define MLFQ_RESET_PERIOD     100000000         /* 10 seconds */
#define MLFQ_LEVEL_RUNTIME(x) (x + 1) * 1000000 /* e.g., 100ms for level 0 */
extern struct process proc_set[MAX_NPROCESS + 1];

static void proc_set_status(int pid, enum proc_status status) {
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING_SYSCALL); }

int proc_alloc() {
    static uint curr_pid = 0;
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid    = ++curr_pid;
            proc_set[i].status = PROC_LOADING;
            proc_set[i].syscall_count = 0;
            /* Student's code goes here (Multiple Projects). */
            proc_set[i].start_time = mtime_get();
            proc_set[i].last_run_time = 0;
            proc_set[i].response_time = 0;
            proc_set[i].cpu_time = 0;
            proc_set[i].number_of_kernel_entries = 0;
            proc_set[i].number_of_syscalls = 0;
            proc_set[i].number_of_interrupts = 0;
            proc_set[i].sleep_end_time = 0;
            /* [Preemptive Scheduling]
             * Initialize the fields for lifecycle statistics and MLFQ.
             * [System Call & Protection]
             * Initialize the fields for the process sleep system call. */

            /* Student's code ends here. */
            return curr_pid;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

static void proc_print_metrics(int i) {
    struct process* p = &proc_set[i];
    ulonglong turnaround = mtime_get() - p->start_time;
    INFO("process %d terminated: turnaround %d ms, response %d ms, CPU %d ms, "
         "%d kernel entries, %d system calls, %d timer interrupts",
         p->pid,
         (uint)(turnaround / 10000),
         (uint)(p->response_time / 10000),
         (uint)(p->cpu_time / 10000),
         p->number_of_kernel_entries,
         p->number_of_syscalls,
         p->number_of_interrupts);
}

void proc_free(int pid) {
    if (pid != GPID_ALL) {
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid == pid &&
                proc_set[i].status != PROC_UNUSED) {
                proc_print_metrics(i);
                earth->mmu_free(pid);
                proc_set_status(pid, PROC_UNUSED);
                return;
            }
        }
        return;
    }

    /* Free all user applications. */
    /* Free all user applications. */
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        if (proc_set[i].pid >= GPID_USER_START &&
            proc_set[i].status != PROC_UNUSED) {
            proc_print_metrics(i);
            earth->mmu_free(proc_set[i].pid);
            proc_set[i].status = PROC_UNUSED;
        }
    }
}

void mlfq_update_level(struct process* p, ulonglong runtime) {
    /* Student's code goes here (Preemptive Scheduling). */

    /* Update the MLFQ-related fields in struct process* p after this
     * process has run on the CPU for another ulonglong runtime time units. */

    /* Student's code ends here. */
}

void mlfq_reset_level() {
    /* Student's code goes here (Preemptive Scheduling). */
    if (!earth->tty_input_empty()) {
        /* Reset the level of GPID_SHELL if there is pending keyboard input. */
    }

    static ulonglong MLFQ_last_reset_time = 0;
    /* Reset the level of all processes every MLFQ_RESET_PERIOD time units. */

    /* Student's code ends here. */
}

void proc_sleep(int pid, uint usec) {
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        if (proc_set[i].pid == pid &&
            proc_set[i].status != PROC_UNUSED) {
            proc_set[i].sleep_end_time =
                mtime_get() + (ulonglong)usec * 10;
            proc_set[i].status = PROC_SLEEPING;
            return;
        }
    }
}

void proc_coresinfo() {
    /* Student's code goes here (Multicore & Locks). */

    /* Print out the pid of the process running on each CPU core. */

    /* Student's code ends here. */
}
