# Seminar 5: a small multi-threaded program (book P1, simplified).
# It runs alone on the emulated RISC-V machine: there is no egos kernel underneath.
#   make        compile
#   make qemu   compile and run (if it hangs, quit with Ctrl-A and then X)

QEMU     = qemu-system-riscv32
RISCV_CC = riscv-none-elf-gcc
OBJCOPY  = riscv-none-elf-objcopy
CFLAGS   = -march=rv32ima_zicsr -mabi=ilp32 -Wl,--gc-sections -ffunction-sections -fdata-sections -fno-builtin
LDFLAGS  = -nostdlib -lc -lgcc
SRC      = thread.s context.s thread.c queue.c mutex.c main.c

all:
	$(RISCV_CC) $(CFLAGS) $(SRC) -Tthread.lds $(LDFLAGS) -o thread.elf
	$(OBJCOPY) -O binary thread.elf thread.bin

qemu: all
	$(QEMU) -nographic -machine virt -smp 1 -bios thread.bin

clean:
	rm -f thread.elf thread.bin
