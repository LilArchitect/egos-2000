/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: functions for memory management unit (MMU)
 * This MMU code contains a simple memory allocation/free mechanism
 * and two memory translation mechanisms: software TLB and page table.
 * It also contains a hardware-specific cache flushing function.
 *
 * Seminar 4 version: page_table_map() and page_table_switch() build and
 * install real page tables for every process, the PMP lets the CPU read
 * them, page_table_translate() does the translation of theory §3 in C, and
 * pt_dump() prints the tables of every process that ends. You
 * complete mmu_free() (Exercise 1) and page_table_free() (Exercise 5).
 */

#include "egos.h"
#include "disk.h"
#include "servers.h"
#include <string.h>

/* Seminar 4: set to 0 to stop printing the page tables of every process that ends */
#define PT_DUMP 1
/* Seminar 4, Exercise 5: set to 1 when page_table_free() is complete */
#define PT_FREE 0
void pt_dump(int pid);
void page_table_free(int pid);

/* Memory allocation and free */
#define APPS_PAGES_CNT (RAM_END - APPS_PAGES_BASE) / PAGE_SIZE
struct page_info {
    int use;       /* Is this page free or allocated? */
    int pid;       /* Which process owns this page? */
    uint vpage_no; /* Which virtual page in this process maps to this physical
                      page? */
} page_info_table[APPS_PAGES_CNT];

uint mmu_alloc() {
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (!page_info_table[i].use) {
            page_info_table[i].use = 1;
            return i;
        }
    FATAL("mmu_alloc: no more free memory");
}

void mmu_free(int pid) {
    int by_tables = PT_FREE && earth->translation == PAGE_TABLE &&
                    pid >= GPID_USER_START;

    if (PT_DUMP && earth->translation == PAGE_TABLE && pid >= GPID_USER_START)
        pt_dump(pid);

    /* Seminar 4, Exercise 1: print one line for each page of pid (its
     * virtual page and its frame), and then the number of pages released. */

    /* Seminar 4, Exercise 5: release the pages by following the page tables */
    if (by_tables) page_table_free(pid);

    /* Release the pages of pid. With PT_FREE, this loop only finds the pages
     * that page_table_free() did not release, and says how many. */
    uint left = 0;
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == pid) {
            memset(&page_info_table[i], 0, sizeof(struct page_info));
            left++;
        }
    if (by_tables && left)
        CRITICAL("mmu_free: page_table_free forgot %d pages of process %d", left, pid);
}

/* Software TLB Translation */
void soft_tlb_map(int pid, uint vpage_no, uint ppage_id) {
    page_info_table[ppage_id].pid      = pid;
    page_info_table[ppage_id].vpage_no = vpage_no;
}

void soft_tlb_switch(int pid) {
    static int curr_vm_pid = -1;
    if (pid == curr_vm_pid) return;

    /* Unmap curr_vm_pid from the user address space */
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == curr_vm_pid)
            memcpy(PAGE_ID_TO_ADDR(i),
                   PAGE_NO_TO_ADDR(page_info_table[i].vpage_no), PAGE_SIZE);

    /* Map pid to the user address space */
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == pid)
            memcpy(PAGE_NO_TO_ADDR(page_info_table[i].vpage_no),
                   PAGE_ID_TO_ADDR(i), PAGE_SIZE);

    curr_vm_pid = pid;
}

uint soft_tlb_translate(int pid, uint vaddr) {
    soft_tlb_switch(pid);
    return vaddr;
}

/* Page Table Translation
 *
 * RISC-V Sv32: a virtual address has three parts,
 *   VPN[1] (bits 31-22) | VPN[0] (bits 21-12) | offset (bits 11-0)
 * VPN[1] selects an entry of the root table, which points to a leaf table;
 * VPN[0] selects an entry of the leaf table, which points to the frame.
 * A page table entry (PTE) is (PPN << 10) | flags, with the flags
 * V=0x1 R=0x2 W=0x4 X=0x8 U=0x10 A=0x40 D=0x80.
 */

#define USER_RWX     (0xC0 | 0x1F)
#define PTE_VALID    0x1
#define MAX_NPROCESS 256
#define WORK_DIR     (SYSCALL_ARG + PAGE_SIZE) /* shared with the shell, see apps/app.h */
static uint* root;
static uint* leaf;
static uint* pid_to_pagetable_base[MAX_NPROCESS];
/* Assume at most MAX_NPROCESS unique processes for simplicity */

void setup_identity_region(int pid, uint addr, uint npages, uint flag) {
    uint vpn1 = addr >> 22;

    if (root[vpn1] & 0x1) {
        /* Leaf has been allocated */
        leaf = (void*)((root[vpn1] << 2) & 0xFFFFF000);
    } else {
        /* Leaf has not been allocated */
        uint ppage_id                 = earth->mmu_alloc();
        leaf                          = (void*)PAGE_ID_TO_ADDR(ppage_id);
        page_info_table[ppage_id].pid = pid;
        memset(leaf, 0, PAGE_SIZE);
        root[vpn1] = ((uint)leaf >> 2) | 0x1;
    }

    /* Setup the entries in the leaf page table */
    uint vpn0 = (addr >> 12) & 0x3FF;
    for (uint i = 0; i < npages; i++)
        leaf[vpn0 + i] = ((addr + i * PAGE_SIZE) >> 2) | flag;
}

