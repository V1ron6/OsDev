/*
 * mm/constants.h - Memory Management Constants
 *
 * Defines all physical and virtual memory regions for the operating system.
 * This is the foundation for all memory subsystems: PMM, paging, heap, etc.
 *
 * Memory Layout (Current - Identity Mapped):
 * 
 * Physical Address Space:
 * ├─ 0x00000 - 0x00FFF   (4 KiB)  - Real mode IVT / BIOS data
 * ├─ 0x01000 - 0x7BFFF   (506 KiB) - Available low memory
 * ├─ 0x7C000 - 0x7DFFF   (8 KiB)  - Bootloader (loaded by BIOS)
 * ├─ 0x7E000 - 0x9FFFF   (136 KiB) - Free (often used for EBDA)
 * ├─ 0xA0000 - 0xBFFFF   (128 KiB) - Video memory (VGA)
 * ├─ 0xC0000 - 0xFFFFF   (256 KiB) - BIOS ROM, BIOS extensions
 * ├─ 0x100000 - 0xNNNNNN - Kernel image + BSS + heap
 * └─ (Extended memory up to system RAM)
 *
 * Kernel Layout (Starting at 0x10000):
 * ├─ 0x10000 - 0x????? - .text (code/rodata)
 * ├─ 0x????? - 0.????? - .data
 * ├─ 0x????? - 0.????? - .bss (uninitialized)
 * ├─ 0x8F000 - 0x93000 - Kernel stack (16 KiB, grows down)
 * ├─ 0x93000 - 0xA0000 - Free for page tables
 * ├─ 0xA0000 - 0xBFFFF - VGA frame buffer (don't allocate)
 * ├─ 0xC0000 - 0xFFFFF - BIOS ROM (don't allocate)
 * └─ 0x100000+ - Kernel heap + page tables (after PMM reserves regions)
 *
 * Virtual Address Space (Current - Identity Mapped):
 * ├─ 0x00000000 - 0x003FFFFF  - Lower half (1 MB available)
 * ├─ 0x00400000 - 0xBFFFFFFF  - User mode region (future)
 * └─ 0xC0000000 - 0xFFFFFFFF  - Kernel region (future higher-half mapping)
 *
 * Future Higher-Half Kernel:
 * ├─ Virtual 0xC0000000 → Physical 0x00000000
 * ├─ Kernel at virt 0xC0100000, phys 0x00100000
 * ├─ User at 0x00000000 - 0xBFFFFFFF
 * └─ Kernel at 0xC0000000 - 0xFFFFFFFF
 *
 * IMPORTANT INVARIANTS:
 * 1. All addresses are currently identity-mapped (virt == phys)
 * 2. PMM must never allocate bootloader, kernel, or reserved regions
 * 3. Page tables will be allocated in 0x93000-0xA0000 initially
 * 4. Kernel heap grows upward from first free frame after kernel
 * 5. Keep reserved regions away from allocation bitmap
 */

#ifndef MM_CONSTANTS_H
#define MM_CONSTANTS_H

#include <stdint.h>

/* =========================================================================
 * PAGE FRAME CONSTANTS
 * ========================================================================= */

/* Page size: 4 KiB (standard x86 paging) */
#define PAGE_SIZE           0x1000
#define PAGE_SIZE_BITS      12          /* log2(PAGE_SIZE) */
#define PAGE_MASK           0xFFFFF000  /* Mask virtual address to get page base */
#define PAGE_OFFSET_MASK    0x00000FFF  /* Extract offset within page */

/* Convert between addresses and page frame numbers */
#define ADDR_TO_FRAME(addr)   ((addr) >> PAGE_SIZE_BITS)
#define FRAME_TO_ADDR(frame)  ((frame) << PAGE_SIZE_BITS)

/* Align address up to next page boundary */
#define ALIGN_UP_PAGE(addr)   (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

/* Align address down to page boundary */
#define ALIGN_DOWN_PAGE(addr) ((addr) & PAGE_MASK)

/* =========================================================================
 * PHYSICAL MEMORY REGIONS - RESERVED (Never Allocate)
 * ========================================================================= */

