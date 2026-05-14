/*
 * mm/pmm.c - Physical Memory Manager Implementation
 *
 * Bitmap-based frame allocator. One bit per 4KB frame.
 * 
 * Implementation notes:
 * - Bitmap stored at PMM_BITMAP_BASE (0x93000)
 * - Each byte manages 8 frames (8 bits)
 * - Bit set = frame in use, bit clear = frame free
 * - Scanning is left-to-right, low-to-high
 *
 * Performance:
 * - Allocation: O(1) amortized (typically find free bit quickly)
 * - Deallocation: O(1) (direct bit flip)
 * - Memory: 1 byte per 8 frames (4KB per 32KB memory)
 *
 * Limitations:
 * - No defragmentation
 * - No contiguous allocation (yet)
 * - No per-process frame tracking
 *
 * Future improvements:
 * - Contiguous frame allocation for DMA
 * - Frame reference counting
 * - Per-zone allocation (ISA, DMA, etc.)
 */

#include "mm/pmm.h"
#include "mm/constants.h"
#include "serial.h"
#include "vga.h"
#include "panic.h"
#include "x86.h"
#include <stddef.h>

/* =========================================================================
 * INTERNAL STATE
 * ========================================================================= */

/* Bitmap: one bit per frame
 * Stored in reserved region after kernel stack
 * PMM_BITMAP_BASE = 0x93000, size = PMM_BITMAP_SIZE bytes
 */
static uint8_t *pmm_bitmap = (uint8_t *)PMM_BITMAP_BASE;

/* Tracking counters (cached for O(1) query)
 * Updated during alloc/free operations
 */
static uint32_t pmm_used_frames = 0;
static uint32_t pmm_free_frames = 0;

/* Track whether PMM has been initialized */
static bool pmm_initialized = false;

/* =========================================================================
 * BITMAP OPERATIONS
 * ========================================================================= */

/**
 * _set_bit() - Mark frame as allocated
 *
 * Internal helper: Sets bit N in bitmap
 * Bit layout: bitmap[byte] bit [bit_offset] corresponds to frame N
 */
static inline void _set_bit(uint32_t frame_num) {
    uint32_t byte_idx = frame_num / 8;
    uint8_t bit_idx = frame_num % 8;
    pmm_bitmap[byte_idx] |= (1 << bit_idx);
}

/**
 * _clear_bit() - Mark frame as free
 *
 * Internal helper: Clears bit N in bitmap
 */
static inline void _clear_bit(uint32_t frame_num) {
    uint32_t byte_idx = frame_num / 8;
    uint8_t bit_idx = frame_num % 8;
    pmm_bitmap[byte_idx] &= ~(1 << bit_idx);
}

/**
 * _test_bit() - Check if frame is allocated
 *
 * Returns: 1 if frame is in use, 0 if free
 */
static inline int _test_bit(uint32_t frame_num) {
    uint32_t byte_idx = frame_num / 8;
    uint8_t bit_idx = frame_num % 8;
    return (pmm_bitmap[byte_idx] >> bit_idx) & 1;
}

/**
 * _find_first_free_frame() - Scan bitmap for first free bit
 *
 * Scans from start_frame forward, returns frame number of first free frame.
 * Returns MEM_MAX_FRAMES if no free frame found (OOM).
 *
 * This is the allocation slow path - scans linearly.
 * Typical systems have many free frames, so this is fast in practice.
 */
static uint32_t _find_first_free_frame(uint32_t start_frame) {
    for (uint32_t frame = start_frame; frame < MEM_MAX_FRAMES; frame++) {
        if (!_test_bit(frame)) {
            return frame;
        }
    }
    return MEM_MAX_FRAMES;  /* OOM - no free frames */
}

/* =========================================================================
 * RESERVED REGION CHECKING
 * ========================================================================= */

/**
 * _is_reserved_frame() - Check if frame is in a protected region
 *
 * Returns: true if frame must never be allocated
 *
 * Protected regions:
 * - IVT (0x0000-0x1000)
 * - Bootloader (0x7C000-0x7E000)
 * - VRAM (0xA0000-0xC0000)
 * - BIOS ROM (0xC0000-0x100000)
 * - Kernel text/data/bss
 * - Kernel stack
 * - Page table area
 */
