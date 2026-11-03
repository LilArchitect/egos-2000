#include "app.h"

int main(int argc, char** argv) {
    struct ps_reply reply;
    int count = get_ps(&reply);

    printf("PID\tNAME\n\r");

    for (int i = 0; i < count; i++) {
        printf("%d\t%s\n\r",
               reply.procs[i].pid,
               reply.procs[i].name);
    }

    return 0;
}