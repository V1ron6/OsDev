/*
 * mm/paging.c - x86 Paging Implementation
 *
 * Implements 32-bit x86 virtual memory through page tables.
 * 
 * Key design:
 * - Kernel page directory statically allocated at compile time
 * - Page tables allocated from PMM as needed
 * - Identity mapping: virtual address == physical address
 * - Currently no demand paging or COW
 *
 * Layout:
 * - Kernel page directory: 4KB, contains 1024 PDEs
 * - Each page table: 4KB, contains 1024 PTEs
 * - Maps up to 4GB virtual address space (1024 * 1024 * 4KB)
 */

#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/constants.h"
#include "x86.h"
#include "serial.h"
#include "vga.h"
#include "panic.h"
#include <string.h>

/* =========================================================================
 * INTERNAL STATE
 * ========================================================================= */

/* Kernel page directory (aligned to 4KB boundary)
 * Placed in .bss section by linker
 */
static page_directory_t kernel_page_dir __attribute__((aligned(4096)));

/* Track paging initialization state */
static bool paging_enabled = false;
static bool paging_initialized = false;

/* =========================================================================
 * HELPER FUNCTIONS
 * ========================================================================= */

/**
 * _get_or_create_page_table() - Get PDE for directory index, creating table if needed
 *
 * Given a page directory index (0-1023), return the PDE.
 * If the PDE points to a valid page table, return it.
 * If not, allocate a new page table from PMM.
 *
 * Returns: Pointer to page_dir_entry_t, or NULL if allocation failed
 */
static page_dir_entry_t *_get_or_create_pde(uint32_t dir_idx) {
    if (dir_idx >= 1024) {
        return NULL;  /* Invalid index */
    }
    
    page_dir_entry_t *pde = &kernel_page_dir.entries[dir_idx];
    
    /* If PDE already points to a page table, return it */
    if (pde->value & PAGE_PRESENT) {
        return pde;
    }
    
    /* Allocate new page table from PMM */
    uint32_t table_phys = pmm_alloc_frame();
    if (table_phys == 0) {
        serial_puts("[ERROR] paging: Out of memory allocating page table\n");
        return NULL;
    }
    
    /* Zero the page table */
    memset((void *)table_phys, 0, PAGE_SIZE);
    
    /* Create PDE pointing to page table
     * Kernel pages: present + writable, no user access
     */
    pde->value = table_phys | PAGE_KERNEL;
    
    return pde;
}

/**
 * _get_page_table() - Get page table from PDE, return NULL if not present
 *
 * Unlike _get_or_create_pde, this does NOT allocate.
 * Used for reading existing mappings.
 */
static page_table_t *_get_page_table(uint32_t dir_idx) {
    if (dir_idx >= 1024) {
        return NULL;
    }
    
    page_dir_entry_t *pde = &kernel_page_dir.entries[dir_idx];
    
    if (!(pde->value & PAGE_PRESENT)) {
        return NULL;  /* No page table allocated */
    }
    
    return (page_table_t *)(PDE_GET_TABLE(*pde));
}

/* =========================================================================
 * PAGING PUBLIC API
 * ========================================================================= */

void paging_init(void) {
    if (paging_initialized) {
        return;  /* Already initialized */
    }
    
    serial_puts("[PAGING] Initializing paging infrastructure\n");
    vga_puts("[PAGING] Initializing...\n");
    
    /* Clear page directory to zero
     * All entries start as "not present"
     */
    memset(&kernel_page_dir, 0, PAGE_SIZE);
    
    /* Create identity mappings for kernel and critical regions
     * This ensures:
     * 1. Kernel code/data remains accessible
     * 2. VGA memory readable/writable
     * 3. Low memory regions mapped
     * 4. Page tables themselves mapped
     */
    
    /* Map kernel region (0x10000 and beyond)
     * Read-only code, read-write data
     */
    for (uint32_t addr = KERNEL_PHYS_BASE; 
         addr <= (uintptr_t)&__bss_end; 
         addr += PAGE_SIZE) {
        paging_map_page(addr, addr, PAGE_KERNEL);
    }
    
    /* Map kernel stack */
    for (uint32_t addr = KERNEL_STACK_BASE; 
         addr < KERNEL_STACK_TOP; 
         addr += PAGE_SIZE) {
        paging_map_page(addr, addr, PAGE_KERNEL);
    }
    
    /* Map low 1MB (includes IVT, BIOS data, bootloader)
     * These regions should be mapped but protected
     * Map as read-only to catch accidental writes
     */
    for (uint32_t addr = 0; addr < 0x100000; addr += PAGE_SIZE) {
        if (addr >= KERNEL_PHYS_BASE && addr < (uintptr_t)&__bss_end)
            continue;  /* Already mapped kernel */
        if (addr >= KERNEL_STACK_BASE && addr < KERNEL_STACK_TOP)
            continue;  /* Already mapped stack */
        
        /* Check if this is reserved (IVT, bootloader, BIOS, etc.) */
        bool is_reserved = false;
        if (addr < 0x001000) is_reserved = true;  /* IVT */
        if (addr >= 0x07C000 && addr < 0x080000) is_reserved = true;  /* Bootloader + EBDA */
        if (addr >= 0x0A0000 && addr < 0x100000) is_reserved = true;  /* VGA + BIOS ROM */
        
        if (is_reserved) {
            /* Map reserved regions as read-only */
            paging_map_page(addr, addr, PAGE_KERNEL_RO);
        } else {
            /* Map free regions as writable */
            paging_map_page(addr, addr, PAGE_KERNEL);
        }
    }
    
    serial_printf("[PAGING] Page directory at 0x%x\n", (uintptr_t)&kernel_page_dir);
    serial_puts("[PAGING] Initialized - identity mapping ready\n");
    vga_puts("[PAGING] Ready (not yet enabled)\n");
    
    paging_initialized = true;
}

