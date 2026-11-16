/* Seminar 5 - A simple queue of thread ids (first in, first out), given */

#include <stdlib.h>
#include "queue.h"

void queue_init(struct queue *q) {
    q->first = NULL;
    q->last = NULL;
}

void queue_push(struct queue *q, int tid) {
    struct node *n = malloc(sizeof(struct node));
    n->tid = tid;
    n->next = NULL;
    if (q->last == NULL) q->first = n;   /* the queue was empty */
    else q->last->next = n;
    q->last = n;
}

int queue_pop(struct queue *q) {
    if (q->first == NULL) return -1;
    struct node *n = q->first;
    int tid = n->tid;
    q->first = n->next;
    if (q->first == NULL) q->last = NULL;   /* the queue is empty now */
    free(n);
    return tid;
}

int queue_empty(struct queue *q) {
    return q->first == NULL;
}
