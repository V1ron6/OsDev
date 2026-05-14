/*
 * mm/vmm.c - Virtual Memory Management Implementation
 *
 * Thin wrapper over paging for higher-level operations.
 */

#include "mm/vmm.h"
#include "mm/paging.h"
#include "mm/constants.h"

void vmm_init(void) {
    /* For now, VMM is just a wrapper around paging */
    paging_init();
}

bool vmm_map_region(uint32_t vbase, uint32_t pbase, uint32_t size, uint32_t flags) {
    /* Align to page boundaries */
    vbase = ALIGN_DOWN_PAGE(vbase);
    pbase = ALIGN_DOWN_PAGE(pbase);
    size = ALIGN_UP_PAGE(size);
    
    /* Map each page */
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        if (!paging_map_page(vbase + offset, pbase + offset, flags)) {
            return false;  /* Allocation failed */
        }
    }
    
    return true;
}

void vmm_unmap_region(uint32_t vbase, uint32_t size) {
    vbase = ALIGN_DOWN_PAGE(vbase);
    size = ALIGN_UP_PAGE(size);
    
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        paging_unmap_page(vbase + offset);
    }
}
