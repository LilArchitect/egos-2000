/*
 * iobound — an I/O-bound program for Seminar 3.
 *
 * usage: iobound [TIMES]
 * Reads the first block of README and prints one short line, TIMES times
 * (0 or no argument: forever). Almost all its time is spent waiting for
 * sys_file and sys_terminal: it makes system calls all the time.
 */

#include "app.h"
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int times = (argc > 1) ? atoi(argv[1]) : 0;

    int ino = dir_lookup(workdir_ino, "README");

    if (ino < 0) {
        INFO("iobound: README not found (run it from /home/yunhao)");
        return -1;
    }

    char buf[BLOCK_SIZE];

    for (int i = 0; times == 0 || i < times; i++) {
        file_read(ino, 0, buf);
        printf("iobound: read %d of README\r\n", i);
    }

    printf("iobound: done\r\n");

    return 0;
}