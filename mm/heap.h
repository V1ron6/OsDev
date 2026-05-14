/*
 * mm/heap.h - Kernel Heap Allocator
 *
 * Simple bump allocator for early kernel memory allocation.
 * 
 * Design:
 * - Grows upward from KERNEL_HEAP_BASE (0x100000)
 * - No freeing (yet) - focus on stability
 * - Thread-safe: NO (must call with interrupts disabled)
 * - Single-threaded kernel initially
 *
 * Future improvements:
 * - Free list allocator
 * - Slab allocator
 * - Per-CPU allocation
 */

#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <stdint.h>
#include <stddef.h>

/**
 * heap_init() - Initialize kernel heap allocator
 *
 * Called after paging is enabled.
 * Sets up initial heap at KERNEL_HEAP_BASE.
 */
void heap_init(void);

/**
 * kmalloc() - Allocate kernel memory
 *
 * Args:
 *   size - Bytes to allocate
 *
 * Returns: Virtual address of allocated memory (4-byte aligned)
 *          NULL if allocation failed (OOM or size > remaining heap)
 *
 * Behavior:
 * - Simple bump allocator: returns next free address, advances pointer
 * - Alignment: All allocations aligned to 4 bytes
 * - No fragmentation (bump allocators don't fragment)
 * - No free (yet) - memory is never reused
 */
void *kmalloc(size_t size);

/**
 * kcalloc() - Allocate and zero kernel memory
 *
 * Like kmalloc but zeroes the memory first.
 */
void *kcalloc(size_t size);

/**
 * kfree() - Free kernel memory
 *
 * Currently not implemented (bump allocator doesn't support freeing).
 * Will be no-op or panic until proper allocator is implemented.
 */
void kfree(void *ptr);

/**
 * heap_get_stats() - Get heap statistics
 *
 * Fills in used/free bytes for diagnostics.
 */
void heap_get_stats(uint32_t *used, uint32_t *free);

#endif /* MM_HEAP_H */