/* Real mode interrupt vector table (0x0000 - 0x0FFF) */
#define MEM_RESERVED_IVT_BASE    0x00000000
#define MEM_RESERVED_IVT_END     0x00001000
#define MEM_RESERVED_IVT_SIZE    0x00001000

/* BIOS data area (0x0400 - 0x04FF, but safer to reserve 0x400-0x1000) */
#define MEM_RESERVED_BIOS_DATA   0x00000400
#define MEM_RESERVED_BIOS_END    0x00001000

/* Bootloader (loaded by BIOS at 0x7C00, size 512 bytes, but reserve sector) */
#define MEM_RESERVED_BOOTLOADER_BASE  0x0007C000
#define MEM_RESERVED_BOOTLOADER_END   0x0007E000
#define MEM_RESERVED_BOOTLOADER_SIZE  0x00002000

/* Extended BIOS Data Area (EBDA) - varies, assume 0x7E000-0x80000 */
#define MEM_RESERVED_EBDA_BASE   0x0007E000
#define MEM_RESERVED_EBDA_END    0x00080000
#define MEM_RESERVED_EBDA_SIZE   0x00002000

/* Video RAM (VGA frame buffer at 0xA0000) */
#define MEM_RESERVED_VRAM_BASE   0x000A0000
#define MEM_RESERVED_VRAM_END    0x000C0000
#define MEM_RESERVED_VRAM_SIZE   0x00020000

/* BIOS ROM (0xC0000 - 0xFFFFF) */
#define MEM_RESERVED_BIOS_ROM_BASE  0x000C0000
#define MEM_RESERVED_BIOS_ROM_END   0x00100000
#define MEM_RESERVED_BIOS_ROM_SIZE  0x00040000

/* =========================================================================
 * KERNEL MEMORY REGIONS
 * ========================================================================= */

/* Kernel code starts here (linked by kernel.ld) */
#define KERNEL_VIRT_BASE    0x00010000
#define KERNEL_PHYS_BASE    0x00010000  /* Currently identity-mapped */

/* These are set by kernel.ld during linking */
extern uint8_t __kernel_start;      /* First kernel byte */
extern uint8_t __text_start;        /* Start of .text section */
extern uint8_t __rodata_start;      /* Start of .rodata */
extern uint8_t __data_start;        /* Start of .data */
extern uint8_t __bss_start;         /* Start of .bss */
extern uint8_t __bss_end;           /* End of .bss (last kernel byte) */
extern uint8_t __kernel_stack_top;  /* Top of kernel stack (grows down) */

/* Kernel stack region (16 KiB, grows downward) */
#define KERNEL_STACK_TOP    0x00093000
#define KERNEL_STACK_BASE   0x0008F000
#define KERNEL_STACK_SIZE   0x00004000  /* 16 KiB */

/* Reserved region for page tables during paging setup */
#define MEM_RESERVED_PAGETABLES_BASE  0x00093000
#define MEM_RESERVED_PAGETABLES_END   0x000A0000
#define MEM_RESERVED_PAGETABLES_SIZE  0x0000D000

/* =========================================================================
 * TOTAL SYSTEM MEMORY & PMM CONSTANTS
 * ========================================================================= */

/* Maximum physical memory we support (for PMM bitmap size)
 * This is the upper limit - actual system RAM may be less.
 * We'll detect actual RAM from BIOS or multiboot info.
 * For now, assume 32 MB maximum for boot testing.
 */
#define MEM_MAX_PHYSICAL    0x02000000  /* 32 MB */
#define MEM_MAX_FRAMES      (MEM_MAX_PHYSICAL / PAGE_SIZE)  /* 8192 frames */

/* PMM bitmap properties */
#define PMM_BITMAP_SIZE     (MEM_MAX_FRAMES / 8)  /* Bytes for bitmap (1 bit per frame) */
#define PMM_BITMAP_BASE     0x00093000            /* Placed after kernel stack */

/* First allocatable frame (after all reserved regions) */
#define MEM_FIRST_FREE_FRAME    (ALIGN_UP_PAGE(0x100000) / PAGE_SIZE)

