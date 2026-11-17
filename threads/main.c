/* Seminar 5 - The increment contest (lecture T07), with 4 threads */

#include "thread.h"

#define USE_MUTEX 0     /* 0: no mutex (Exercises 1 and 4)
                           1: mutex_lock / mutex_unlock (Exercises 2, 3 and 5) */
#define N_THREADS 4
#define ROUNDS    5

int sum = 0;            /* shared by all the threads */
struct mutex m;         /* shared by all the threads */

void increment(void *arg) {
    char *name = arg;
    for (int i = 0; i < ROUNDS; i++) {
        if (USE_MUTEX) mutex_lock(&m);
        int tmp = sum;          /* sum = sum + 1, in two steps: load ...           */
        thread_yield();         /* (here the thread could lose the CPU)            */
        sum = tmp + 1;          /*                              ... add and store  */
        printf("%s: sum = %d\n", name, sum);
        if (USE_MUTEX) mutex_unlock(&m);
        thread_yield();         /* let the others run before the next round */
    }
}

int main() {
    thread_init();
    mutex_init(&m);

    char *names[N_THREADS] = {"A", "B", "C", "D"};
    int tids[N_THREADS];
    for (int i = 0; i < N_THREADS; i++)
        tids[i] = thread_create(increment, names[i]);

    for (int i = 0; i < N_THREADS; i++)
        thread_join(tids[i]);

    printf("Final sum: %d (expected %d)\n", sum, N_THREADS * ROUNDS);
    thread_exit();
}