void paging_enable(void) {
    if (!paging_initialized) {
        kernel_panic("paging_enable() called before paging_init()");
    }
    
    if (paging_enabled) {
        return;  /* Already enabled */
    }
    
    serial_puts("[PAGING] Enabling paging...\n");
    
    /* Load page directory address into CR3 */
    uint32_t pdir_phys = (uintptr_t)&kernel_page_dir;
    paging_load_page_dir(pdir_phys);
    
    /* Enable paging by setting CR0.PG bit
     * CR0 = 0x00000001 | 0x80000000 (PE | PG)
     */
    uint32_t cr0 = read_cr0();
    cr0 |= 0x80000000;  /* Set PG (bit 31) */
    write_cr0(cr0);
    
    /* Flush TLB to ensure fresh translation
     * This reloads CR3, discarding all TLB entries
     */
    write_cr3(pdir_phys);
    
    serial_puts("[PAGING] Paging enabled!\n");
    serial_printf("[PAGING] CR0 = 0x%x (PG bit set)\n", read_cr0());
    serial_printf("[PAGING] CR3 = 0x%x\n", read_cr3());
    
    vga_puts("[PAGING] Paging enabled\n");
    
    paging_enabled = true;
}

void paging_disable(void) {
    if (!paging_enabled) {
        return;  /* Not enabled */
    }
    
    serial_puts("[WARN] Disabling paging...\n");
    
    uint32_t cr0 = read_cr0();
    cr0 &= ~0x80000000;  /* Clear PG bit (bit 31) */
    write_cr0(cr0);
    
    /* Flush TLB */
    write_cr3(read_cr3());
    
    paging_enabled = false;
    serial_puts("[WARN] Paging disabled - addresses are now physical\n");
}

bool paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    /* Align addresses to page boundaries */
    vaddr = ALIGN_DOWN_PAGE(vaddr);
    paddr = ALIGN_DOWN_PAGE(paddr);
    
    /* Extract directory and table indices from virtual address */
    uint32_t dir_idx = VADDR_DIR_IDX(vaddr);
    uint32_t tbl_idx = VADDR_TABLE_IDX(vaddr);
    
    /* Get or create the page table for this directory entry */
    page_dir_entry_t *pde = _get_or_create_pde(dir_idx);
    if (pde == NULL) {
        return false;  /* Allocation failed */
    }
    
    /* Get the page table */
    page_table_t *table = (page_table_t *)(PDE_GET_TABLE(*pde));
    if (table == NULL) {
        serial_printf("[ERROR] Invalid page table at dir_idx %u\n", dir_idx);
        return false;
    }
    
    /* Set the page table entry */
    page_tbl_entry_t *pte = &table->entries[tbl_idx];
    pte->value = (paddr & 0xFFFFF000) | (flags & 0x00000FFF);
    
    /* Flush TLB entry for this virtual address */
    invlpg(vaddr);
    
    return true;
}

void paging_unmap_page(uint32_t vaddr) {
    vaddr = ALIGN_DOWN_PAGE(vaddr);
    
    uint32_t dir_idx = VADDR_DIR_IDX(vaddr);
    uint32_t tbl_idx = VADDR_TABLE_IDX(vaddr);
    
    page_table_t *table = _get_page_table(dir_idx);
    if (table == NULL) {
        return;  /* No page table, nothing to unmap */
    }
    
    /* Clear the page table entry (not present) */
    table->entries[tbl_idx].value = 0;
    
    /* Flush TLB */
    invlpg(vaddr);
}

uint32_t paging_get_mapping(uint32_t vaddr) {
    uint32_t dir_idx = VADDR_DIR_IDX(vaddr);
    uint32_t tbl_idx = VADDR_TABLE_IDX(vaddr);
    uint32_t offset = VADDR_OFFSET(vaddr);
    
    page_table_t *table = _get_page_table(dir_idx);
    if (table == NULL) {
        return 0;  /* Not mapped */
    }
    
    page_tbl_entry_t *pte = &table->entries[tbl_idx];
    if (!(pte->value & PAGE_PRESENT)) {
        return 0;  /* Not present */
    }
    
    /* Return physical address with offset preserved */
    return (pte->value & 0xFFFFF000) | offset;
}

