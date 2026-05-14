/*
 * mm/paging.h - x86 Paging Infrastructure
 *
 * Implements 32-bit x86 paging with:
 * - Page directories (top-level page tables)
 * - Page tables (4KB pages)
 * - Virtual address translation
 * - Identity mapping (current approach)
 * - Future higher-half kernel support
 *
 * x86 Paging Reference:
 * 
 * CR3: Page Directory Base Register
 *   Bits 0-11:  Ignored
 *   Bits 12-31: Physical address of page directory (aligned to 4KB)
 *   When paging enabled, CPU uses CR3 to find page directory
 *
 * CR0 Paging Enable:
 *   Bit 31: PG (paging enable) - set to enable address translation
 *   Bit 0:  PE (protected mode enable) - already set
 *   Must enable both for paging to work
 *
 * Page Directory Entry (PDE) and Page Table Entry (PTE):
 *   Bit 0:      P (Present) - must be 1 for valid page
 *   Bit 1:      R/W (Read/Write) - 1 = writable, 0 = read-only
 *   Bit 2:      U/S (User/Supervisor) - 1 = user, 0 = kernel
 *   Bit 3:      PWT (Write Through) - 1 = write through, 0 = write back
 *   Bit 4:      PCD (Cache Disable) - 1 = uncached, 0 = cached
 *   Bit 5:      A (Accessed) - set by CPU when accessed
 *   Bit 6:      D (Dirty) - set by CPU when written to (PTEs only)
 *   Bits 12-31: Base address (aligned to 4KB)
 *
 * Virtual Address Translation:
 *   31-22: Page directory index (1024 entries)
 *   21-12: Page table index (1024 entries)
 *   11-0:  Offset within 4KB page
 *
 * Identity Mapping (current):
 *   Virtual address 0x00000000 → Physical address 0x00000000
 *   This makes paging transparent - no address translation needed
 *   Later: Higher-half mapping will change this
 */

#ifndef MM_PAGING_H
#define MM_PAGING_H

#include <stdint.h>
#include "types.h"

/* =========================================================================
 * PAGE TABLE STRUCTURES
 * ========================================================================= */

/**
 * Page Directory Entry (PDE)
 *
 * Points to a page table. The page table contains 1024 PTEs,
 * each mapping a 4KB page.
 *
 * Physical layout:
 *   31-12: Base address of page table (must be 4KB aligned)
 *   11-9:  Reserved (must be 0)
 *   8:     Global (not flushed on TLB clear) - ignored in PDEs
 *   7:     Page size (0 = 4KB, 1 = 4MB) - only if PSE enabled
 *   6:     Dirty (set by CPU when page table written) - unused for PDEs
 *   5:     Accessed (set by CPU)
 *   4:     PCD (cache disable)
 *   3:     PWT (write through)
 *   2:     U/S (user/supervisor)
 *   1:     R/W (read/write)
 *   0:     P (present)
 */
typedef struct {
    uint32_t value;
} page_dir_entry_t;

/**
 * Page Table Entry (PTE)
 *
 * Maps a virtual page to a physical frame. Each PTE covers 4KB.
 *
 * Physical layout (same as PDE, but bit 6 = Dirty for PTEs):
 *   31-12: Base address of physical page frame (4KB aligned)
 *   11-9:  Reserved
 *   8:     G (global - not flushed on TLB clear)
 *   7:     PAT (page attribute table)
 *   6:     D (dirty - set by CPU on write)
 *   5:     A (accessed - set by CPU on read/write)
 *   4:     PCD (cache disable)
 *   3:     PWT (write through)
 *   2:     U/S (user/supervisor)
 *   1:     R/W (read/write)
 *   0:     P (present)
 */
typedef struct {
    uint32_t value;
} page_tbl_entry_t;

/**
 * Page Directory (kernel)
 *
 * Top-level page table containing 1024 PDEs.
 * Each PDE points to a page table.
 * Allocated at 4KB boundary.
 */
typedef struct {
    page_dir_entry_t entries[1024];
} page_directory_t;

/**
 * Page Table
 *
 * Second-level page table containing 1024 PTEs.
 * Each PTE maps one 4KB page.
 * Allocated at 4KB boundary.
 */
typedef struct {
    page_tbl_entry_t entries[1024];
} page_table_t;

/* =========================================================================
 * HELPER MACROS FOR PAGE TABLE MANIPULATION
 * ========================================================================= */

/* Extract table/frame address from entry (mask to 4KB alignment) */
#define PTE_GET_FRAME(entry)   ((entry).value & 0xFFFFF000)
#define PDE_GET_TABLE(entry)   ((entry).value & 0xFFFFF000)

/* Extract offset within page from virtual address */
#define VADDR_OFFSET(vaddr)    ((vaddr) & 0x00000FFF)

/* Extract page table index from virtual address (bits 12-21) */
#define VADDR_TABLE_IDX(vaddr) (((vaddr) >> 12) & 0x3FF)

/* Extract page directory index from virtual address (bits 22-31) */
#define VADDR_DIR_IDX(vaddr)   (((vaddr) >> 22) & 0x3FF)

/* =========================================================================
 * PAGING INITIALIZATION & MANAGEMENT
 * ========================================================================= */

/**
 * paging_init() - Initialize paging infrastructure
 *
 * Actions:
 * 1. Allocate kernel page directory
 * 2. Allocate identity-mapping page tables for low 1MB
 * 3. Set up page directory entries
 * 4. Create identity-mapped kernel region
 * 5. Populate page tables with initial mappings
 *
 * After this, paging is ready but NOT YET ENABLED.
 * Call paging_enable() to activate virtual memory.
 *
 * Invariants:
 * - Kernel is identity-mapped (virt == phys)
 * - Reserved regions are marked present but read-only
 * - Free memory is not mapped (page faults will detect access)
 */
