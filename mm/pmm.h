/*
 * mm/pmm.h - Physical Memory Manager
 *
 * Provides bitmap-based physical frame allocation for the kernel.
 * 
 * Design:
 * - One bit per 4KB frame
 * - Bitmap stored at PMM_BITMAP_BASE (0x93000)
 * - Tracks all frames from 0 to MEM_MAX_FRAMES
 * - Reserved regions marked as "used" (never allocated)
 *
 * Safety invariants:
 * 1. Never allocate frames in reserved regions
 * 2. Never allocate kernel memory
 * 3. Prevent double-free by checking bit before freeing
 * 4. Bounds check all frame numbers against MEM_MAX_FRAMES
 *
 * Usage:
 * 1. Call pmm_init() at boot time (initializes bitmap + reserves regions)
 * 2. Call pmm_alloc_frame() to allocate a frame
 * 3. Call pmm_free_frame() to deallocate
 * 4. Call pmm_get_status() for diagnostics
 */

#ifndef MM_PMM_H
#define MM_PMM_H

#include <stdint.h>
#include "types.h"

/* Typedef for frame handle (physical address of 4KiB page) */
typedef uint32_t phys_frame_t;

/* =========================================================================
 * PMM LIFECYCLE
 * ========================================================================= */

/**
 * pmm_init() - Initialize physical memory manager
 *
 * Called during kernel bootstrap (after VGA/serial init).
 * 
 * Actions:
 * 1. Clear bitmap to 0 (all frames free initially)
 * 2. Mark all reserved regions as used
 * 3. Mark all kernel frames as used
 * 4. Print diagnostic info
 *
 * MUST be called before any pmm_alloc_frame() calls.
 * Safe to call once - repeated calls are no-op.
 */
void pmm_init(void);

/* =========================================================================
 * FRAME ALLOCATION
 * ========================================================================= */

/**
 * pmm_alloc_frame() - Allocate a single 4KiB physical frame
 *
 * Returns: Physical address of allocated frame (multiple of 0x1000)
 *          0 if no frames available (out of memory)
 *
 * Behavior:
 * - Scans bitmap for first free bit
 * - Marks bit as used
 * - Returns physical address (frame_number * PAGE_SIZE)
 * - O(n) worst case, typically O(1) amortized
 *
 * Side effects: Modifies bitmap, updates free frame count
 *
 * Thread-safe: NO - must be called with interrupts disabled
 */
uint32_t pmm_alloc_frame(void);

/**
 * pmm_alloc_frames() - Allocate multiple contiguous frames
 *
 * Args:
 *   count - Number of consecutive frames to allocate
 *
 * Returns: Physical address of first frame in block (multiple of 0x1000)
 *          0 if not enough contiguous frames available
 *
 * Note: This is NOT implemented yet - reserved for future use.
 * Currently only pmm_alloc_frame() is available.
 */
uint32_t pmm_alloc_frames(uint32_t count);

/* =========================================================================
 * FRAME DEALLOCATION
 * ========================================================================= */

/**
 * pmm_free_frame() - Deallocate a single 4KiB physical frame
 *
 * Args:
 *   frame - Physical address of frame to free (should be multiple of 0x1000)
 *
 * Behavior:
 * - Converts address to frame number
 * - Validates frame is not in reserved regions
 * - Marks bit as free
 * - Updates free frame count
 *
 * Safety checks:
 * - Ignores frees of invalid addresses (e.g., < PAGE_SIZE)
 * - Prevents double-free (won't panic, just returns)
 * - Won't free kernel or reserved regions
 *
 * Thread-safe: NO - must be called with interrupts disabled
 */
void pmm_free_frame(uint32_t frame);

/* =========================================================================
 * STATUS & DIAGNOSTICS
 * ========================================================================= */

/**
 * pmm_get_free_frames() - Get count of unallocated frames
 *
 * Returns: Number of free frames available for allocation
 */
uint32_t pmm_get_free_frames(void);

/**
 * pmm_get_used_frames() - Get count of allocated frames
 *
 * Returns: Number of frames in use (allocated + reserved)
 */
uint32_t pmm_get_used_frames(void);

/**
 * pmm_get_total_frames() - Get total frame count
 *
 * Returns: MEM_MAX_FRAMES (theoretical maximum)
 */
uint32_t pmm_get_total_frames(void);

/**
 * pmm_is_frame_free() - Check if a frame is allocated
 *
 * Args:
 *   frame - Physical address to check
 *
 * Returns: true if frame is free (available for allocation)
 *          false if frame is in use or invalid
 */
bool pmm_is_frame_free(uint32_t frame);

/**
 * pmm_dump_map() - Print memory map for debugging
 *
 * Output (via serial + VGA):
 * - Total frames / free frames / reserved frames
 * - Reserved region list
 * - First 10 allocated frames
 *
 * Useful for:
 * - Verifying reserved regions are protected
 * - Checking for memory leaks
 * - Debugging allocation patterns
 */
void pmm_dump_map(void);

#endif /* MM_PMM_H */
