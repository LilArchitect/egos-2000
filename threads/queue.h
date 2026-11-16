/* Seminar 5 - A simple queue of thread ids (first in, first out), given */

struct node {
    int tid;
    struct node *next;
};

struct queue {
    struct node *first;    /* the next one to leave */
    struct node *last;     /* the last one that arrived */
};

void queue_init(struct queue *q);            /* empty queue */
void queue_push(struct queue *q, int tid);   /* add tid at the end */
int  queue_pop(struct queue *q);             /* remove and return the first tid (-1 if empty) */
int  queue_empty(struct queue *q);           /* 1 if empty, 0 if not */
