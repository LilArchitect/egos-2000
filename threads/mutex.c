/* Seminar 5 - The mutex: this is the file you write */

#include "thread.h"

void mutex_init(struct mutex *m) {
    /* Exercise 2: the mutex starts free, with nobody waiting. */

}

void mutex_lock(struct mutex *m) {
    /* Exercise 2:
     * - If the mutex is free, take it and return.
     * - If not, add your tid at the END of tid_waiting and block.
     *   When you wake up, mutex_unlock has already given the mutex to you. */

}

void mutex_unlock(struct mutex *m) {
    /* Exercise 2:
     * - If nobody waits, the mutex becomes free.
     * - If not, take the LAST tid of tid_waiting and unblock it.
     *   The mutex goes directly to that thread: do not make it free. */

}
