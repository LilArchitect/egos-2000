#include "thread.h"

void mutex_init(struct mutex *m) {
    m->open = 1;
    queue_init(&m->waiting);
}

void mutex_lock(struct mutex *m) {
    if (m->open == 1) {
        m->open = 0;
    } else {
        queue_push(&m->waiting, thread_self());
        thread_block();
    }
}

void mutex_unlock(struct mutex *m) {
    if (queue_empty(&m->waiting)) {
        m->open = 1;
    } else {
        int next_tid = queue_pop(&m->waiting);
        thread_unblock(next_tid);
    }
}