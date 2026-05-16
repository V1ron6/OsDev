/*
 * arch/x86/gdt.h - Global Descriptor Table (GDT) Management
 *
 * The GDT defines memory segments for x86 protected mode:
 * - Code segments (ring 0 kernel, ring 3 user)
 * - Data segments (ring 0 kernel, ring 3 user)
 * - Task State Segment (TSS) for privilege transitions
 *
 * Selector format:
 *   Bits 15-3: GDT index
 *   Bit 2: TI (table indicator) - 0=GDT, 1=LDT
 *   Bits 1-0: RPL (requested privilege level)
 */

#ifndef _BYTEBANDIT_GDT_H
#define _BYTEBANDIT_GDT_H

#include "types.h"
#include "x86.h"

/* GDT Index Numbers (used to calculate selectors) */
#define GDT_INDEX_NULL       0  /* NULL entry (required) */
#define GDT_INDEX_KERN_CODE  1  /* Kernel code segment */
#define GDT_INDEX_KERN_DATA  2  /* Kernel data segment */
#define GDT_INDEX_USER_CODE  3  /* User code segment */
#define GDT_INDEX_USER_DATA  4  /* User data segment */
#define GDT_INDEX_TSS        5  /* Task State Segment */

/* Selector values (index * 8, with TI and RPL bits) */
#define KERN_CODE_SEL   ((GDT_INDEX_KERN_CODE << 3) | 0)  /* Ring 0, GDT */
#define KERN_DATA_SEL   ((GDT_INDEX_KERN_DATA << 3) | 0)  /* Ring 0, GDT */
#define USER_CODE_SEL   ((GDT_INDEX_USER_CODE << 3) | 3)  /* Ring 3, GDT */
#define USER_DATA_SEL   ((GDT_INDEX_USER_DATA << 3) | 3)  /* Ring 3, GDT */
#define TSS_SEL         ((GDT_INDEX_TSS << 3) | 0)        /* Ring 0, GDT */

/* Maximum number of entries in our GDT */
#define GDT_SIZE         6

/**
 * Initialize the GDT with kernel and user segments.
 *
 * This function:
 * - Creates NULL entry (required)
 * - Creates kernel code/data segments (ring 0)
 * - Creates user code/data segments (ring 3)
 * - Creates TSS descriptor (but doesn't initialize TSS yet)
 * - Loads GDT via LGDT instruction
 *
 * Must be called before any privilege transitions or task switching.
 */
void gdt_init(void);

/**
 * Set the TSS descriptor in the GDT.
 *
 * @param tss_addr  Physical address of TSS structure
 *
 * This is called by tss_init() after the TSS is allocated,
 * to complete the GDT setup for TSS descriptor.
 */
void gdt_set_tss_descriptor(uint32_t tss_addr);

/**
 * Get the current GDT size in bytes.
 *
 * @return Number of bytes occupied by GDT entries.
 */
uint32_t gdt_get_size(void);

/**
 * Get the base address of the GDT.
 *
 * @return Physical address of GDT first entry.
 */
uint32_t gdt_get_base(void);

#endif /* _BYTEBANDIT_GDT_H */
