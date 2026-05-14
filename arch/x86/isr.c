#include "arch/x86/isr.h"
#include "kernel/console.h"
#include "kernel/panic.h"

void isr_handler(isr_frame_t *frame)
{
    const char *message = "Unknown exception";

    switch (frame->int_no) {
    case 0:
        message = "Divide-by-zero";
        break;
    case 6:
        message = "Invalid opcode";
        break;
    case 13:
        message = "General protection fault";
        break;
    default:
        break;
    }

    console_write("\nCPU EXCEPTION: ");
    console_write(message);
    console_write("\nVector: ");
    console_write_hex(frame->int_no);
    console_write(" Error: ");
    console_write_hex(frame->err_code);
    console_write("\nEIP: ");
    console_write_hex(frame->eip);
    console_write(" CS: ");
    console_write_hex(frame->cs);
    console_write(" EFLAGS: ");
    console_write_hex(frame->eflags);
    console_write("\n");

    PANIC("Unhandled CPU exception");
}
