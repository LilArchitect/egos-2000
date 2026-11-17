/* Seminar 5 - A tiny cooperative thread library (book P1, simplified), given */

/* ---------- printf and malloc support (from the egos "thread" branch) ---------- */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static void terminal_write(const char *s, int len) {
    for (int i = 0; i < len; i++) *(char *)(0x10000000UL) = s[i];
}

static void format_to_str(char *out, const char *fmt, va_list args) {
    for (out[0] = 0; *fmt != '\0'; fmt++) {
        if (*fmt != '%') { strncat(out, fmt, 1); continue; }
        fmt++;
        if (*fmt == 's') strcat(out, va_arg(args, char *));
        else if (*fmt == 'd') itoa(va_arg(args, int), out + strlen(out), 10);
    }
}

int printf(const char *format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    format_to_str(buf, format, args);
    va_end(args);
    terminal_write(buf, strlen(buf));
    return 0;
}

extern char __heap_start, __heap_max;
static char *brk = &__heap_start;
char *_sbrk(int size) {
    if (brk + size > (char *)&__heap_max) return NULL;
    char *old = brk;
    brk += size;
    return old;
}

/* Switch QEMU off when the program ends */
static void halt() {
    *(volatile int *)0x100000 = 0x5555;
    while (1);
}

/* ------------------------------ the thread library ------------------------------ */
#include "thread.h"

#define STACK_SIZE (16 * 1024)

#define SCHEDULER 1     /* 0: round robin (as given)
                           1: fixed priority: the READY thread with the lowest tid (Exercise 4) */

enum status { EMPTY, READY, RUNNING, WAITING, DONE };

struct thread {                /* one entry of the TCB (thread control block) */
    enum status status;
    void *sp;                  /* saved stack pointer, while the thread is not running */
    char *stack;               /* its stack (malloc); main uses the initial one */
    void (*entry)(void *);     /* the function it runs ... */
    void *arg;                 /* ... and its argument */
    int joiner;                /* tid of the thread waiting for it in thread_join, or -1 */
};

static struct thread TCB[MAX_THREADS];
static int current = 0;        /* TCB[current] is the running thread */
static int switches = 0;       /* context switches since the start */

void ctx_start(void **old_sp, void *new_sp);    /* context.s */
void ctx_switch(void **old_sp, void *new_sp);   /* context.s */

int thread_self() {
    return current;
}

void thread_init() {
    for (int i = 0; i < MAX_THREADS; i++) TCB[i].status = EMPTY;
    TCB[0].status = RUNNING;
    TCB[0].joiner = -1;
    current = 0;
}

/* Free the stacks of the threads that have finished (never the running one). */
static void free_done_threads() {
    for (int i = 1; i < MAX_THREADS; i++)
        if (TCB[i].status == DONE && i != current) {
            free(TCB[i].stack);
            TCB[i].stack = NULL;
        }
}

/* The scheduler: choose the next thread to run among the READY ones (-1 if none). */
static int next_ready() {
    if (SCHEDULER == 1) {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (TCB[i].status == READY) {
                return i;
            }
        }
        return -1;
    }

    /* Round robin: the first READY thread after the current one. */
    for (int k = 1; k <= MAX_THREADS; k++) {
        int i = (current + k) % MAX_THREADS;
        if (TCB[i].status == READY) return i;
    }
    return -1;
}

/* Save the registers of the current thread on its stack, and restore those of next. */
static void switch_to(int next) {
    int old = current;
    current = next;
    TCB[next].status = RUNNING;
    switches++;
    ctx_switch(&TCB[old].sp, TCB[next].sp);
    /* We get here when some other thread switches back to us. */
    free_done_threads();
}

/* First function of every new thread, on its new stack. */
void ctx_entry() {
    TCB[current].entry(TCB[current].arg);
    thread_exit();
}

int thread_create(void (*entry)(void *), void *arg) {
    int i;
    for (i = 1; i < MAX_THREADS && TCB[i].status != EMPTY; i++);
    if (i == MAX_THREADS) return -1;
    TCB[i].entry = entry;
    TCB[i].arg = arg;
    TCB[i].joiner = -1;
    TCB[i].stack = malloc(STACK_SIZE);

    TCB[current].status = READY;         /* the creator waits: the new thread runs first */
    int old = current;
    current = i;
    TCB[i].status = RUNNING;
    switches++;
    ctx_start(&TCB[old].sp, TCB[i].stack + STACK_SIZE);
    /* We get here when some other thread switches back to the creator. */
    free_done_threads();
    return i;
}

void thread_yield() {
    int next = next_ready();
    if (next < 0) return;                /* nobody else is READY: keep running */
    TCB[current].status = READY;
    switch_to(next);
}

/* Called when no thread is READY: end the program. */
static void no_thread_to_run() {
    int waiting = 0;
    for (int i = 0; i < MAX_THREADS; i++)
        if (TCB[i].status == WAITING) waiting++;
    if (waiting > 0)
        printf("Nobody can run: WAITING threads: %d, and nobody will wake them up.\n", waiting);
    else
        printf("All threads finished after %d context switches.\n", switches);
    halt();
}

void thread_block() {
    TCB[current].status = WAITING;
    int next = next_ready();
    if (next < 0) no_thread_to_run();
    switch_to(next);
}

void thread_unblock(int tid) {
    if (TCB[tid].status == WAITING) TCB[tid].status = READY;
}

/* Wait until thread tid has finished. */
void thread_join(int tid) {
    /* Exercise 5: replace this loop. */
    while (TCB[tid].status != DONE) {
        thread_yield();
    }
}

void thread_exit() {
    TCB[current].status = DONE;
    /* Exercise 5: wake up the thread that waits for this one, if there is one. */

    int next = next_ready();
    if (next < 0) no_thread_to_run();
    int old = current;
    current = next;
    TCB[next].status = RUNNING;
    switches++;
    ctx_switch(&TCB[old].sp, TCB[next].sp);   /* never comes back */
}
