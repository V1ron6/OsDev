/*
 * mm/vmm.h - Virtual Memory Management
 *
 * Higher-level abstraction over paging subsystem.
 * Handles virtual address space management.
 *
 * Currently: Thin wrapper around paging functions
 * Future: Page table hierarchies, regions, permissions
 */

#ifndef MM_VMM_H
#define MM_VMM_H

#include <stdint.h>
#include "types.h"

/**
 * vmm_init() - Initialize virtual memory manager
 * Currently calls paging_init() internally
 */
void vmm_init(void);

/**
 * vmm_map_region() - Map a region of virtual to physical memory
 *
 * Args:
 *   vbase - Virtual address base (will be aligned to 4KB)
 *   pbase - Physical address base (will be aligned to 4KB)
 *   size  - Size in bytes (will be rounded up to next 4KB)
 *   flags - Paging flags (PAGE_PRESENT, PAGE_WRITE, etc.)
 *
 * Returns: true on success, false on OOM
 */
bool vmm_map_region(uint32_t vbase, uint32_t pbase, uint32_t size, uint32_t flags);

/**
 * vmm_unmap_region() - Unmap a virtual memory region
 */
void vmm_unmap_region(uint32_t vbase, uint32_t size);

#endif /* MM_VMM_H */
