#ifndef __CLOCK_X86_H__
#define __CLOCK_X86_H__

#include "../apic_asm.h" // Kept (relative include to local arch header)

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


int init_8253A_timer(unsigned freq);
void stop_8253A_timer(void);
void arch_timer_int_handler(void);

#endif /* __CLOCK_X86_H__ */