void paging_init(void);

/**
 * paging_enable() - Enable paging in CPU
 *
 * Actions:
 * 1. Load page directory address into CR3
 * 2. Set CR0.PG bit to enable paging
 * 3. Flush TLB (optional, but recommended)
 *
 * After this call:
 * - Virtual address translation is active
 * - Unmapped page access will cause #PF (page fault)
 * - Must ensure current code is properly mapped!
 *
 * Current: Kernel is identity-mapped, so no address change.
 * Future: After higher-half mapping, CR3 load will relocate kernel.
 *
 * Thread-safe: NO - must be called with interrupts disabled
 */
void paging_enable(void);

/**
 * paging_disable() - Disable paging in CPU
 *
 * DANGER: This is rarely needed and risky.
 * Only for memory diagnostics or system shutdown.
 *
 * Actions:
 * 1. Clear CR0.PG bit
 * 2. Flush TLB
 *
 * After disable, all addresses become physical addresses.
 * Current code must be at same physical location!
 */
void paging_disable(void);

/* =========================================================================
 * PAGE MAPPING OPERATIONS
 * ========================================================================= */

/**
 * paging_map_page() - Map a single 4KB virtual page to physical frame
 *
 * Args:
 *   vaddr  - Virtual address (will be aligned to 4KB boundary)
 *   paddr  - Physical address to map to (will be aligned to 4KB boundary)
 *   flags  - PAGE_* flags (PAGE_PRESENT, PAGE_WRITE, PAGE_USER, etc.)
 *
 * Behavior:
 * - Allocates page tables as needed
 * - Sets PTE flags according to flags parameter
 * - Overwrites any existing mapping (no COW)
 * - TLB entry invalidated automatically (INVLPG)
 *
 * Returns: true if successful, false if allocation failed (OOM)
 *
 * Thread-safe: NO - must be called with interrupts disabled
 */
bool paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

/**
 * paging_unmap_page() - Unmap a single virtual page
 *
 * Args:
 *   vaddr - Virtual address of page to unmap
 *
 * Behavior:
 * - Clears PTE for the page (sets present bit to 0)
 * - Does NOT free the underlying page table
 * - TLB entry invalidated automatically
 * - Accessing unmapped page will cause page fault
 *
 * Thread-safe: NO
 */
void paging_unmap_page(uint32_t vaddr);

/**
 * paging_get_mapping() - Get physical address for virtual page
 *
 * Args:
 *   vaddr - Virtual address (any offset within page)
 *
 * Returns: Physical address corresponding to vaddr, or 0 if unmapped
 *          (Note: 0x0000 is a valid physical address, need better error handling)
 *
 * Thread-safe: YES (read-only, no CPU state changes)
 */
uint32_t paging_get_mapping(uint32_t vaddr);

/* =========================================================================
 * PAGE FAULT HANDLING
 * ========================================================================= */

/**
 * paging_handle_page_fault() - Handle #PF (exception 14)
 *
 * Called from exception handler when a page fault occurs.
 * Reads CR2 (faulting address) and determines what went wrong.
 *
 * Actions:
 * 1. Read CR2 (faulting linear address)
 * 2. Read error code from exception frame
 * 3. Determine fault type (not present, protection, reserved)
 * 4. Log diagnostic information
 * 5. Panic (for now - no page allocation yet)
 *
 * Error code bits:
 *   Bit 0: P (present) - 0 = page not present, 1 = protection violation
 *   Bit 1: W/R (write) - 0 = read, 1 = write
 *   Bit 2: U/S (user) - 0 = kernel, 1 = user
 *   Bit 3: RSVD - 1 = reserved bit set in page table
 *   Bit 4: I/D - 1 = instruction fetch
 *
 * Should be called from:
 *   exception_handler(14, error_code)
 *
 * Currently panics on ANY page fault (no demand paging yet).
 */
void paging_handle_page_fault(uint32_t error_code);

/* =========================================================================
 * PAGE DIRECTORY MANAGEMENT
 * ========================================================================= */

/**
 * paging_get_page_dir() - Get kernel page directory address
 *
 * Returns: Physical address of kernel page directory
 *
 * Used for:
 * - Debugging
 * - Pre-loading CR3 before enable
 * - Process creation (copy page directory)
 */
uint32_t paging_get_page_dir(void);

/**
 * paging_load_page_dir() - Load a page directory into CR3
 *
 * Args:
 *   pdir_addr - Physical address of page directory (must be 4KB aligned)
 *
 * Behavior:
 * - Writes to CR3 register
 * - Invalidates TLB (flushes all address translations)
 * - Used when switching address spaces (processes)
 *
 * Thread-safe: NO - affects entire CPU address translation
 */
void paging_load_page_dir(uint32_t pdir_addr);

/* =========================================================================
 * DIAGNOSTICS & DEBUGGING
 * ========================================================================= */

/**
 * paging_dump_directory() - Print page directory for debugging
 *
 * Output (via serial):
 * - Entry 0-1023 status (present/mapped)
 * - Mapped range addresses
 * - Flags for each entry
 */
void paging_dump_directory(void);

/**
 * paging_dump_table() - Print a single page table for debugging
 *
 * Args:
 *   table_idx - Page directory index (0-1023)
 *
 * Output:
 * - All 1024 entries in the page table
 * - Physical addresses and flags
 */
void paging_dump_table(uint32_t table_idx);

/**
 * paging_is_mapped() - Check if virtual page is mapped
 *
 * Args:
 *   vaddr - Virtual address
 *
 * Returns: true if page is present and mapped, false otherwise
 */
bool paging_is_mapped(uint32_t vaddr);

#endif /* MM_PAGING_H */
