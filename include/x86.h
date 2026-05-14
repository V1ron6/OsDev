#ifndef _BYTEBANDIT_X86_H
#define _BYTEBANDIT_X86_H

#include "types.h"

/* x86 segmentation and descriptor structures */

/* GDT Descriptor (8 bytes) */
struct gdt_descriptor {
    uint16_t size;
    uint32_t offset;
} __attribute__((packed));

/* GDT Entry (8 bytes) */
struct gdt_entry {
    uint16_t limit_low;         /* Segment limit (bits 0-15) */
    uint16_t base_low;          /* Base address (bits 0-15) */
    uint8_t  base_mid;          /* Base address (bits 16-23) */
    uint8_t  access;            /* Type and DPL flags */
    uint8_t  granularity;       /* Flags and limit high */
    uint8_t  base_high;         /* Base address (bits 24-31) */
} __attribute__((packed));

/* Access byte flags */
#define GDT_ACCESS_PRESENT      0x80    /* P: Descriptor present */
#define GDT_ACCESS_PRIVILEGE    0x60    /* DPL: Privilege level */
#define GDT_ACCESS_CODE_DATA    0x10    /* S: Code/data (not system) */
#define GDT_ACCESS_EXECUTE      0x08    /* E: Executable (code) */
#define GDT_ACCESS_DIRECTION    0x04    /* DC: Direction/conforming */
#define GDT_ACCESS_RW           0x02    /* RW: Read/write */
#define GDT_ACCESS_ACCESSED     0x01    /* A: Accessed */

/* Granularity byte flags */
#define GDT_GRANULARITY_LIMIT   0x0F    /* Limit high (bits 16-19) */
#define GDT_GRANULARITY_AVL     0x10    /* AVL: Available for software */
#define GDT_GRANULARITY_SIZE    0x40    /* D/B: 32-bit segment */
#define GDT_GRANULARITY_PAGE    0x80    /* G: 4KB page granularity */

/* IDT Gate Descriptor (8 bytes) */
struct idt_gate {
    uint16_t offset_low;        /* Handler offset (bits 0-15) */
    uint16_t selector;          /* Code segment selector */
    uint8_t  reserved;          /* Must be 0 */
    uint8_t  type_attr;         /* Gate type and attributes */
    uint16_t offset_high;       /* Handler offset (bits 16-31) */
} __attribute__((packed));

/* IDT Gate type attributes */
#define IDT_GATE_TASK           0x05    /* Task gate */
#define IDT_GATE_INTR_16        0x06    /* 16-bit interrupt gate */
#define IDT_GATE_TRAP_16        0x07    /* 16-bit trap gate */
#define IDT_GATE_INTR_32        0x0E    /* 32-bit interrupt gate */
#define IDT_GATE_TRAP_32        0x0F    /* 32-bit trap gate */
#define IDT_ATTR_PRESENT        0x80    /* P: Descriptor present */
#define IDT_ATTR_DPL_MASK       0x60    /* DPL: Privilege level */
#define IDT_ATTR_DPL_0          0x00    /* Ring 0 (kernel) */
#define IDT_ATTR_DPL_3          0x60    /* Ring 3 (user) */

/* Control Register bits */
#define CR0_PAGING_ENABLE       0x80000000
#define CR0_PROTECTION_ENABLE   0x00000001

/* Inline assembly helpers */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* CPU flags helpers */
static inline void disable_interrupts(void) {
    __asm__ volatile("cli");
}

static inline void enable_interrupts(void) {
    __asm__ volatile("sti");
}

static inline void halt(void) {
    __asm__ volatile("hlt");
}

static inline uint32_t read_cr0(void) {
    uint32_t val;
    __asm__ volatile("movl %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint32_t val) {
    __asm__ volatile("movl %0, %%cr0" : : "r"(val));
}

static inline uint32_t read_cr3(void) {
    uint32_t val;
    __asm__ volatile("movl %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint32_t val) {
    __asm__ volatile("movl %0, %%cr3" : : "r"(val));
}

static inline uint32_t read_cr2(void) {
    uint32_t val;
    __asm__ volatile("movl %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint32_t read_eip(void) {
    uint32_t val;
    __asm__ volatile("movl $., %0" : "=r"(val));
    return val;
}

static inline void invlpg(uint32_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr));
}

#endif /* _BYTEBANDIT_X86_H */
