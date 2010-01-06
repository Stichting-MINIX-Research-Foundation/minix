
#ifndef _VM_H
#define _VM_H 1

/* Pseudo error codes */
#define VMSUSPEND       (-996)
#define EFAULT_SRC	(-995)
#define EFAULT_DST	(-994)

#define FIXLINMSG(prp) { prp->p_delivermsg_lin = umap_local(prp, D, prp->p_delivermsg_vir, sizeof(message)); }

#define PHYS_COPY_CATCH(src, dst, size, a) {	\
	vmassert(intr_disabled());		\
	catch_pagefaults++;			\
	a = phys_copy(src, dst, size);		\
	catch_pagefaults--;			\
	}

#endif


