#pragma once

#include "servers.h"
#include <string.h>

enum syscall_type {
    SYS_RECV = 1,
    SYS_SEND = 2,
    SYS_GETPID = 3,
};

#define SYSCALL_MSG_LEN 1024
struct syscall {
    enum syscall_type type; /* SYS_SEND, SYS_RECV or SYS_GETPID*/
    int sender;             /* sender process ID    */
    int receiver;           /* receiver process ID  */
    char content[SYSCALL_MSG_LEN];
    enum { PENDING, DONE } status;
};

void sys_send(int receiver, char* msg, uint size);
void sys_recv(int from, int* sender, char* buf, uint size);
int sys_getpid(void);