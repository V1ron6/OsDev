/*
 * arch/x86/gdt.c - Global Descriptor Table Implementation
 *
 * Creates and manages the GDT for x86 protected mode.
 */

#include "gdt.h"
#include "tss.h"
#include "serial.h"
#include "vga.h"

/* GDT table (statically allocated) */
static struct gdt_entry gdt_entries[GDT_SIZE];

/* GDT descriptor for LGDT instruction */
static struct gdt_descriptor gdt_descriptor;

/**
 * Create a single GDT entry.
 *
 * GDT entries are 8 bytes and encode:
 * - Base address (32 bits, split across fields)
 * - Limit (20 bits: 16 in limit_low, 4 in granularity)
 * - Access flags (present, privilege, type, direction, rw)
 * - Granularity flags (granule size, mode)
 */
static void _gdt_set_entry(int index, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t granularity) {
    struct gdt_entry *entry = &gdt_entries[index];
    
    /* Split base address into three fields */
    entry->base_low  = (base >> 0) & 0xFFFF;
    entry->base_mid  = (base >> 16) & 0xFF;
    entry->base_high = (base >> 24) & 0xFF;
    
    /* Split limit into two fields */
    entry->limit_low  = (limit >> 0) & 0xFFFF;
    entry->granularity = (granularity & 0xF0) | ((limit >> 16) & 0x0F);
    
    /* Set access and granularity flags */
    entry->access = access;
}