void pagetable_identity_map(int pid) {
    /* Allocate the root page table and set the page table base (satp) */
    uint ppage_id = earth->mmu_alloc();
    root          = (void*)PAGE_ID_TO_ADDR(ppage_id);
    memset(root, 0, PAGE_SIZE);
    page_info_table[ppage_id].pid = pid;
    pid_to_pagetable_base[pid]    = root;

    /* Allocate the leaf page tables */
    for (uint i = RAM_START; i < RAM_END; i += PAGE_SIZE * 1024)
        setup_identity_region(pid, i, 1024, USER_RWX);    /* RAM   */
    setup_identity_region(pid, CLINT_BASE, 16, USER_RWX); /* CLINT */
    setup_identity_region(pid, UART_BASE, 1, USER_RWX);   /* UART  */
    setup_identity_region(pid, SPI_BASE, 1, USER_RWX);    /* SPI   */

    if (earth->platform == ARTY) {
        setup_identity_region(pid, BOARD_FLASH_ROM, 1024, USER_RWX); /* ROM */
        setup_identity_region(pid, ETHMAC_CSR_BASE, 1, USER_RWX);
        setup_identity_region(pid, ETHMAC_TX_BUFFER, 1, USER_RWX);
        setup_identity_region(pid, ETHMAC_RX_BUFFER, 1, USER_RWX);
    }
}

void page_table_map(int pid, uint vpage_no, uint ppage_id) {
    if (pid >= MAX_NPROCESS) FATAL("page_table_map: pid too large");

    /* First mapping of this process: create its root table. A server
     * (pid < GPID_USER_START) can see all the memory (identity map); a user
     * process can only see the work directory page, shared with the shell. */
    if (!pid_to_pagetable_base[pid]) {
        if (pid < GPID_USER_START) {
            pagetable_identity_map(pid);
        } else {
            uint root_id = earth->mmu_alloc();
            root         = (void*)PAGE_ID_TO_ADDR(root_id);
            memset(root, 0, PAGE_SIZE);
            page_info_table[root_id].pid = pid;
            pid_to_pagetable_base[pid]   = root;
            setup_identity_region(pid, WORK_DIR, 1, USER_RWX);
        }
    }

    /* Find the leaf table (create it if needed) and fill the entry */
    root      = pid_to_pagetable_base[pid];
    uint vpn1 = vpage_no >> 10;
    uint vpn0 = vpage_no & 0x3FF;
    if (!(root[vpn1] & PTE_VALID)) {
        uint leaf_id = earth->mmu_alloc();
        leaf         = (void*)PAGE_ID_TO_ADDR(leaf_id);
        memset(leaf, 0, PAGE_SIZE);
        page_info_table[leaf_id].pid = pid;
        root[vpn1]                   = ((uint)leaf >> 2) | PTE_VALID;
    }
    leaf       = (void*)((root[vpn1] >> 10) << 12);
    leaf[vpn0] = ((uint)PAGE_ID_TO_ADDR(ppage_id) >> 2) | USER_RWX;

    page_info_table[ppage_id].pid      = pid;
    page_info_table[ppage_id].vpage_no = vpage_no;
}

void page_table_switch(int pid) {
    /* satp = MODE bit (Sv32) | physical page number of the root table */
    asm("csrw satp, %0" ::"r"(((uint)pid_to_pagetable_base[pid] >> 12) | (1 << 31)));
}

uint page_table_translate(int pid, uint vaddr) {
    /* Translate vaddr with the page tables of process pid, as in theory §3.
     * Return the physical address, or 0 if vaddr is not mapped. */

    // Split the virtual address
    uint vpn1   = vaddr >> 22;                  // bits 31-22 of vaddr
    uint vpn0   = (vaddr >> 12) & 0x3FF;        // bits 21-12 of vaddr
    uint offset = vaddr & 0xFFF;                // bits 11-0 of vaddr

    // Step 0: the root table of pid (the kernel keeps it, satp points to it)
    uint* root_table = pid_to_pagetable_base[pid];

    // Step 1: the entry of the root table chosen by VPN[1]
    uint entry = root_table[vpn1];
    if (!(entry & PTE_VALID)) return 0;         // V = 0: not valid
    uint* leaf_table = (uint*)((entry >> 10) << 12); // PPN -> address of the leaf table

    // Step 2: the entry of the leaf table chosen by VPN[0]
    entry = leaf_table[vpn0];
    if (!(entry & PTE_VALID)) return 0;         // V = 0: not valid
    uint frame = (entry >> 10) << 12;           // PPN -> address of the frame

    // Step 3: the frame plus the offset
    return frame + offset;
}

/* Seminar 4, Exercise 5: release all the pages of process pid by following
 * its page tables from the root, instead of looking at every frame of
 * page_info_table. free_frame() releases one frame, given its address. */
