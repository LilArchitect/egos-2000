#include "app.h"

int main(int argc, char** argv) {
    printf("my pid is %d\n\r", sys_getpid());
    return 0;
}