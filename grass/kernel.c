/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 *   intr_entry() handles timer and device interrupts.
 *   excp_entry() handles system calls and faults (e.g., invalid memory access).
 */

#include "process.h"
#include <string.h>

uint core_in_kernel;
uint core_to_proc_idx[NCORES];
struct process proc_set[MAX_NPROCESS + 1];
/* proc_set[MAX_NPROCESS] is a placeholder for idle cores. */

#define curr_proc_idx core_to_proc_idx[core_in_kernel]
#define curr_pid      proc_set[curr_proc_idx].pid
#define curr_status   proc_set[curr_proc_idx].status
#define curr_saved    proc_set[curr_proc_idx].saved_registers
#define CORE_IDLE (curr_proc_idx == MAX_NPROCESS)

#define PREEMPTIVE 1

extern void idle_entry(void);

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    /* With the kernel lock, only one core can enter this point at any time. */
    asm("csrr %0, mhartid" : "=r"(core_in_kernel));

    /* Save the process context. */
    asm("csrr %0, mepc" : "=r"(proc_set[curr_proc_idx].mepc));
    memcpy(curr_saved, (void*)(EGOS_STACK_TOP - 32 * 4), 32 * 4);
    if (!CORE_IDLE) {
        proc_set[curr_proc_idx].number_of_kernel_entries++;
    }

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    /* Restore the process context. */
    asm("csrw mepc, %0" ::"r"(proc_set[curr_proc_idx].mepc));
    memcpy((void*)(EGOS_STACK_TOP - 32 * 4), curr_saved, 32 * 4);
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield();
static void proc_try_syscall(struct process* proc);

static void excp_entry(uint id) {
    if (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M) {
        proc_set[curr_proc_idx].number_of_syscalls++;
        /* Copy the system call arguments from user space to the kernel. */
        uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
        memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr,
               sizeof(struct syscall));
        proc_set[curr_proc_idx].syscall.status = PENDING;

        proc_set_pending(curr_pid);
        proc_set[curr_proc_idx].mepc += 4;
        proc_try_syscall(&proc_set[curr_proc_idx]);
        proc_yield();
        return;
    }
    /* Student's code goes here (System Call & Protection | Virtual Memory). */

    /* Kill the current process if curr_pid is a user application. */

    /* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}

static void intr_entry(uint id) {
    if (id == INTR_ID_TIMER) {
        if (!CORE_IDLE) {
            proc_set[curr_proc_idx].number_of_interrupts++;
        }

        if (PREEMPTIVE || CORE_IDLE) {
            proc_yield();
        } else {
            earth->timer_reset(core_in_kernel);
        }
    }
}

static void proc_yield() {
    if (!CORE_IDLE && curr_status == PROC_RUNNING) {
        proc_set_runnable(curr_pid);
    }

    int next_idx = MAX_NPROCESS;

    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        uint idx = (curr_proc_idx + i) % MAX_NPROCESS;
        struct process* p = &proc_set[idx];

        if (p->status == PROC_PENDING_SYSCALL) {
            proc_try_syscall(p);
        }

        if (p->status == PROC_SLEEPING &&
            p->sleep_end_time <= mtime_get()) {
            p->status = PROC_RUNNABLE;
        }

        if (p->status == PROC_READY ||
            p->status == PROC_RUNNABLE) {
            next_idx = idx;
            break;
        }
    }

    ulonglong now = mtime_get();

    if (!CORE_IDLE) {
        proc_set[curr_proc_idx].cpu_time +=
            now - proc_set[curr_proc_idx].last_run_time;
    }

    if (next_idx == MAX_NPROCESS) {
        struct process* idle = &proc_set[MAX_NPROCESS];

        if (!CORE_IDLE) {
            memcpy(idle->saved_registers,
                   curr_saved, sizeof(idle->saved_registers));
        }

        idle->mepc = (uint)idle_entry;
        idle->saved_registers[30] = EGOS_STACK_TOP;

        curr_proc_idx = MAX_NPROCESS;

        uint mstatus;
        asm volatile("csrr %0, mstatus" : "=r"(mstatus));
        mstatus |= (3U << 11) | (1U << 7);
        asm volatile("csrw mstatus, %0" :: "r"(mstatus));

        earth->timer_reset(core_in_kernel);
        return;
    }

    if (proc_set[next_idx].status == PROC_READY) {
        proc_set[next_idx].response_time =
            now - proc_set[next_idx].start_time;
    }

    proc_set[next_idx].last_run_time = now;
    curr_proc_idx = next_idx;

    earth->mmu_switch(curr_pid);
    earth->mmu_flush_cache();

    uint mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    if (curr_pid >= GPID_USER_START || earth->translation == PAGE_TABLE) {
        mstatus &= ~(3U << 11);
    } else {
        mstatus |= (3U << 11);
    }
    asm volatile("csrw mstatus, %0" :: "r"(mstatus));

    if (curr_status == PROC_READY) {
        curr_saved[0] = APPS_ARG;
        curr_saved[1] = APPS_ARG + 4;
        proc_set[curr_proc_idx].mepc = APPS_ENTRY;
    }

    proc_set_running(curr_pid);
    earth->timer_reset(core_in_kernel);
}

static void proc_try_send(struct process* sender) {
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process* dst = &proc_set[i];
        if (dst->pid == sender->syscall.receiver &&
            dst->status != PROC_UNUSED) {
            /* Return if process dst is not receiving messages or
             * is not taking messages from the sender process. */
            if (!(dst->syscall.type == SYS_RECV &&
                  dst->syscall.status == PENDING))
                return;
            if (!(dst->syscall.sender == GPID_ALL ||
                  dst->syscall.sender == sender->pid))
                return;

            dst->syscall.status = DONE;
            dst->syscall.sender = sender->pid;
            /* Copy the system call arguments within the kernel PCB. */
            memcpy(dst->syscall.content, sender->syscall.content,
                   SYSCALL_MSG_LEN);
            return;
        }
    }
    FATAL("proc_try_send: unknown receiver pid=%d", sender->syscall.receiver);
}

static void proc_try_recv(struct process* receiver) {
    if (receiver->syscall.status == PENDING) return;

    /* Copy the system call struct from the kernel back to user space. */
    uint syscall_paddr = earth->mmu_translate(receiver->pid, SYSCALL_ARG);
    memcpy((void*)syscall_paddr, &receiver->syscall, sizeof(struct syscall));

    /* Set the receiver and sender back to RUNNABLE. */
    proc_set_runnable(receiver->pid);
    proc_set_runnable(receiver->syscall.sender);
}

static void proc_try_syscall(struct process* proc) {
    switch (proc->syscall.type) {
    case SYS_RECV:
        proc_try_recv(proc);
        break;

    case SYS_SEND:
        proc_try_send(proc);
        break;

    case SYS_GETPID: {
        proc->syscall.sender = proc->pid;
        proc->syscall.status = DONE;

        uint syscall_paddr =
            earth->mmu_translate(proc->pid, SYSCALL_ARG);

        memcpy((void*)syscall_paddr,
               &proc->syscall, sizeof(struct syscall));

        proc_set_runnable(proc->pid);
        break;
    }

    default:
        FATAL("proc_try_syscall: unknown syscall type=%d",
              proc->syscall.type);
    }
}