void gdt_init(void) {
    serial_puts("[GDT] Initializing Global Descriptor Table\n");
    
    /* Entry 0: NULL (required by x86) */
    _gdt_set_entry(GDT_INDEX_NULL, 0, 0, 0, 0);
    
    /* Entry 1: Kernel code segment (ring 0)
     *
     * 4GB segment (limit 0xFFFFF with 4KB granule)
     * Readable (for fetching constants), executable
     */
    _gdt_set_entry(GDT_INDEX_KERN_CODE,
                   0,                           /* Base: 0x00000000 */
                   0xFFFFF,                     /* Limit: 4GB */
                   GDT_ACCESS_PRESENT |         /* P: Present */
                   0x00 |                       /* DPL: Ring 0 */
                   GDT_ACCESS_CODE_DATA |       /* S: Code/data */
                   GDT_ACCESS_EXECUTE |         /* E: Executable */
                   GDT_ACCESS_DIRECTION |       /* C: Conforming */
                   GDT_ACCESS_RW,               /* R: Readable */
                   GDT_GRANULARITY_PAGE |       /* G: 4KB granule */
                   GDT_GRANULARITY_SIZE);       /* D: 32-bit */
    
    /* Entry 2: Kernel data segment (ring 0)
     *
     * 4GB segment, readable/writable
     * Used for kernel stack, heap, and all kernel memory
     */
    _gdt_set_entry(GDT_INDEX_KERN_DATA,
                   0,                           /* Base: 0x00000000 */
                   0xFFFFF,                     /* Limit: 4GB */
                   GDT_ACCESS_PRESENT |         /* P: Present */
                   0x00 |                       /* DPL: Ring 0 */
                   GDT_ACCESS_CODE_DATA |       /* S: Code/data */
                   0x00 |                       /* E: Not executable */
                   0x00 |                       /* DC: Expand up */
                   GDT_ACCESS_RW,               /* W: Writable */
                   GDT_GRANULARITY_PAGE |       /* G: 4KB granule */
                   GDT_GRANULARITY_SIZE);       /* D: 32-bit */
    
    /* Entry 3: User code segment (ring 3)
     *
     * 4GB segment, readable, executable
     * Used for user-space programs (ring 3)
     */
    _gdt_set_entry(GDT_INDEX_USER_CODE,
                   0,                           /* Base: 0x00000000 */
                   0xFFFFF,                     /* Limit: 4GB */
                   GDT_ACCESS_PRESENT |         /* P: Present */
                   0x60 |                       /* DPL: Ring 3 */
                   GDT_ACCESS_CODE_DATA |       /* S: Code/data */
                   GDT_ACCESS_EXECUTE |         /* E: Executable */
                   GDT_ACCESS_DIRECTION |       /* C: Conforming */
                   GDT_ACCESS_RW,               /* R: Readable */
                   GDT_GRANULARITY_PAGE |       /* G: 4KB granule */
                   GDT_GRANULARITY_SIZE);       /* D: 32-bit */
    
    /* Entry 4: User data segment (ring 3)
     *
     * 4GB segment, readable/writable
     * Used for user stack and data
     */
    _gdt_set_entry(GDT_INDEX_USER_DATA,
                   0,                           /* Base: 0x00000000 */
                   0xFFFFF,                     /* Limit: 4GB */
                   GDT_ACCESS_PRESENT |         /* P: Present */
                   0x60 |                       /* DPL: Ring 3 */
                   GDT_ACCESS_CODE_DATA |       /* S: Code/data */
                   0x00 |                       /* E: Not executable */
                   0x00 |                       /* DC: Expand up */
                   GDT_ACCESS_RW,               /* W: Writable */
                   GDT_GRANULARITY_PAGE |       /* G: 4KB granule */
                   GDT_GRANULARITY_SIZE);       /* D: 32-bit */
    
    /* Entry 5: TSS (Task State Segment)
     *
     * System segment (not code/data), present
     * DPL = 0 (only kernel can load via LTR)
     * Limit will be set when TSS is created
     *
     * This is a special entry for task switching and privilege transitions.
     * Base address will be set by tss_init().
     */
    _gdt_set_entry(GDT_INDEX_TSS,
                   0,                           /* Base: set by tss_init() */
                   0,                           /* Limit: set by tss_init() */
                   GDT_ACCESS_PRESENT |         /* P: Present */
                   0x00 |                       /* DPL: Ring 0 */
                   0x09,                        /* S=0, Type=0x9 (TSS 32-bit) */
                   0);                          /* Flags: none */
    
    /* Set up GDT descriptor for LGDT instruction
     *
     * LGDT needs:
     * - Limit: size of GDT in bytes - 1
     * - Offset: linear address of first entry
     */
    gdt_descriptor.size = (GDT_SIZE * sizeof(struct gdt_entry)) - 1;
    gdt_descriptor.offset = (uint32_t)&gdt_entries[0];
    
    /* Load GDT via LGDT instruction */
    __asm__ volatile("lgdt (%0)" : : "r"(&gdt_descriptor));
    
    /* Reload data segments and stack with new kernel selector
     *
     * After loading GDT, we need to reload DS, ES, SS, FS, GS
     * with the new kernel data selector.
     * CS is reloaded via far jump (done in assembly bootstrap).
     */
    uint16_t sel = KERN_DATA_SEL;
    __asm__ volatile(
        "movw %0, %%ds\n"
        "movw %0, %%es\n"
        "movw %0, %%ss\n"
        "movw %0, %%fs\n"
        "movw %0, %%gs\n"
        : : "r"(sel)
    );
    
    serial_printf("[GDT] Initialized with %d entries\n", GDT_SIZE);
    serial_printf("[GDT] Kernel code selector: 0x%02x\n", KERN_CODE_SEL);
    serial_printf("[GDT] Kernel data selector: 0x%02x\n", KERN_DATA_SEL);
    serial_printf("[GDT] User code selector:   0x%02x\n", USER_CODE_SEL);
    serial_printf("[GDT] User data selector:   0x%02x\n", USER_DATA_SEL);
    serial_printf("[GDT] TSS selector:         0x%02x\n", TSS_SEL);
}

void gdt_set_tss_descriptor(uint32_t tss_addr) {
    /* TSS is a system segment, 104 bytes for 32-bit TSS */
    uint32_t tss_limit = sizeof(tss_t) - 1;
    
    _gdt_set_entry(GDT_INDEX_TSS,
                   tss_addr,                    /* Base: TSS address */
                   tss_limit,                   /* Limit: TSS size */
                   GDT_ACCESS_PRESENT |         /* P: Present */
                   0x00 |                       /* DPL: Ring 0 */
                   0x09,                        /* S=0, Type=0x9 (TSS available) */
                   0);                          /* Flags: none */
    
    serial_printf("[GDT] TSS descriptor set: addr=0x%08x, limit=%d\n",
                  tss_addr, tss_limit);
}

uint32_t gdt_get_size(void) {
    return GDT_SIZE * sizeof(struct gdt_entry);
}

uint32_t gdt_get_base(void) {
    return (uint32_t)&gdt_entries[0];
}
