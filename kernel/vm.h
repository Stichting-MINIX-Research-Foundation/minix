
#ifndef _VM_H
#define _VM_H 1

#define CHECKRANGE_OR_SUSPEND(pr, start, length, wr)  { int mr; \
	if(vm_running && (mr=vm_checkrange(proc_addr(who_p), pr, start, length, wr, 0)) != OK) { \
		return mr;					 \
	} }

#define CHECKRANGE(pr, start, length, wr)   \
	vm_checkrange(proc_addr(who_p), pr, start, length, wr, 1)

/* Pseudo error codes */
#define VMSUSPEND       -996
#define EFAULT_SRC	-995
#define EFAULT_DST	-994

#endif


