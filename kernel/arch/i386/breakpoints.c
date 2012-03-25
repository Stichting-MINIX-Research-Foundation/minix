#include "kernel/kernel.h"
#include "arch_proto.h"

#include "debugreg.h"

int breakpoint_set(phys_bytes linaddr, int bp, const int flags)
{
	unsigned long dr7, dr7flags;
	
	if (bp >= BREAKPOINT_COUNT)
		return EINVAL;
	
	/* convert flags */
	dr7flags = 0;
	switch (flags & BREAKPOINT_FLAG_RW_MASK) {
		case BREAKPOINT_FLAG_RW_EXEC:  dr7flags |= DR7_RW_EXEC(bp);  break;
		case BREAKPOINT_FLAG_RW_WRITE: dr7flags |= DR7_RW_WRITE(bp); break;
		case BREAKPOINT_FLAG_RW_RW:    dr7flags |= DR7_RW_RW(bp);    break;
		default: return EINVAL;			
	}
	switch (flags & BREAKPOINT_FLAG_LEN_MASK) {
		case BREAKPOINT_FLAG_LEN_1: dr7flags |= DR7_LN_1(bp); break;
		case BREAKPOINT_FLAG_LEN_2: dr7flags |= DR7_LN_2(bp); break;
		case BREAKPOINT_FLAG_LEN_4: dr7flags |= DR7_LN_4(bp); break;
		default: return EINVAL;	
	}
	switch (flags & BREAKPOINT_FLAG_MODE_MASK) {
		case BREAKPOINT_FLAG_MODE_OFF: break;
		case BREAKPOINT_FLAG_MODE_LOCAL: dr7flags |= DR7_L(bp); break;
		case BREAKPOINT_FLAG_MODE_GLOBAL: dr7flags |= DR7_G(bp); break;
		default: return EINVAL;	
	}
	
	/* disable breakpoint before setting address */
	dr7 = st_dr7();
	dr7 &= ~(DR7_L(bp) | DR7_G(bp) | DR7_RW_MASK(bp) | DR7_LN_MASK(bp));
	ld_dr7(dr7);

	/* need to set new breakpoint? */	
	if ((flags & BREAKPOINT_FLAG_MODE_MASK) == BREAKPOINT_FLAG_MODE_OFF)
		return 0;
			
	/* set breakpoint address */
	switch (bp) {
		case 0: ld_dr0(linaddr); break;
		case 1: ld_dr1(linaddr); break;
		case 2: ld_dr2(linaddr); break;
		case 3: ld_dr3(linaddr); break;
		default: panic("%s:%d: invalid breakpoint index", __FILE__,  __LINE__);
	}
	
	/* set new flags */
	dr7 |= dr7flags;
	ld_dr7(dr7);
	return 0;
}

