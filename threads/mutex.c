/* Seminar 5 - The mutex: this is the file you write */

#include "thread.h"

void mutex_init(struct mutex *m) {
    m->open = 1;
    m->n_threads_waiting = 0;
}

void mutex_lock(struct mutex *m) {
    if (m->open == 1) {
        m->open = 0;
    } else {
        m->tid_waiting[m->n_threads_waiting] = thread_self();
        m->n_threads_waiting++;
        thread_block();
    }
}

void mutex_unlock(struct mutex *m) {
    if (m->n_threads_waiting == 0) {
        m->open = 1;
    } else {
        m->n_threads_waiting--;
        int next_tid = m->tid_waiting[m->n_threads_waiting];
        thread_unblock(next_tid);
    }
}
