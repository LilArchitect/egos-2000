/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: the process management system server
 * Handle the creation and termination of other processes.
 */

#include "app.h"
#include "elf.h"
#include "disk.h"

struct process_entry {
    int pid;
    int is_active;
    char name[CMD_ARG_LEN];
};

static struct process_entry proc_table[PS_MAX];

static void add_proc(int pid, const char* name) {
    for (int i = 0; i < PS_MAX; i++) {
        if (!proc_table[i].is_active) {
            proc_table[i].pid = pid;
            strncpy(proc_table[i].name, name, CMD_ARG_LEN - 1);
            proc_table[i].name[CMD_ARG_LEN - 1] = '\0';
            proc_table[i].is_active = 1;
            return;
        }
    }
}

static void remove_proc(int pid) {
    for (int i = 0; i < PS_MAX; i++) {
        if (proc_table[i].is_active && proc_table[i].pid == pid) {
            proc_table[i].is_active = 0;
            return;
        }
    }
}

static int app_ino, app_pid;
static void sys_spawn(uint base);
static int app_spawn(struct proc_request* req);

/* See earth/boot.s for boot_lock and booted_core_cnt. */
struct multicore {
    int boot_lock;
    int booted_core_cnt;
};

int main(int unused, struct multicore* boot) {
    SUCCESS("Enter kernel process GPID_PROCESS");

    add_proc(GPID_PROCESS, "sys_process");

    /* Student's code goes here (Multicore & Locks). */

    /* Release the boot lock, so the other 3 cores can enter the
     * boot loader. Then wait for all 4 cores to finish booting. */

    /* Student's code ends here. */

    int sender, shell_waiting;
    char buf[SYSCALL_MSG_LEN];

    // We create a variable to count the amount of requests
    int request_count = 0;

    sys_spawn(SYS_TERM_EXEC_START);
    grass->sys_recv(GPID_TERMINAL, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_process receives: %s", buf);

    sys_spawn(SYS_FILE_EXEC_START);
    grass->sys_recv(GPID_FILE, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_process receives: %s", buf);

    sys_spawn(SYS_SHELL_EXEC_START);

    while (1) {
        struct proc_request* req = (void*)buf;
        struct proc_reply* reply = (void*)buf;
        grass->sys_recv(GPID_ALL, &sender, buf, SYSCALL_MSG_LEN);

        // Request counter handler
        request_count++;

        if (request_count % 10 == 0) {
            INFO("[sys proc] handled %d requests", request_count);
        }

        switch (req->type) {
        case PROC_SPAWN:
            reply->type = app_spawn(req);

            shell_waiting =
                (req->argv[req->argc - 1][0] != '&') && (reply->type == CMD_OK);
            if (!shell_waiting && reply->type == CMD_OK)
                INFO("process %d running in the background", app_pid);
            grass->sys_send(GPID_SHELL, (void*)reply, sizeof(*reply));
            break;
        case PROC_EXIT:
            grass->proc_free(sender);
            remove_proc(sender);

            if (shell_waiting && app_pid == sender)
                grass->sys_send(GPID_SHELL, (void*)reply, sizeof(*reply));
            else if (app_pid == sender)
                INFO("background process %d terminated", sender);
            break;
        case PROC_KILLALL:
            grass->proc_free(GPID_ALL);

            for (int i = 0; i < PS_MAX; i++) {
                if (proc_table[i].is_active &&
                    proc_table[i].pid >= GPID_USER_START) {
                    proc_table[i].is_active = 0;
                }
            }
            break;
        case PROC_GETPID: {
            struct proc_getpid_reply reply;

            reply.pid = sender;

            grass->sys_send(sender, (void*)&reply, sizeof(reply));
            break;
        }
        case PROC_PS: {
            struct ps_reply ps = {0};
        
            for (int i = 0; i < PS_MAX; i++) {
                if (proc_table[i].is_active) {
                    ps.procs[ps.count].pid = proc_table[i].pid;
                    strncpy(ps.procs[ps.count].name,
                            proc_table[i].name, CMD_ARG_LEN);
                    ps.count++;
                }
            }
        
            grass->sys_send(sender, (char*)&ps, sizeof(ps));
            break;
        }        
        case PROC_SLEEP:
            grass->proc_sleep(sender, req->sleep_usecs);
            break;
        /* Student's code goes here (System Call & Protection). */

        /* Add a case that handles process sleep. */

        /* Student's code ends here. */
        default:
            FATAL("sys_process: invalid request %d", req->type);
        }
    }
}

static void app_read(uint off, char* dst) { file_read(app_ino, off, dst); }

static int app_spawn(struct proc_request* req) {
    int bin_ino = dir_lookup(0, "bin/");
    if ((app_ino = dir_lookup(bin_ino, req->argv[0])) < 0) return CMD_ERROR;
    int argc = req->argv[req->argc - 1][0] == '&' ? req->argc - 1 : req->argc;

    app_pid = grass->proc_alloc();
    elf_load(app_pid, app_read, argc, (void**)req->argv);
    add_proc(app_pid, req->argv[0]);
    grass->proc_set_ready(app_pid);

    return CMD_OK;
}

static int sys_apps_base;
char* sys_apps[] = {"sys_process", "sys_terminal", "sys_file", "sys_shell"};

static void sys_proc_read(uint block_no, char* dst) {
    earth->disk_read(sys_apps_base + block_no, 1, dst);
}

static void sys_spawn(uint base) {
    int pid = grass->proc_alloc();
    INFO("Load kernel process #%d: %s", pid, sys_apps[pid - 1]);

    sys_apps_base = base;
    elf_load(pid, sys_proc_read, 0, NULL);
    add_proc(pid, sys_apps[pid - 1]);
    grass->proc_set_ready(pid);
}
