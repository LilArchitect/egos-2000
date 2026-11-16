/* Seminar 5 - A tiny cooperative thread library (book P1, simplified) */

#include "queue.h"

#define MAX_THREADS 16

/* ---- The thread library (thread.c), given ---- */
void thread_init();                                    /* main becomes thread 0 */
int  thread_create(void (*entry)(void *), void *arg);  /* like pthread_create; returns the tid */
void thread_yield();                                   /* let the next READY thread run */
void thread_exit();                                    /* like pthread_exit */
void thread_join(int tid);                             /* like pthread_join (Exercise 5) */

/* Three functions to build a mutex with */
int  thread_self();                 /* tid of the running thread */
void thread_block();                /* running thread -> WAITING, and switch to another one */
void thread_unblock(int tid);       /* thread tid: WAITING -> READY (no switch) */

int  printf(const char *format, ...);   /* only %d and %s */

/* ---- The mutex (mutex.c), Exercises 2 and 3 ---- */
struct mutex {
    int open;                       /* 1 = free, 0 = taken */
    int n_threads_waiting;          /* how many tids are in tid_waiting */
    int tid_waiting[MAX_THREADS];   /* the threads that wait for the mutex */
};

void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);     /* Exercises 2 and 3 */
void mutex_unlock(struct mutex *m);
