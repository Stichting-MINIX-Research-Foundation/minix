#ifndef __GLO_X86_H__
#define __GLO_X86_H__

#include "kernel/kernel.h"
#include "arch_proto.h"

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


EXTERN int cpu_has_tsc;	/* signal whether this cpu has time stamp register. This
			   feature was introduced by Pentium */

EXTERN struct tss_s tss[CONFIG_MAX_CPUS];

EXTERN int i386_paging_enabled;

#endif /* __GLO_X86_H__ */
