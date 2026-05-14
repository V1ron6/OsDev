/*
 * mm/heap.c - Kernel Heap Allocator Implementation
 *
 * Simple bump allocator for initial kernel memory management.
 * No fragmentation, deterministic behavior, but no freeing.
 */

#include "mm/heap.h"
#include "mm/constants.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "serial.h"
#include "vga.h"
#include <string.h>

/* =========================================================================
 * INTERNAL STATE
 * ========================================================================= */

/* Kernel heap base address (starts after kernel code/data/stack) */
#define KERNEL_HEAP_BASE    0x00100000
#define KERNEL_HEAP_SIZE    0x00400000  /* 4 MB initial heap */
#define KERNEL_HEAP_END     (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)

/* Heap pointer (advanced by kmalloc) */
static void *heap_ptr = (void *)KERNEL_HEAP_BASE;

/* Track initialized state */
static bool heap_initialized = false;

/* =========================================================================
 * PUBLIC API
 * ========================================================================= */

void heap_init(void) {
    if (heap_initialized) {
        return;
    }
    
    serial_puts("[HEAP] Initializing kernel heap allocator\n");
    vga_puts("[HEAP] Initializing...\n");
    
    serial_printf("[HEAP] Heap base: 0x%x\n", KERNEL_HEAP_BASE);
    serial_printf("[HEAP] Heap size: %.1f MB\n", KERNEL_HEAP_SIZE / (1024.0 * 1024.0));
    serial_printf("[HEAP] Heap end:  0x%x\n", KERNEL_HEAP_END);
    
    /* Note: We don't pre-allocate pages - they'll fault in when accessed.
     * This is OK for now because we don't have demand paging.
     * Once paging is fully enabled, we need to pre-map heap region.
     */
    
    /* Pre-map heap region for safety */
    for (uint32_t addr = KERNEL_HEAP_BASE; addr < KERNEL_HEAP_BASE + (1 * 1024 * 1024); 
         addr += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            serial_puts("[ERROR] heap_init: Out of memory\n");
            return;
        }
        paging_map_page(addr, frame, PAGE_KERNEL);
    }
    
    vga_puts("[HEAP] Mapped 1MB\n");
    
    heap_ptr = (void *)KERNEL_HEAP_BASE;
    heap_initialized = true;
    
    serial_puts("[HEAP] Ready for allocation\n");
}

void *kmalloc(size_t size) {
    if (!heap_initialized) {
        serial_puts("[ERROR] kmalloc() called before heap_init()\n");
        return NULL;
    }
    
    if (size == 0) {
        return NULL;  /* Allocate zero bytes is undefined */
    }
    
    /* Align to 4-byte boundary for safety */
    size = (size + 3) & ~3;
    
    /* Check for overflow */
    if ((uintptr_t)heap_ptr + size > KERNEL_HEAP_END) {
        serial_printf("[ERROR] kmalloc(%u) - heap overflow\n", size);
        return NULL;  /* Out of memory */
    }
    
    void *allocated = heap_ptr;
    heap_ptr = (void *)((uintptr_t)heap_ptr + size);
    
    /* Debug logging (can be verbose) */
    /* serial_printf("[HEAP] Allocated %u bytes at 0x%x\n", size, (uint32_t)allocated); */
    
    return allocated;
}

void *kcalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void kfree(void *ptr) {
    /* Bump allocator doesn't support freeing
     * For now, just warn and ignore
     */
    if (ptr != NULL) {
        serial_printf("[WARN] kfree(0x%x) - not implemented yet\n", (uint32_t)ptr);
    }
}

void heap_get_stats(uint32_t *used, uint32_t *free) {
    if (heap_initialized) {
        *used = (uintptr_t)heap_ptr - KERNEL_HEAP_BASE;
        *free = KERNEL_HEAP_END - (uintptr_t)heap_ptr;
    } else {
        *used = 0;
        *free = 0;
    }
}