static bool _is_reserved_frame(uint32_t frame_num) {
    uint32_t addr = FRAME_TO_ADDR(frame_num);
    
    /* IVT */
    if (addr >= MEM_RESERVED_IVT_BASE && addr < MEM_RESERVED_IVT_END)
        return true;
    
    /* Bootloader */
    if (addr >= MEM_RESERVED_BOOTLOADER_BASE && addr < MEM_RESERVED_BOOTLOADER_END)
        return true;
    
    /* EBDA */
    if (addr >= MEM_RESERVED_EBDA_BASE && addr < MEM_RESERVED_EBDA_END)
        return true;
    
    /* Video RAM */
    if (addr >= MEM_RESERVED_VRAM_BASE && addr < MEM_RESERVED_VRAM_END)
        return true;
    
    /* BIOS ROM */
    if (addr >= MEM_RESERVED_BIOS_ROM_BASE && addr < MEM_RESERVED_BIOS_ROM_END)
        return true;
    
    /* Kernel (text/data/bss) */
    if (addr >= KERNEL_PHYS_BASE && addr <= (uintptr_t)&__bss_end)
        return true;
    
    /* Kernel stack */
    if (addr >= KERNEL_STACK_BASE && addr < KERNEL_STACK_TOP)
        return true;
    
    /* Page table reservation area */
    if (addr >= MEM_RESERVED_PAGETABLES_BASE && addr < MEM_RESERVED_PAGETABLES_END)
        return true;
    
    return false;
}

/* =========================================================================
 * PUBLIC PMM API
 * ========================================================================= */

void pmm_init(void) {
    if (pmm_initialized)
        return;  /* Already initialized */
    
    serial_puts("[PMM] Initializing physical memory manager\n");
    vga_puts("[PMM] Initializing...\n");
    
    /* Clear bitmap (all frames marked as free) */
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0;
    }
    pmm_used_frames = 0;
    pmm_free_frames = MEM_MAX_FRAMES;
    
    /* Mark all reserved regions as used
     * This prevents them from being allocated
     */
    for (uint32_t frame = 0; frame < MEM_MAX_FRAMES; frame++) {
        if (_is_reserved_frame(frame)) {
            _set_bit(frame);
            pmm_used_frames++;
            pmm_free_frames--;
        }
    }
    
    serial_printf("[PMM] Bitmap at 0x%x, size %u bytes\n", 
                  PMM_BITMAP_BASE, PMM_BITMAP_SIZE);
    serial_printf("[PMM] Total frames: %u (%.1f MB)\n", 
                  MEM_MAX_FRAMES, (MEM_MAX_FRAMES * PAGE_SIZE) / (1024.0 * 1024.0));
    serial_printf("[PMM] Reserved frames: %u\n", pmm_used_frames);
    serial_printf("[PMM] Free frames: %u\n", pmm_free_frames);
    
    vga_printf("[PMM] %u frames reserved, %u frames free\n", 
               pmm_used_frames, pmm_free_frames);
    
    pmm_initialized = true;
}

uint32_t pmm_alloc_frame(void) {
    if (!pmm_initialized) {
        serial_puts("[ERROR] pmm_alloc_frame() called before pmm_init()\n");
        kernel_panic("PMM not initialized");
    }
    
    if (pmm_free_frames == 0) {
        serial_puts("[ERROR] Out of memory - no free frames\n");
        return 0;  /* Out of memory */
    }
    
    /* Find first free frame starting from frame 0 */
    uint32_t frame = _find_first_free_frame(0);
    
    if (frame >= MEM_MAX_FRAMES) {
        serial_puts("[ERROR] No free frames found (OOM)\n");
        return 0;
    }
    
    /* Mark frame as allocated */
    _set_bit(frame);
    pmm_used_frames++;
    pmm_free_frames--;
    
    uint32_t addr = FRAME_TO_ADDR(frame);
    
    /* Debug output (verbose, may spam logs) */
    /* serial_printf("[PMM] Allocated frame %u (0x%x)\n", frame, addr); */
    
    return addr;
}

uint32_t pmm_alloc_frames(uint32_t count) {
    /* TODO: Implement contiguous frame allocation
     * Required for: DMA operations, page table allocation, etc.
     */
    (void)count;  /* Suppress unused warning */
    return 0;     /* Not implemented yet */
}

