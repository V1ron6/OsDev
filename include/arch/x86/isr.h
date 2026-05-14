#ifndef ARCH_X86_ISR_H
#define ARCH_X86_ISR_H

#include <stdint.h>

typedef struct isr_frame {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t int_no;
    uint32_t err_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} isr_frame_t;

void isr_handler(isr_frame_t *frame);

void isr0(void);
void isr6(void);
void isr13(void);

#endif