void paging_handle_page_fault(uint32_t error_code) {
    uint32_t cr2 = read_cr2();  /* Faulting linear address */
    
    serial_puts("\n");
    serial_puts("╔════════════════════════════════════════════════════╗\n");
    serial_puts("║                 PAGE FAULT (#PF)                   ║\n");
    serial_puts("╚════════════════════════════════════════════════════╝\n");
    
    serial_printf("Faulting Address: 0x%x\n", cr2);
    serial_printf("Error Code:       0x%x\n", error_code);
    
    /* Decode error code bits */
    bool present = (error_code & 0x1) != 0;
    bool write = (error_code & 0x2) != 0;
    bool user = (error_code & 0x4) != 0;
    bool rsvd = (error_code & 0x8) != 0;
    bool instr_fetch = (error_code & 0x10) != 0;
    
    serial_puts("\nFault Reason:\n");
    
    if (present) {
        serial_puts("  - Protection violation (page present but access denied)\n");
        if (write) {
            serial_puts("  - Attempted write to read-only page\n");
        } else {
            serial_puts("  - Attempted read from protected page\n");
        }
    } else {
        serial_puts("  - Page not present (not mapped or swapped out)\n");
    }
    
    if (user) {
        serial_puts("  - Triggered in user mode\n");
    } else {
        serial_puts("  - Triggered in kernel mode\n");
    }
    
    if (rsvd) {
        serial_puts("  - Reserved bit set in page table\n");
    }
    
    if (instr_fetch) {
        serial_puts("  - Caused by instruction fetch\n");
    }
    
    serial_printf("\nInstruction Pointer: 0x%x\n", read_eip());
    serial_puts("\n");
    
    /* Panic - we don't handle page faults yet */
    kernel_panic("Page fault - demand paging not implemented yet");
}

uint32_t paging_get_page_dir(void) {
    return (uintptr_t)&kernel_page_dir;
}

void paging_load_page_dir(uint32_t pdir_addr) {
    /* CR3 holds page directory base address
     * Must be 4KB aligned
     */
    if (pdir_addr & 0x00000FFF) {
        serial_printf("[ERROR] paging_load_page_dir: address 0x%x not aligned\n", pdir_addr);
        return;
    }
    
    write_cr3(pdir_addr);
}

void paging_dump_directory(void) {
    serial_puts("\n");
    serial_puts("═════════════════════════════════════════════════════\n");
    serial_puts("               PAGE DIRECTORY DUMP\n");
    serial_puts("═════════════════════════════════════════════════════\n");
    
    serial_printf("Page Directory @ 0x%x\n\n", paging_get_page_dir());
    
    uint32_t mapped_count = 0;
    uint32_t last_start = 0xFFFFFFFF;
    bool in_range = false;
    
    for (uint32_t i = 0; i < 1024; i++) {
        page_dir_entry_t *pde = &kernel_page_dir.entries[i];
        
        if (pde->value & PAGE_PRESENT) {
            mapped_count++;
            
            if (!in_range) {
                last_start = i * (1024 * PAGE_SIZE);  /* 4MB per entry */
                in_range = true;
            }
        } else {
            if (in_range) {
                uint32_t range_end = i * (1024 * PAGE_SIZE) - 1;
                serial_printf("  Entry %u-%u:  0x%08x - 0x%08x\n",
                            i - 1024 * PAGE_SIZE / (1024 * PAGE_SIZE),
                            i - 1,
                            last_start,
                            range_end);
                in_range = false;
            }
        }
    }
    
    serial_printf("\nTotal mapped entries: %u / 1024\n", mapped_count);
    serial_puts("═════════════════════════════════════════════════════\n\n");
}

void paging_dump_table(uint32_t table_idx) {
    if (table_idx >= 1024) {
        serial_printf("[ERROR] Table index %u out of range\n", table_idx);
        return;
    }
    
    page_table_t *table = _get_page_table(table_idx);
    if (table == NULL) {
        serial_printf("Table index %u: Not allocated\n", table_idx);
        return;
    }
    
    serial_printf("\n=== Page Table %u ===\n", table_idx);
    serial_printf("Base Virtual Address: 0x%x\n", table_idx * (1024 * PAGE_SIZE));
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < 1024; i++) {
        page_tbl_entry_t *pte = &table->entries[i];
        
        if (pte->value & PAGE_PRESENT) {
            if (count < 10) {  /* Show first 10 entries */
                serial_printf("  [%u] 0x%x → 0x%x (flags: 0x%x)\n",
                            i, table_idx * (1024 * PAGE_SIZE) + i * PAGE_SIZE,
                            PTE_GET_FRAME(*pte), pte->value & 0xFFF);
            }
            count++;
        }
    }
    serial_printf("Total mapped pages: %u / 1024\n\n", count);
}

bool paging_is_mapped(uint32_t vaddr) {
    uint32_t dir_idx = VADDR_DIR_IDX(vaddr);
    uint32_t tbl_idx = VADDR_TABLE_IDX(vaddr);
    
    page_table_t *table = _get_page_table(dir_idx);
    if (table == NULL) {
        return false;
    }
    
    return (table->entries[tbl_idx].value & PAGE_PRESENT) != 0;
}