static uint frames_freed;

static void free_frame(uint paddr) {
    memset(&page_info_table[(paddr - APPS_PAGES_BASE) / PAGE_SIZE], 0,
           sizeof(struct page_info));
    frames_freed++;
}

void page_table_free(int pid) {
    frames_freed = 0;

    /* Delete the two lines marked DELETE, and replace every ___ with an
     * expression (they are like the expressions of page_table_translate).
     * Then set PT_FREE to 1 at the beginning of this file. */

    /* DELETE this line
    uint* root_table = pid_to_pagetable_base[pid];
    for (uint i = 0; i < 1024; i++) {
        if (___) continue;                      // root entry i is not valid
        uint* leaf_table = (uint*)(___);        // address of its leaf table
        for (uint j = 0; j < 1024; j++) {
            if (___) continue;                  // leaf entry j is not valid
            uint frame = ___;                   // address of its frame
            if (frame >= APPS_PAGES_BASE)       // why? (Exercise 5)
                free_frame(frame);
        }
        free_frame((uint)leaf_table);           // the leaf table itself
    }
    free_frame((uint)root_table);               // the root table itself
    pid_to_pagetable_base[pid] = 0;
    DELETE this line */

    INFO("page_table_free released %d pages of process %d", frames_freed, pid);
}

/* Print the page tables of a process, and what page_table_translate()
 * returns for a few addresses */
void pt_dump(int pid) {
    uint* r = pid_to_pagetable_base[pid];
    printf("--- page tables of process %d: satp=0x%x\r\n", pid,
           ((uint)r >> 12) | (1 << 31));
    for (uint i = 0; i < 1024; i++) {
        if (!r[i]) continue;
        printf("root @0x%x  entry 0x%x: 0x%x\r\n", (uint)r, i, r[i]);
        uint* l = (void*)((r[i] >> 10) << 12);
        for (uint j = 0; j < 1024; j++)
            if (l[j])
                printf("  leaf @0x%x  entry 0x%x: 0x%x\r\n", (uint)l, j, l[j]);
    }

    uint test[] = {APPS_ENTRY, SYSCALL_ARG, APPS_STACK_TOP - 16, RAM_START};
    for (uint i = 0; i < 4; i++)
        printf("translate(0x%x) = 0x%x\r\n", test[i],
               page_table_translate(pid, test[i]));
}

void flush_cache() {
    if (earth->platform == ARTY) {
        /* Flush the L1 instruction cache */
        /* See
         * https://github.com/yhzhang0128/litex/blob/egos/litex/soc/cores/cpu/vexriscv_smp/system.h#L9-L25
         */
        asm(".word(0x100F)\nnop\nnop\nnop\nnop\nnop\n");
    }
    if (earth->translation == PAGE_TABLE) {
        /* Flush the TLB, which is the cache of page table entries */
        /* See
         * https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf#subsection.4.2.1
         */
        asm("sfence.vma zero,zero");
    }
}

/* MMU Initialization */
void mmu_init() {
    earth->mmu_free        = mmu_free;
    earth->mmu_alloc       = mmu_alloc;
    earth->mmu_flush_cache = flush_cache;

    /* Setup a PMP region for the whole 4GB address space */
    asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
    asm("csrw pmpcfg0, %0" : : "r"(0xF));

    /* Setup PMP NAPOT region 0x80400000 - 0x80800000 as r/w/x */
    uint pmp_napot_addr = (APPS_ENTRY >> 2) | (((1 << 21) - 1) >> 2);
    asm("csrw pmpaddr0, %0" : : "r"(pmp_napot_addr));
    asm("csrw pmpcfg0, %0" : : "r"(0x1F));

    /* Choose memory translation mechanism in QEMU */
    CRITICAL("Choose a memory translation mechanism:");
    printf("Enter 0: page tables\r\nEnter 1: software TLB\r\n");

    char buf[2];
    for (buf[0] = 0; buf[0] != '0' && buf[0] != '1'; earth->tty_read(&buf[0]));
    earth->translation = (buf[0] == '0') ? PAGE_TABLE : SOFT_TLB;
    INFO("%s translation is chosen",
         earth->translation == PAGE_TABLE ? "Page table" : "Software");

    if (earth->translation == PAGE_TABLE) {
        /* The page tables are allocated above 0x80800000, outside the PMP
         * region: open the PMP again so that the CPU can read them. With
         * page tables, the tables themselves protect the memory. */
        asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
        asm("csrw pmpcfg0, %0" : : "r"(0xF));

        /* Setup an identity mapping using page tables */
        pagetable_identity_map(0);
        asm("csrw satp, %0" ::"r"(((uint)root >> 12) | (1 << 31)));

        earth->mmu_map       = page_table_map;
        earth->mmu_switch    = page_table_switch;
        earth->mmu_translate = page_table_translate;
    } else {
        earth->mmu_map       = soft_tlb_map;
        earth->mmu_switch    = soft_tlb_switch;
        earth->mmu_translate = soft_tlb_translate;
    }
}