/* =========================================================================
 * MEMORY MANAGEMENT HELPER MACROS
 * ========================================================================= */

/* Check if an address is in a reserved region */
#define IS_RESERVED_IVT(addr)    ((addr) >= MEM_RESERVED_IVT_BASE && (addr) < MEM_RESERVED_IVT_END)
#define IS_RESERVED_BOOTLOAD(addr) ((addr) >= MEM_RESERVED_BOOTLOADER_BASE && (addr) < MEM_RESERVED_BOOTLOADER_END)
#define IS_RESERVED_VRAM(addr)   ((addr) >= MEM_RESERVED_VRAM_BASE && (addr) < MEM_RESERVED_VRAM_END)
#define IS_RESERVED_BIOS(addr)   ((addr) >= MEM_RESERVED_BIOS_ROM_BASE && (addr) < MEM_RESERVED_BIOS_ROM_END)

/* Check if a frame is in kernel region */
#define IS_KERNEL_FRAME(frame) \
    (((frame) * PAGE_SIZE >= KERNEL_PHYS_BASE && \
      (frame) * PAGE_SIZE < (uintptr_t)&__bss_end) || \
     ((frame) * PAGE_SIZE >= KERNEL_STACK_BASE && \
      (frame) * PAGE_SIZE < KERNEL_STACK_TOP))

/* =========================================================================
 * FUTURE HIGHER-HALF KERNEL CONSTANTS
 * ========================================================================= */

/* Kernel virtual base when using higher-half mapping (future)
 * This prepares the architecture for relocating the kernel
 * to 0xC0000000 while keeping user space at 0x00000000.
 */
#define KERNEL_HIGHER_HALF_BASE  0xC0000000

/* Offset to convert physical to virtual in higher-half mode (future) */
#define KERNEL_OFFSET            (KERNEL_HIGHER_HALF_BASE - KERNEL_PHYS_BASE)

/* Macro for future use - currently does nothing (identity mapping) */
#define PHYS_TO_VIRT(phys) ((uintptr_t)(phys))
#define VIRT_TO_PHYS(virt) ((uintptr_t)(virt))

/* =========================================================================
 * PAGING CONSTANTS
 * ========================================================================= */

/* Page directory/table entry size and count */
#define NUM_PAGE_DIR_ENTRIES    1024
#define NUM_PAGE_TABLE_ENTRIES  1024
#define PAGE_DIR_SIZE           (NUM_PAGE_DIR_ENTRIES * 4)  /* 4KB */
#define PAGE_TABLE_SIZE         (NUM_PAGE_TABLE_ENTRIES * 4) /* 4KB */

/* x86 page entry flags (in CR0, CR3, page table entries) */
#define PAGE_PRESENT            0x00000001  /* PTE bit 0: present in memory */
#define PAGE_WRITE              0x00000002  /* PTE bit 1: writable */
#define PAGE_USER               0x00000004  /* PTE bit 2: user accessible */
#define PAGE_PWT                0x00000008  /* PTE bit 3: write through */
#define PAGE_PCD                0x00000010  /* PTE bit 4: cache disabled */
#define PAGE_ACCESSED           0x00000020  /* PTE bit 5: accessed since last refresh */
#define PAGE_DIRTY              0x00000040  /* PTE bit 6: written to since last refresh */
#define PAGE_PAT                0x00000080  /* PTE bit 7: page table attribute */
#define PAGE_GLOBAL             0x00000100  /* PTE bit 8: global (not flushed on CR3 change) */

/* Kernel page: present, writable, not user-accessible */
#define PAGE_KERNEL             (PAGE_PRESENT | PAGE_WRITE)

/* User page: present, writable, user-accessible */
#define PAGE_USER_RW            (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

/* Read-only kernel page */
#define PAGE_KERNEL_RO          (PAGE_PRESENT)

/* Kernel page, no caching (for MMIO regions) */
#define PAGE_KERNEL_NOCACHE     (PAGE_PRESENT | PAGE_WRITE | PAGE_PCD)

#endif /* MM_CONSTANTS_H */
