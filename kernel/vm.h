
#ifndef _VM_H
#define _VM_H 1

#define CHECKRANGE_OR_SUSPEND(pr, start, length, wr)  { int mr; \
	if(vm_running && (mr=vm_checkrange(proc_addr(who_p), pr, start, length, wr, 0)) != OK) { \
		return mr;					 \
	} }

#define CHECKRANGE(pr, start, length, wr)   \
	vm_checkrange(proc_addr(who_p), pr, start, length, wr, 1)

/* Pseudo error code indicating a process request has to be
 * restarted after an OK from VM.
 */
#define VMSUSPEND       -996

#endif


