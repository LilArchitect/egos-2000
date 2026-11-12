#include "app.h"
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: fault load|store|exec|csr\r\n");
        return -1;
    }

    printf("fault: trying '%s'\r\n", argv[1]);

    if (strcmp(argv[1], "load") == 0) {
        int x = *(volatile int*)0x80000000;
        printf("fault: read 0x%x\r\n", x);
    } else if (strcmp(argv[1], "store") == 0) {
        *(volatile int*)0x80000000 = 0;
    } else if (strcmp(argv[1], "exec") == 0) {
        void (*f)() = (void*)0x80000000;
        f();
    } else if (strcmp(argv[1], "csr") == 0) {
        uint m;
        asm volatile("csrr %0, mstatus" : "=r"(m));
        printf("fault: mstatus = 0x%x\r\n", m);
    }

    printf("fault: survived\r\n");
    return 0;
}