void pmm_free_frame(uint32_t frame_addr) {
    if (!pmm_initialized) {
        serial_puts("[WARN] pmm_free_frame() called before pmm_init()\n");
        return;
    }
    
    /* Reject invalid addresses (too small) */
    if (frame_addr < PAGE_SIZE) {
        return;  /* Silently ignore - likely null pointer */
    }
    
    /* Convert address to frame number */
    uint32_t frame = ADDR_TO_FRAME(frame_addr);
    
    /* Bounds check */
    if (frame >= MEM_MAX_FRAMES) {
        serial_printf("[WARN] pmm_free_frame(0x%x) - out of bounds\n", frame_addr);
        return;
    }
    
    /* Prevent freeing reserved regions */
    if (_is_reserved_frame(frame)) {
        serial_printf("[WARN] pmm_free_frame(0x%x) - reserved region\n", frame_addr);
        return;
    }
    
    /* Prevent double-free */
    if (!_test_bit(frame)) {
        serial_printf("[WARN] pmm_free_frame(0x%x) - already free\n", frame_addr);
        return;
    }
    
    /* Mark frame as free */
    _clear_bit(frame);
    pmm_used_frames--;
    pmm_free_frames++;
    
    /* Debug output (verbose) */
    /* serial_printf("[PMM] Freed frame %u (0x%x)\n", frame, frame_addr); */
}

uint32_t pmm_get_free_frames(void) {
    return pmm_free_frames;
}

uint32_t pmm_get_used_frames(void) {
    return pmm_used_frames;
}

uint32_t pmm_get_total_frames(void) {
    return MEM_MAX_FRAMES;
}

bool pmm_is_frame_free(uint32_t frame_addr) {
    uint32_t frame = ADDR_TO_FRAME(frame_addr);
    
    if (frame >= MEM_MAX_FRAMES)
        return false;  /* Invalid */
    
    return !_test_bit(frame);
}

void pmm_dump_map(void) {
    serial_puts("\n");
    serial_puts("═══════════════════════════════════════════════════════\n");
    serial_puts("                   PMM MEMORY MAP\n");
    serial_puts("═══════════════════════════════════════════════════════\n");
    
    serial_printf("Total Physical Memory: %u frames (%.1f MB)\n",
                  MEM_MAX_FRAMES, (MEM_MAX_FRAMES * PAGE_SIZE) / (1024.0 * 1024.0));
    serial_printf("Free Frames:           %u (%.1f MB)\n",
                  pmm_free_frames, (pmm_free_frames * PAGE_SIZE) / (1024.0 * 1024.0));
    serial_printf("Used Frames:           %u (%.1f MB)\n",
                  pmm_used_frames, (pmm_used_frames * PAGE_SIZE) / (1024.0 * 1024.0));
    serial_printf("Utilization:           %.1f%%\n",
                  (100.0 * pmm_used_frames) / MEM_MAX_FRAMES);
    
    serial_puts("\nReserved Regions:\n");
    serial_printf("  IVT:              0x%08x - 0x%08x (%u frames)\n",
                  MEM_RESERVED_IVT_BASE, MEM_RESERVED_IVT_END,
                  (MEM_RESERVED_IVT_END - MEM_RESERVED_IVT_BASE) / PAGE_SIZE);
    serial_printf("  BOOTLOADER:       0x%08x - 0x%08x (%u frames)\n",
                  MEM_RESERVED_BOOTLOADER_BASE, MEM_RESERVED_BOOTLOADER_END,
                  (MEM_RESERVED_BOOTLOADER_END - MEM_RESERVED_BOOTLOADER_BASE) / PAGE_SIZE);
    serial_printf("  EBDA:             0x%08x - 0x%08x (%u frames)\n",
                  MEM_RESERVED_EBDA_BASE, MEM_RESERVED_EBDA_END,
                  (MEM_RESERVED_EBDA_END - MEM_RESERVED_EBDA_BASE) / PAGE_SIZE);
    serial_printf("  VIDEO RAM:        0x%08x - 0x%08x (%u frames)\n",
                  MEM_RESERVED_VRAM_BASE, MEM_RESERVED_VRAM_END,
                  (MEM_RESERVED_VRAM_END - MEM_RESERVED_VRAM_BASE) / PAGE_SIZE);
    serial_printf("  BIOS ROM:         0x%08x - 0x%08x (%u frames)\n",
                  MEM_RESERVED_BIOS_ROM_BASE, MEM_RESERVED_BIOS_ROM_END,
                  (MEM_RESERVED_BIOS_ROM_END - MEM_RESERVED_BIOS_ROM_BASE) / PAGE_SIZE);
    serial_printf("  KERNEL:           0x%08x - 0x%08x\n",
                  KERNEL_PHYS_BASE, (uintptr_t)&__bss_end);
    serial_printf("  STACK:            0x%08x - 0x%08x (%u frames)\n",
                  KERNEL_STACK_BASE, KERNEL_STACK_TOP, KERNEL_STACK_SIZE / PAGE_SIZE);
    
    serial_puts("\n═══════════════════════════════════════════════════════\n\n");
}
