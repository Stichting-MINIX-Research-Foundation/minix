#ifndef __I386_WATCHDOG_H__
#define __I386_WATCHDOG_H__

#include "kernel/kernel.h"

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h> // For fixed-width types like k_uint16_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


struct nmi_frame {
	reg_t	eax;
	reg_t	ecx;
	reg_t	edx;
	reg_t	ebx;
	reg_t	esp;
	reg_t	ebp;
	reg_t	esi;
	reg_t	edi;
	u16_t	gs; // u16_t might be undefined
	u16_t	fs; // u16_t might be undefined
	u16_t	es; // u16_t might be undefined
	u16_t	ds; // u16_t might be undefined
	reg_t	pc;	/* arch independent name for program counter */
	reg_t	cs;
	reg_t	eflags;
};

int i386_watchdog_start(void);

#define nmi_in_kernel(f)	((f)->cs == KERN_CS_SELECTOR)

#endif /* __I386_WATCHDOG_H__ */
