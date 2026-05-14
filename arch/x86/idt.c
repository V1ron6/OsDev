#include "arch/x86/idt.h"
#include "arch/x86/isr.h"
#include "libk/string.h"
#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_descriptor;

static void idt_set_gate(uint8_t index, uint32_t base, uint16_t selector, uint8_t flags)
{
    idt[index].base_low = (uint16_t)(base & 0xFFFF);
    idt[index].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[index].selector = selector;
    idt[index].zero = 0;
    idt[index].flags = flags;
}

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));

    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);

    idt_descriptor.limit = (uint16_t)(sizeof(idt) - 1);
    idt_descriptor.base = (uint32_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idt_descriptor));
}
