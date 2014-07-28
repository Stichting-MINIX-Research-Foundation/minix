
#ifndef _ARM_TYPES_H
#define _ARM_TYPES_H

#include <minix/sys_config.h>
#include <machine/stackframe.h>
#include <sys/cdefs.h>

typedef struct segframe {
	reg_t	p_ttbr;		/* page table root */
	u32_t	*p_ttbr_v;
	char	*fpu_state;
} segframe_t;

struct cpu_info {
	u32_t	arch;
	u32_t	implementer;
	u32_t	part;
	u32_t	variant;
	u32_t	freq;		/* in MHz */
	u32_t	revision;
};

typedef u32_t atomic_t;	/* access to an aligned 32bit value is atomic on ARM */

#endif /* #ifndef _ARM_TYPES_H */

