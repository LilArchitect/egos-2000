/*
 * cpubound — a CPU-bound program for Seminar 3.
 *
 * usage: cpubound [ROUNDS] [-s] [-w MS]
 * Computes for ROUNDS rounds (0 or no argument: forever).
 * With -w MS it first sleeps MS milliseconds (of egos time).
 * It prints a line when it starts and when it ends and, unless -s
 * (silent) is given, one line after each round. With -s it makes NO
 * system call while it computes: the only way the kernel can take the
 * CPU away from it is a timer interrupt.
 */

#include "app.h"
#include <string.h>
#include <stdlib.h>

#define ROUND_WORK 20000000

int main(int argc, char** argv) {
    int rounds = 0, silent = 0, wait_ms = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            silent = 1;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            wait_ms = atoi(argv[++i]);
        } else {
            rounds = atoi(argv[i]);
        }
    }

    if (wait_ms > 0) {
        sleep(wait_ms * 1000);
    }

    printf("cpubound: start\r\n");

    volatile uint x = 0;

    for (int r = 0; rounds == 0 || r < rounds; r++) {
        for (uint j = 0; j < ROUND_WORK; j++) {
            x += j; /* pure computation */
        }

        if (!silent) {
            printf("cpubound: round %d\r\n", r);
        }
    }

    printf("cpubound: done\r\n");

    return 0;
}